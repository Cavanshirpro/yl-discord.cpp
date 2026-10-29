// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "network_internal.hpp"
#include "message_validation.hpp"
#include <algorithm>
#include <cmath>
#include <charconv>
#include <cstdlib>
#include <sstream>

namespace yl::discord {
namespace {
using namespace detail;
std::optional<double> seconds(std::string_view text) {
    if (text.empty()) return {};
    double value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
        std::isfinite(value) && value >= 0 && value <= 86'400) return value;
    return {};
}
std::chrono::milliseconds milliseconds(double value) {
    if (!std::isfinite(value) || value < 0 || value > 86'400) throw ProtocolError("Invalid rate-limit delay");
    return std::chrono::milliseconds{static_cast<std::int64_t>(std::ceil(value * 1000))};
}
std::pair<std::string, std::string> route_key(const std::string& method, const std::string& route) {
    std::istringstream input(route.substr(0, route.find('?')));
    std::vector<std::string> parts;
    std::string part;
    while (std::getline(input, part, '/')) if (!part.empty()) parts.push_back(part);
    std::string major;
    std::string normalized = method;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const bool numeric = std::all_of(parts[i].begin(), parts[i].end(), [](unsigned char c) { return std::isdigit(c) != 0; });
        if (i > 0 && (parts[i - 1] == "channels" || parts[i - 1] == "guilds" || parts[i - 1] == "webhooks")) {
            major = parts[i - 1] + ":" + parts[i];
            if (parts[i - 1] == "webhooks" && i + 1 < parts.size()) major += ":" + parts[i + 1];
        }
        const bool emoji = i > 0 && parts[i - 1] == "reactions";
        normalized += "/" + (emoji ? std::string(":emoji") : numeric ? std::string(":id") : parts[i]);
    }
    return {normalized + "|" + major, major};
}
int discord_code(const std::string& body) {
    const auto json = Json::parse(body, nullptr, false);
    if (!json.is_discarded() && json.contains("code") && json["code"].is_number_integer()) return json["code"].get<int>();
    return 0;
}
}

HttpError::HttpError(long status, int discord_code)
    : std::runtime_error("Discord HTTP " + std::to_string(status) + " (code " + std::to_string(discord_code) + ")"), status_(status), code_(discord_code) {}

struct RestClient::State {
    struct Bucket { detail::AsyncMutex mutex; detail::Clock::time_point until{}; };
    asio::any_io_executor executor;
    ConnectionOptions options;
    std::shared_ptr<detail::CurlEngine> engine;
    std::string token;
    std::unordered_map<std::string, std::shared_ptr<Bucket>> routes;
    std::unordered_map<std::string, std::shared_ptr<Bucket>> buckets;
    detail::Clock::time_point global_until{};
    std::vector<std::weak_ptr<asio::steady_timer>> waits;
    bool stopped = false;
    bool auth_failed = false;
    bool used = false;
    State(asio::any_io_executor executor, ConnectionOptions options)
        : executor(executor), options(std::move(options)), engine(std::make_shared<detail::CurlEngine>(executor)) {
        detail::validate_url(this->options.rest_url, false);
        if (this->options.rest_url.find('?') != std::string::npos) throw std::invalid_argument("REST base URL cannot contain a query");
        while (this->options.rest_url.ends_with('/')) this->options.rest_url.pop_back();
        if (this->options.request_timeout.count() <= 0 || this->options.request_timeout.count() > 86'400'000 ||
            this->options.connect_timeout.count() <= 0 || this->options.connect_timeout.count() > 86'400'000 ||
            this->options.max_response_bytes == 0 || this->options.reconnect_delay.count() < 0 ||
            this->options.reconnect_delay.count() > 30'000)
            throw std::invalid_argument("Invalid connection limits/timeouts");
    }
    task<> wait_until(detail::Clock::time_point until) {
        if (stopped) detail::cancelled();
        if (until <= detail::Clock::now()) co_return;
        auto timer = std::make_shared<asio::steady_timer>(executor, until);
        std::erase_if(waits, [](const auto& value) { return value.expired(); });
        waits.push_back(timer);
        co_await timer->async_wait(asio::use_awaitable);
        if (stopped) detail::cancelled();
    }
};

RestClient::RestClient(asio::any_io_executor executor, ConnectionOptions options)
    : state_(std::make_shared<State>(executor, std::move(options))) {}
RestClient::~RestClient() = default;
void RestClient::authenticate(std::string token) {
    detail::validate_token(token);
    if (state_->used) throw std::logic_error("Cannot change authentication after starting REST requests");
    state_->token = std::move(token);
}
void RestClient::cancel() {
    state_->stopped = true;
    state_->engine->cancel();
    for (auto& weak : state_->waits) if (auto timer = weak.lock()) timer->cancel();
}
task<HttpResponse> RestClient::request(std::string method, std::string route, std::string body) {
    return request_impl(state_, std::move(method), std::move(route), std::move(body));
}
task<HttpResponse> RestClient::request_impl(std::shared_ptr<State> state, std::string method, std::string route, std::string body) {
    if (state->token.empty()) throw std::logic_error("REST client is not authenticated");
    if (method != "GET" && method != "POST" && method != "PUT" && method != "PATCH" && method != "DELETE")
        throw std::invalid_argument("Unsupported HTTP method");
    if (route.empty() || route.front() != '/' || route.find('#') != std::string::npos ||
        std::any_of(route.begin(), route.end(), [](unsigned char c) { return c <= 32 || c == 127; }))
        throw std::invalid_argument("Expected an API-relative HTTP route");
    state->used = true;
    const auto [key, major] = route_key(method, route);
    unsigned retries = 0;
    for (;;) {
        if (state->stopped) detail::cancelled();
        if (state->auth_failed) throw HttpError(401, 0);
        auto& slot = state->routes[key];
        if (!slot) slot = std::make_shared<State::Bucket>();
        auto bucket = slot;
        auto guard = co_await bucket->mutex.lock();
        if (state->stopped) detail::cancelled();
        if (state->auth_failed) throw HttpError(401, 0);
        if (state->routes[key] != bucket) continue;
        while (std::max(bucket->until, state->global_until) > detail::Clock::now())
            co_await state->wait_until(std::max(bucket->until, state->global_until));
        auto transfer = std::make_shared<detail::Transfer>(state->executor, state->options);
        const auto url = state->options.rest_url + route;
        transfer->body = body;
        curl_easy_setopt(transfer->easy, CURLOPT_URL, url.c_str());
        curl_easy_setopt(transfer->easy, CURLOPT_CUSTOMREQUEST, method.c_str());
        const auto header = "Authorization: Bot " + state->token;
        transfer->headers = curl_slist_append(transfer->headers, header.c_str());
        transfer->headers = curl_slist_append(transfer->headers, "Content-Type: application/json");
        curl_easy_setopt(transfer->easy, CURLOPT_HTTPHEADER, transfer->headers);
        if (!body.empty()) {
            curl_easy_setopt(transfer->easy, CURLOPT_POSTFIELDS, transfer->body.data());
            curl_easy_setopt(transfer->easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(transfer->body.size()));
        }
        transfer = co_await state->engine->perform(transfer);
        auto response = std::move(transfer->response);
        const auto hash = response.headers.find("x-ratelimit-bucket");
        auto learned = bucket;
        if (hash != response.headers.end()) {
            auto& shared = state->buckets[hash->second + "|" + major];
            if (!shared) shared = std::make_shared<State::Bucket>();
            learned = shared;
            learned->until = std::max(learned->until, bucket->until);
            state->routes[key] = learned;
        }
        if (response.headers.contains("x-ratelimit-remaining") && response.headers.at("x-ratelimit-remaining") == "0") {
            if (auto reset = response.headers.find("x-ratelimit-reset-after"); reset != response.headers.end()) {
                if (auto delay = seconds(reset->second)) learned->until = detail::Clock::now() + milliseconds(*delay);
            }
        }
        if (response.status == 429) {
            const auto json = detail::Json::parse(response.body, nullptr, false);
            std::optional<double> retry;
            bool global = response.headers.contains("x-ratelimit-global") ||
                (response.headers.contains("x-ratelimit-scope") && response.headers.at("x-ratelimit-scope") == "global");
            if (json.is_object()) {
                if (json.contains("retry_after") && json["retry_after"].is_number()) retry = json["retry_after"].get<double>();
                if (json.contains("global") && json["global"].is_boolean()) global = global || json["global"].get<bool>();
            }
            if (!retry && response.headers.contains("retry-after")) retry = seconds(response.headers.at("retry-after"));
            if (!retry) throw ProtocolError("429 response has no valid retry delay");
            const auto until = detail::Clock::now() + std::max(milliseconds(*retry), std::chrono::milliseconds{1});
            if (global) state->global_until = std::max(state->global_until, until);
            learned->until = std::max(learned->until, until);
            if (retries++ >= state->options.max_rate_limit_retries) throw HttpError(429, discord_code(response.body));
            continue;
        }
        if (response.status == 401) state->auth_failed = true;
        if (response.status < 200 || response.status >= 300) throw HttpError(response.status, discord_code(response.body));
        co_return response;
    }
}

task<Message> RestClient::send_message(MessageRequest request) { return send_impl(state_, std::move(request)); }
task<Message> RestClient::send_impl(std::shared_ptr<State> state, MessageRequest request) {
    detail::validate_message_parts(request.content, request.embeds, request.allowed_mentions);
    auto response = co_await request_impl(state, "POST", "/channels/" + request.channel_id.str() + "/messages", detail::message_payload(request));
    co_return detail::parse_message_response(response.body);
}

task<Message> RestClient::fetch_message(Snowflake channel, Snowflake message) {
    return message_impl(state_, "GET", channel, message, {});
}
task<Message> RestClient::edit_message(Snowflake channel, Snowflake message, std::string content) {
    MessageEditOptions options;
    options.content = std::move(content);
    return edit_message(channel, message, std::move(options));
}
task<Message> RestClient::edit_message(Snowflake channel, Snowflake message, MessageEditOptions options) {
    return message_impl(state_, "PATCH", channel, message, std::move(options));
}
task<Message> RestClient::message_impl(std::shared_ptr<State> state, std::string method, Snowflake channel, Snowflake message, MessageEditOptions options) {
    if (method == "PATCH") validate_message_edit(options);
    const auto body = method == "PATCH" ? detail::edit_message_payload(options) : std::string{};
    auto response = co_await request_impl(state, std::move(method), "/channels/" + channel.str() + "/messages/" + message.str(), body);
    co_return detail::parse_message_response(response.body);
}
task<> RestClient::delete_message(Snowflake channel, Snowflake message) { return delete_impl(state_, channel, message); }
task<> RestClient::delete_impl(std::shared_ptr<State> state, Snowflake channel, Snowflake message) {
    co_await request_impl(state, "DELETE", "/channels/" + channel.str() + "/messages/" + message.str(), {});
}
task<> RestClient::add_reaction(Snowflake channel, Snowflake message, std::string emoji) {
    return reaction_impl(state_, channel, message, std::move(emoji), false);
}
task<> RestClient::remove_reaction(Snowflake channel, Snowflake message, std::string emoji) {
    return reaction_impl(state_, channel, message, std::move(emoji), true);
}
task<> RestClient::reaction_impl(std::shared_ptr<State> state, Snowflake channel, Snowflake message, std::string emoji, bool remove) {
    const auto route = "/channels/" + channel.str() + "/messages/" + message.str() + "/reactions/" + detail::reaction_path(emoji) + "/@me";
    co_await request_impl(state, remove ? "DELETE" : "PUT", route, {});
}
std::string token_from_env(std::string_view name) {
    std::string variable(name);
    if (variable.empty() || variable.find('\0') != std::string::npos) throw std::invalid_argument("Invalid token environment variable name");
    const char* value = std::getenv(variable.c_str());
    if (!value || !*value) throw std::runtime_error("Bot token environment variable is not set");
    std::string token(value); detail::validate_token(token); return token;
}
} // namespace yl::discord
