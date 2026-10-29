// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/transport.hpp>
#include <asio/any_io_executor.hpp>
#include <chrono>
#include <map>
#include <stdexcept>

namespace yl::discord {

/// @brief Network configuration. Plain HTTP/WS is accepted only on loopback.
struct ConnectionOptions {
    std::string rest_url = "https://discord.com/api/v10";
    std::chrono::milliseconds request_timeout{30'000};
    std::chrono::milliseconds connect_timeout{15'000};
    std::chrono::milliseconds reconnect_delay{1'000};
    unsigned max_reconnect_attempts = 8;
    unsigned max_rate_limit_retries = 5;
    std::size_t max_response_bytes = 8 * 1024 * 1024;
};

struct HttpResponse {
    long status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
};

class YL_DISCORD_EXPORT TransportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class YL_DISCORD_EXPORT ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class YL_DISCORD_EXPORT HttpError : public std::runtime_error {
public:
    HttpError(long status, int discord_code);
    [[nodiscard]] long status() const noexcept { return status_; }
    [[nodiscard]] int discord_code() const noexcept { return code_; }
private:
    long status_;
    int code_;
};

/// @brief Async bot-authenticated REST client; serializes each route/bucket.
class YL_DISCORD_EXPORT RestClient final : public MessageTransport {
public:
    explicit RestClient(asio::any_io_executor executor, ConnectionOptions options = {});
    ~RestClient() override;
    void authenticate(std::string bot_token);
    /// @brief Low-level request. Routes are API-relative and share all rate limits.
    [[nodiscard]] task<HttpResponse> request(std::string method, std::string route, std::string json_body = {});
    [[nodiscard]] task<Message> send_message(MessageRequest request) override;
    [[nodiscard]] task<Message> fetch_message(Snowflake channel, Snowflake message) override;
    [[nodiscard]] task<Message> edit_message(Snowflake channel, Snowflake message, std::string content) override;
    [[nodiscard]] task<Message> edit_message(Snowflake channel, Snowflake message, MessageEditOptions options) override;
    [[nodiscard]] task<> delete_message(Snowflake channel, Snowflake message) override;
    [[nodiscard]] task<> add_reaction(Snowflake channel, Snowflake message, std::string emoji) override;
    [[nodiscard]] task<> remove_reaction(Snowflake channel, Snowflake message, std::string emoji) override;
    /// @brief Cancel pending requests/waits on this executor; a new Client is needed after closing.
    void cancel();
private:
    struct State;
    std::shared_ptr<State> state_;
    static task<HttpResponse> request_impl(std::shared_ptr<State>, std::string, std::string, std::string,
                                           std::shared_ptr<const std::vector<FileUpload>> files = {});
    static task<Message> send_impl(std::shared_ptr<State>, MessageRequest);
    static task<Message> message_impl(std::shared_ptr<State>, std::string method, Snowflake, Snowflake, MessageEditOptions);
    static task<> delete_impl(std::shared_ptr<State>, Snowflake, Snowflake);
    static task<> reaction_impl(std::shared_ptr<State>, Snowflake, Snowflake, std::string, bool);
};

/// @brief Read a raw bot token from an environment variable, without logging it.
[[nodiscard]] YL_DISCORD_EXPORT std::string token_from_env(std::string_view name = "DISCORD_BOT_TOKEN");

} // namespace yl::discord
