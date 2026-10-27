// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/transport.hpp>
#include <yl/discord/network.hpp>
#include <asio/any_io_executor.hpp>
#include <asio/io_context.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace yl::discord {
namespace detail { struct Connection; }

struct ClientOptions {
    Intents intents = Intents::defaults();
    ConnectionOptions connection;
};

struct TaskError {
    std::string source;
    std::exception_ptr exception;
};

/// @brief Single-threaded async event host with a pluggable message transport.
class YL_DISCORD_EXPORT Client {
public:
    using MessageHandler = std::function<task<>(Message)>;
    using ReadyHandler = std::function<task<>(Ready)>;
    using RawHandler = std::function<task<>(RawEvent)>;

    explicit Client(ClientOptions options = {});
    explicit Client(std::shared_ptr<MessageTransport> transport, ClientOptions options = {});
    virtual ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Client& listen_message(MessageHandler handler);
    Client& listen_ready(ReadyHandler handler);
    Client& listen_raw(RawHandler handler);

    // Thread-safe ingress used by the Gateway and offline test transports.
    void dispatch(Message message);
    void dispatch(Ready event);
    void dispatch(RawEvent event);

    // Drains queued work and timers; use run(token) to start the built-in Gateway.
    // A subsequent run() can process newly dispatched events.
    void run();
    /// @brief Connect a bot account and run until close() or a fatal connection error.
    void run(std::string bot_token);
    [[nodiscard]] task<> start(std::string bot_token);
    /// @brief Thread-safe graceful live shutdown, including pending HTTP cancellation.
    void close();
    [[nodiscard]] RestClient& rest();
    void stop() noexcept;
    void spawn(task<> work, std::string source = "background");
    template <typename T>
    void spawn(task<T> work, std::string source = "background") {
        spawn(detail::discard_result(std::move(work)), std::move(source));
    }
    [[nodiscard]] task<Message> send_message(Snowflake channel, std::string content);
    [[nodiscard]] asio::any_io_executor executor() noexcept { return io_.get_executor(); }
    [[nodiscard]] const ClientOptions& options() const noexcept { return options_; }
    [[nodiscard]] std::vector<TaskError> take_errors();

    virtual task<> on_ready(Ready event);
    virtual task<> on_message(Message message);
    virtual task<> on_raw_event(RawEvent event);
    virtual task<> on_error(TaskError error);

protected:
    virtual void handle_message(Message message);

private:
    void report_error(TaskError error);
    asio::io_context io_;
    std::shared_ptr<MessageTransport> transport_;
    std::shared_ptr<detail::Connection> connection_;
    ClientOptions options_;
    std::vector<std::shared_ptr<MessageHandler>> message_handlers_;
    std::vector<std::shared_ptr<ReadyHandler>> ready_handlers_;
    std::vector<std::shared_ptr<RawHandler>> raw_handlers_;
    std::vector<TaskError> errors_;
    bool running_ = false;
    bool closing_ = false;
};

} // namespace yl::discord
