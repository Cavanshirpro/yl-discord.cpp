// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "network_internal.hpp"
#include <algorithm>
#include <random>

namespace yl::discord::detail {
namespace {
double jitter() {
    thread_local std::mt19937 random{std::random_device{}()};
    return std::uniform_real_distribution<double>{0, 1}(random);
}
bool fatal_close(int code) {
    return code == 4004 || code == 4010 || code == 4011 || code == 4012 || code == 4013 || code == 4014;
}
Snowflake id(const Json& data) {
    if (!data.is_string()) throw ProtocolError("Expected a Discord ID string");
    return Snowflake{data.get<std::string>()};
}
std::string gateway_address(const std::string& url) {
    validate_url(url, true);
    const auto base = url.substr(0, url.find('?'));
    return base + (base.ends_with('/') ? "" : "/") + "?v=10&encoding=json";
}
}
User parse_user(const Json& data) {
    User user;
    user.id = id(data.at("id"));
    user.name = data.value("username", std::string{});
    user.bot = data.value("bot", false);
    return user;
}
Message parse_message(const Json& data) {
    Message message;
    message.id = id(data.at("id"));
    message.channel_id = id(data.at("channel_id"));
    if (data.contains("guild_id") && !data["guild_id"].is_null()) message.guild_id = id(data["guild_id"]);
    message.author = parse_user(data.at("author"));
    message.content = data.value("content", std::string{});
    return message;
}
Connection::Connection(asio::any_io_executor executor, ConnectionOptions options, Intents intents)
    : executor(executor), rest(std::make_shared<RestClient>(executor, options)), options(std::move(options)),
      intents(intents), delay(executor), heartbeat(executor), handshake(executor) {
    bool ws = false, wss = false;
    for (auto protocol = curl_version_info(CURLVERSION_NOW)->protocols; *protocol; ++protocol) {
        ws = ws || std::string_view(*protocol) == "ws";
        wss = wss || std::string_view(*protocol) == "wss";
    }
    if (!ws || !wss) throw TransportError("libcurl must provide both ws and wss protocols");
}
void Connection::stop() {
    if (stopped) return;
    stopped = true;
    delay.cancel(); heartbeat.cancel(); handshake.cancel(); rest->cancel();
    if (socket) {
        auto current = socket;
        asio::co_spawn(executor, current->close(), [current](std::exception_ptr) { current->abort(); });
    }
}
task<> Connection::run(std::string bot_token) {
    validate_token(bot_token);
    if (started) throw std::logic_error("A live Client can only be started once; construct a new Client to restart");
    started = true; token = std::move(bot_token);
    rest->authenticate(token);
    try {
        const auto response = co_await rest->request("GET", "/gateway/bot");
        const auto discovery = Json::parse(response.body);
        gateway_url = discovery.at("url").get<std::string>();
        validate_url(gateway_url, true);
        if (discovery.value("shards", 1) > 1) throw ProtocolError("This bot requires sharding, which is not supported in v0.1");
        identifies_left = discovery.at("session_start_limit").at("remaining").get<unsigned>();
    } catch (...) {
        if (stopped) co_return;
        throw;
    }
    unsigned failures = 0;
    while (!stopped) {
        established = false; hello_received = false; awaiting_ack = false; heartbeat_error = {};
        ++generation;
        const auto began = Clock::now();
        std::exception_ptr error;
        bool protocol_failure = false;
        try { co_await session(); }
        catch (const ProtocolError&) { error = std::current_exception(); protocol_failure = true; }
        catch (const Json::exception&) { error = std::make_exception_ptr(ProtocolError("Invalid Gateway payload")); protocol_failure = true; }
        catch (...) { error = std::current_exception(); }
        ++generation;
        heartbeat.cancel(); handshake.cancel();
        const int close = socket ? socket->close_code : 1006;
        if (socket) socket->abort();
        socket.reset();
        if (stopped) break;
        if (fatal_close(close)) throw ProtocolError("Fatal Gateway close code " + std::to_string(close));
        if (protocol_failure) std::rethrow_exception(error);
        if (close == 4007 || close == 4009 || close == 1000 || close == 1001) {
            session_id.clear(); resume_url.clear(); sequence.reset();
        }
        if (established && Clock::now() - began >= std::chrono::seconds{60}) failures = 0;
        if (failures++ >= options.max_reconnect_attempts) {
            if (error) std::rethrow_exception(error);
            throw TransportError("Gateway reconnect budget exhausted");
        }
        const auto factor = 1ULL << std::min(failures - 1, 5U);
        const auto backoff = std::min(std::chrono::milliseconds{30'000},
            std::chrono::milliseconds{static_cast<std::int64_t>(options.reconnect_delay.count() * factor)});
        delay.expires_after(backoff);
        asio::error_code ignored;
        co_await delay.async_wait(asio::redirect_error(asio::use_awaitable, ignored));
    }
}
task<> Connection::send_heartbeat() {
    Json value = sequence ? Json(*sequence) : Json(nullptr);
    awaiting_ack = true;
    const Json payload{{"op", 1}, {"d", value}};
    co_await socket->write(payload.dump());
}
task<> Connection::heartbeat_loop(std::shared_ptr<Connection> self, std::uint64_t epoch, std::chrono::milliseconds interval) {
    auto next = std::chrono::milliseconds{static_cast<std::int64_t>(interval.count() * jitter())};
    while (!self->stopped && self->generation == epoch) {
        self->heartbeat.expires_after(next);
        asio::error_code error;
        co_await self->heartbeat.async_wait(asio::redirect_error(asio::use_awaitable, error));
        if (error || self->stopped || self->generation != epoch) co_return;
        if (self->awaiting_ack) throw TransportError("Gateway heartbeat ACK deadline exceeded");
        co_await self->send_heartbeat();
        next = interval;
    }
}
task<> Connection::session() {
    socket = std::make_shared<WebSocket>(executor, options);
    const bool resume = !session_id.empty() && sequence.has_value() && !resume_url.empty();
    co_await socket->connect(gateway_address(resume ? resume_url : gateway_url));
    const auto epoch = generation;
    std::weak_ptr<Connection> weak = shared_from_this();
    handshake.expires_after(options.connect_timeout);
    handshake.async_wait([weak, epoch](asio::error_code error) {
        if (!error) if (auto self = weak.lock(); self && self->generation == epoch && !self->established) {
            self->heartbeat_error = std::make_exception_ptr(TransportError("Gateway handshake deadline exceeded"));
            self->socket->abort();
        }
    });
    for (;;) {
        std::string text;
        try { text = co_await socket->read(); }
        catch (...) { if (heartbeat_error) std::rethrow_exception(heartbeat_error); throw; }
        auto packet = Json::parse(text);
        const int op = packet.at("op").get<int>();
        static const Json null_data;
        const auto& data = packet.contains("d") ? packet["d"] : null_data;
        if (op == 10) {
            if (hello_received) throw ProtocolError("Duplicate Gateway Hello");
            hello_received = true;
            const auto count = data.at("heartbeat_interval").get<std::int64_t>();
            if (count <= 0 || count > 300'000) throw ProtocolError("Invalid Gateway heartbeat interval");
            auto self = shared_from_this();
            asio::co_spawn(executor, heartbeat_loop(self, epoch, std::chrono::milliseconds{count}), [self, epoch](std::exception_ptr error) {
                if (error && !self->stopped && self->generation == epoch) {
                    self->heartbeat_error = error;
                    if (self->socket) self->socket->abort();
                }
            });
            if (resume) {
                const Json payload{{"op", 6}, {"d", {{"token", token}, {"session_id", session_id}, {"seq", *sequence}}}};
                co_await socket->write(payload.dump());
            } else {
                if (identifies_left == 0) throw ProtocolError("Gateway session start limit exhausted");
                --identifies_left;
                const Json payload{{"op", 2}, {"d", {{"token", token}, {"intents", intents.value()},
                    {"properties", {{"os", "cpp"}, {"browser", "yl-discord.cpp"}, {"device", "yl-discord.cpp"}}}}}};
                co_await socket->write(payload.dump());
            }
        } else if (op == 11) {
            awaiting_ack = false;
        } else if (op == 1) {
            co_await send_heartbeat();
        } else if (op == 7) {
            throw TransportError("Gateway requested reconnect");
        } else if (op == 9) {
            if (!data.is_boolean()) throw ProtocolError("Invalid Gateway session response");
            if (!data.get<bool>()) { session_id.clear(); sequence.reset(); resume_url.clear(); }
            // Invalid-session recovery has Discord's recommended randomized delay.
            delay.expires_after(std::chrono::milliseconds{1000 + static_cast<int>(jitter() * 4000)});
            asio::error_code ignored;
            co_await delay.async_wait(asio::redirect_error(asio::use_awaitable, ignored));
            throw TransportError("Gateway invalidated session");
        } else if (op == 0) {
            if (!hello_received) throw ProtocolError("Gateway dispatch arrived before Hello");
            if (!packet.at("s").is_number_integer() ||
                (packet.at("s").is_number_integer() && !packet.at("s").is_number_unsigned() && packet.at("s").get<std::int64_t>() < 0))
                throw ProtocolError("Invalid Gateway sequence number");
            const auto next_sequence = packet.at("s").get<std::uint64_t>();
            if (sequence && next_sequence <= *sequence) continue;
            sequence = next_sequence;
            const auto name = packet.at("t").get<std::string>();
            raw(RawEvent{name, text, sequence});
            if (name == "READY") {
                session_id = data.at("session_id").get<std::string>();
                resume_url = data.at("resume_gateway_url").get<std::string>();
                validate_url(resume_url, true);
                if (session_id.empty()) throw ProtocolError("Empty Gateway session ID");
                established = true; handshake.cancel();
                ready(Ready{parse_user(data.at("user"))});
            } else if (name == "RESUMED") {
                established = true; handshake.cancel();
            } else if (name == "MESSAGE_CREATE") {
                message(parse_message(data));
            }
        }
    }
}
} // namespace yl::discord::detail
