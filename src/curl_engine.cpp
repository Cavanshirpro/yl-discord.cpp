// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include "network_internal.hpp"
#include "upload_internal.hpp"
#include <yl/discord/version.hpp>
#include <algorithm>
#include <array>
#include <cctype>

namespace yl::discord::detail {
namespace {
void initialize_curl() {
    static const bool ready = [] {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) throw TransportError("libcurl initialization failed");
        const auto* info = curl_version_info(CURLVERSION_NOW);
        if (!(info->features & CURL_VERSION_ASYNCHDNS)) throw TransportError("libcurl requires asynchronous DNS support");
        return true;
    }();
    (void)ready;
}
std::size_t body_callback(char* data, std::size_t size, std::size_t count, void* user) noexcept {
    auto& transfer = *static_cast<Transfer*>(user);
    const auto bytes = size * count;
    if (bytes > transfer.limit - transfer.response.body.size()) return 0;
    try { transfer.response.body.append(data, bytes); return bytes; } catch (...) { return 0; }
}
std::size_t header_callback(char* data, std::size_t size, std::size_t count, void* user) noexcept {
    const auto bytes = size * count;
    try {
        auto& headers = static_cast<Transfer*>(user)->response.headers;
        std::string line(data, bytes);
        if (line.starts_with("HTTP/")) headers.clear();
        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            auto key = line.substr(0, colon);
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            auto value = line.substr(colon + 1);
            const auto first = value.find_first_not_of(" \t\r\n");
            headers[key] = first == std::string::npos ? "" : value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
        }
        return bytes;
    } catch (...) { return 0; }
}
}

void validate_token(const std::string& token) {
    if (token.empty() || token.size() > 4096 || std::any_of(token.begin(), token.end(), [](unsigned char c) { return std::isspace(c) || c < 32 || c == 127; }))
        throw std::invalid_argument("Expected a raw bot token without whitespace");
}

void validate_url(const std::string& url, bool websocket) {
    CURLU* parsed = curl_url();
    if (!parsed) throw std::bad_alloc();
    std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> guard(parsed, curl_url_cleanup);
    if (curl_url_set(parsed, CURLUPART_URL, url.c_str(), CURLU_NON_SUPPORT_SCHEME) != CURLUE_OK || url.find_first_of("\r\n#") != std::string::npos)
        throw std::invalid_argument("Invalid connection URL");
    const auto part = [&](CURLUPart field) {
        char* value = nullptr;
        if (curl_url_get(parsed, field, &value, 0) != CURLUE_OK) return std::string{};
        std::string result(value); curl_free(value); return result;
    };
    const auto scheme = part(CURLUPART_SCHEME);
    const auto host = part(CURLUPART_HOST);
    const bool loopback = host == "127.0.0.1" || host == "[::1]" || host == "localhost";
    if (!part(CURLUPART_USER).empty() || !part(CURLUPART_PASSWORD).empty() ||
        (scheme != (websocket ? "wss" : "https") && !(loopback && scheme == (websocket ? "ws" : "http"))))
        throw std::invalid_argument("TLS is required except for local loopback fixtures");
}

task<AsyncMutex::Guard> AsyncMutex::lock() {
    auto executor = co_await asio::this_coro::executor;
    while (locked_) {
        auto timer = std::make_shared<asio::steady_timer>(executor, Clock::time_point::max());
        waiters_.push_back(timer);
        asio::error_code ignored;
        co_await timer->async_wait(asio::redirect_error(asio::use_awaitable, ignored));
        const auto cancellation = co_await asio::this_coro::cancellation_state;
        if (cancellation.cancelled() != asio::cancellation_type::none) cancelled();
    }
    locked_ = true;
    co_return Guard{this};
}
void AsyncMutex::unlock() {
    locked_ = false;
    while (!waiters_.empty()) {
        auto next = waiters_.front().lock(); waiters_.pop_front();
        if (next) { next->expires_at(Clock::now()); break; }
    }
}

Transfer::Transfer(asio::any_io_executor executor, const ConnectionOptions& options)
    : limit(options.max_response_bytes), completion(executor, Clock::time_point::max()) {
    initialize_curl();
    easy = curl_easy_init();
    if (!easy) throw std::bad_alloc();
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(options.connect_timeout.count()));
    curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, static_cast<long>(options.request_timeout.count()));
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "DiscordBot (https://github.com/Cavanshirpro/yl-discord.cpp, " YL_DISCORD_VERSION ")");
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, body_callback);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");
}
Transfer::~Transfer() {
    if (auto owner = engine.lock()) owner->detach(easy);
    upload.reset();
    if (easy) curl_easy_cleanup(easy);
    curl_slist_free_all(headers);
}
CurlEngine::CurlEngine(asio::any_io_executor executor) : executor(executor), tick(executor) {
    initialize_curl(); multi = curl_multi_init();
    if (!multi) throw std::bad_alloc();
}
CurlEngine::~CurlEngine() {
    for (auto* easy : attached) curl_multi_remove_handle(multi, easy);
    curl_multi_cleanup(multi);
}
void CurlEngine::detach(CURL* handle) {
    if (attached.erase(handle)) curl_multi_remove_handle(multi, handle);
    pending.erase(handle);
}
void CurlEngine::cancel() {
    stopped = true;
    auto transfers = std::move(pending);
    for (auto& [handle, transfer] : transfers) {
        if (attached.erase(handle)) curl_multi_remove_handle(multi, handle);
        transfer->error = std::make_exception_ptr(asio::system_error(asio::error::operation_aborted));
        transfer->done = true; transfer->completion.expires_at(Clock::now());
    }
    tick.cancel();
}
task<std::shared_ptr<Transfer>> CurlEngine::perform(std::shared_ptr<Transfer> transfer) {
    if (stopped) cancelled();
    auto self = shared_from_this();
    transfer->engine = self;
    if (curl_multi_add_handle(multi, transfer->easy) != CURLM_OK) throw TransportError("Could not schedule HTTP transfer");
    attached.insert(transfer->easy); pending.emplace(transfer->easy, transfer);
    if (!pumping) {
        pumping = true;
        asio::co_spawn(executor, pump(self), [self](std::exception_ptr error) {
            if (error) { self->pumping = false; self->cancel(); }
        });
    }
    try {
        if (!transfer->done) {
            asio::error_code ignored;
            co_await transfer->completion.async_wait(asio::redirect_error(asio::use_awaitable, ignored));
        }
        if (!transfer->done) cancelled();
        if (transfer->error) std::rethrow_exception(transfer->error);
    } catch (...) { detach(transfer->easy); throw; }
    co_return transfer;
}
task<> CurlEngine::pump(std::shared_ptr<CurlEngine> engine) {
    while (!engine->pending.empty()) {
        int active = 0;
        const auto code = curl_multi_perform(engine->multi, &active);
        if (code != CURLM_OK) throw TransportError("libcurl multi processing failed");
        int remaining = 0;
        while (auto* info = curl_multi_info_read(engine->multi, &remaining)) {
            if (info->msg != CURLMSG_DONE) continue;
            auto found = engine->pending.find(info->easy_handle);
            if (found == engine->pending.end()) continue;
            auto transfer = found->second;
            engine->pending.erase(found);
            const auto result = info->data.result;
            curl_easy_getinfo(transfer->easy, CURLINFO_RESPONSE_CODE, &transfer->response.status);
            if (result != CURLE_OK) transfer->error = std::make_exception_ptr(TransportError(std::string("Network transfer failed: ") + curl_easy_strerror(result)));
            if (!transfer->keep_connected || result != CURLE_OK) engine->detach(transfer->easy);
            transfer->done = true;
            transfer->completion.expires_at(Clock::now());
        }
        if (engine->pending.empty()) break;
        long suggested = 10;
        curl_multi_timeout(engine->multi, &suggested);
        if (suggested < 0) suggested = 10;
        engine->tick.expires_after(std::chrono::milliseconds{std::clamp(suggested, 1L, 10L)});
        asio::error_code error;
        co_await engine->tick.async_wait(asio::redirect_error(asio::use_awaitable, error));
        if (engine->stopped) break;
    }
    // Set before completion handlers can enqueue a new transfer.
    engine->pumping = false;
}

WebSocket::WebSocket(asio::any_io_executor executor, ConnectionOptions options)
    : options_(options), engine_(std::make_shared<CurlEngine>(executor)), watch_(executor) {}
WebSocket::~WebSocket() {
    if (watch_.is_open()) { asio::error_code ignored; (void)watch_.release(ignored); }
    transfer_.reset();
}
task<> WebSocket::connect(std::string url) {
    validate_url(url, true);
    auto transfer = std::make_shared<Transfer>(engine_->executor, options_);
    transfer->keep_connected = true;
    curl_easy_setopt(transfer->easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(transfer->easy, CURLOPT_CONNECT_ONLY, 2L);
    transfer_ = co_await engine_->perform(transfer);
    curl_socket_t native = CURL_SOCKET_BAD;
    curl_easy_getinfo(transfer_->easy, CURLINFO_ACTIVESOCKET, &native);
    if (native == CURL_SOCKET_BAD) throw TransportError("Gateway socket unavailable");
    watch_.assign(asio::ip::tcp::v4(), native);
}
void WebSocket::abort() {
    aborted_ = true;
    asio::error_code ignored; watch_.cancel(ignored);
    engine_->cancel();
}
task<std::string> WebSocket::read() {
    auto lifetime = shared_from_this();
    std::string message;
    std::string close_payload;
    std::array<char, 16 * 1024> buffer{};
    while (!aborted_) {
        std::size_t received = 0;
        const curl_ws_frame* meta = nullptr;
        const auto code = curl_ws_recv(transfer_->easy, buffer.data(), buffer.size(), &received, &meta);
        if (code == CURLE_AGAIN) { co_await watch_.async_wait(asio::ip::tcp::socket::wait_read, asio::use_awaitable); continue; }
        if (code != CURLE_OK) throw TransportError("Gateway connection closed");
        if (meta->flags & CURLWS_CLOSE) {
            close_payload.append(buffer.data(), received);
            if (meta->bytesleft != 0) continue;
            if (close_payload.size() >= 2) close_code = (static_cast<unsigned char>(close_payload[0]) << 8) | static_cast<unsigned char>(close_payload[1]);
            throw TransportError("Gateway closed with code " + std::to_string(close_code));
        }
        if (meta->flags & (CURLWS_PING | CURLWS_PONG)) continue;
        if (!(meta->flags & CURLWS_TEXT)) throw ProtocolError("Unexpected binary Gateway frame");
        if (received > options_.max_response_bytes - message.size()) throw ProtocolError("Gateway payload exceeds configured limit");
        message.append(buffer.data(), received);
        if (meta->bytesleft == 0 && !(meta->flags & CURLWS_CONT)) co_return message;
    }
    cancelled();
}
task<> WebSocket::write(std::string data, unsigned flags) {
    auto lifetime = shared_from_this();
    if ((flags & CURLWS_TEXT) && data.size() > 4096) throw ProtocolError("Outgoing Gateway payload exceeds 4096 bytes");
    auto guard = co_await writer_.lock();
    if (aborted_ || !transfer_) cancelled();
    auto deadline = std::make_shared<asio::steady_timer>(engine_->executor, options_.request_timeout);
    std::weak_ptr<WebSocket> weak = shared_from_this();
    deadline->async_wait([weak](asio::error_code error) { if (!error) if (auto socket = weak.lock()) socket->abort(); });
    try {
        std::size_t offset = 0;
        do {
            if (aborted_) cancelled();
            std::size_t sent = 0;
            const auto code = curl_ws_send(transfer_->easy, data.data() + offset, data.size() - offset, &sent, 0, flags);
            offset += sent;
            if (code != CURLE_OK && code != CURLE_AGAIN) throw TransportError("Gateway send failed");
            if (offset < data.size() || code == CURLE_AGAIN)
                co_await watch_.async_wait(asio::ip::tcp::socket::wait_write, asio::use_awaitable);
        } while (offset < data.size());
    } catch (...) { deadline->cancel(); throw; }
    deadline->cancel();
}
task<> WebSocket::close() {
    try { if (transfer_ && !aborted_) co_await write(std::string{"\x03\xe8", 2}, CURLWS_CLOSE); } catch (...) {}
    abort();
}
} // namespace yl::discord::detail
