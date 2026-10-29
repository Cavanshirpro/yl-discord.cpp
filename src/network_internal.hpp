// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once
#include <yl/discord/network.hpp>
#include <asio/co_spawn.hpp>
#include <asio/redirect_error.hpp>
#include <asio/ip/tcp.hpp>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <deque>
#include <functional>
#include <set>
#include <unordered_map>

namespace yl::discord::detail {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

[[noreturn]] inline void cancelled() { throw asio::system_error(asio::error::operation_aborted); }
void validate_token(const std::string& token);
void validate_url(const std::string& url, bool websocket);
User parse_user(const Json& data);
Message parse_message(const Json& data);
std::string heartbeat_payload(std::optional<std::uint64_t> sequence);
std::string identify_payload(const std::string& token, Intents intents);
std::string resume_payload(const std::string& token, const std::string& session, std::uint64_t sequence);
std::string message_payload(const MessageRequest& request);
std::string edit_message_payload(const std::string& content);
Message parse_message_response(const std::string& body);
std::string reaction_path(const std::string& emoji);

class AsyncMutex {
public:
    struct Guard {
        AsyncMutex* owner;
        Guard(AsyncMutex* owner) : owner(owner) {}
        Guard(Guard&& other) noexcept : owner(std::exchange(other.owner, nullptr)) {}
        Guard(const Guard&) = delete;
        ~Guard() { if (owner) owner->unlock(); }
    };
    task<Guard> lock();
private:
    void unlock();
    bool locked_ = false;
    std::deque<std::weak_ptr<asio::steady_timer>> waiters_;
};

struct CurlEngine;
struct Transfer {
    CURL* easy = nullptr;
    curl_slist* headers = nullptr;
    HttpResponse response;
    std::string body;
    std::size_t limit;
    asio::steady_timer completion;
    std::weak_ptr<CurlEngine> engine;
    std::exception_ptr error;
    bool done = false;
    bool keep_connected = false;
    Transfer(asio::any_io_executor executor, const ConnectionOptions& options);
    ~Transfer();
};

struct CurlEngine : std::enable_shared_from_this<CurlEngine> {
    asio::any_io_executor executor;
    CURLM* multi = nullptr;
    asio::steady_timer tick;
    std::unordered_map<CURL*, std::shared_ptr<Transfer>> pending;
    std::set<CURL*> attached;
    bool pumping = false;
    bool stopped = false;
    explicit CurlEngine(asio::any_io_executor executor);
    ~CurlEngine();
    void detach(CURL* handle);
    void cancel();
    task<std::shared_ptr<Transfer>> perform(std::shared_ptr<Transfer> transfer);
    static task<> pump(std::shared_ptr<CurlEngine> engine);
};

class WebSocket : public std::enable_shared_from_this<WebSocket> {
public:
    WebSocket(asio::any_io_executor executor, ConnectionOptions options);
    ~WebSocket();
    task<> connect(std::string url);
    task<std::string> read();
    task<> write(std::string data, unsigned flags = CURLWS_TEXT);
    task<> close();
    void abort();
    int close_code = 1006;
private:
    ConnectionOptions options_;
    std::shared_ptr<CurlEngine> engine_;
    std::shared_ptr<Transfer> transfer_;
    asio::ip::tcp::socket watch_;
    AsyncMutex writer_;
    bool aborted_ = false;
};

struct Connection : std::enable_shared_from_this<Connection> {
    asio::any_io_executor executor;
    std::shared_ptr<RestClient> rest;
    ConnectionOptions options;
    Intents intents;
    std::function<void(Message)> message;
    std::function<void(Ready)> ready;
    std::function<void(RawEvent)> raw;
    std::shared_ptr<WebSocket> socket;
    asio::steady_timer delay;
    asio::steady_timer heartbeat;
    asio::steady_timer handshake;
    std::optional<std::uint64_t> sequence;
    std::string session_id;
    std::string resume_url;
    std::string gateway_url;
    std::string token;
    std::exception_ptr heartbeat_error;
    bool stopped = false;
    bool started = false;
    bool awaiting_ack = false;
    bool hello_received = false;
    bool established = false;
    std::uint64_t generation = 0;
    unsigned identifies_left = 0;
    Connection(asio::any_io_executor executor, ConnectionOptions options, Intents intents);
    void stop();
    task<> run(std::string bot_token);
    task<> session();
    task<> send_heartbeat();
    static task<> heartbeat_loop(std::shared_ptr<Connection>, std::uint64_t, std::chrono::milliseconds);
};
} // namespace yl::discord::detail
