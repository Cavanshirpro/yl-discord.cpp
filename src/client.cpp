// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord/client.hpp>
#include "network_internal.hpp"
#include <asio/co_spawn.hpp>
#include <asio/post.hpp>
#include <stdexcept>
#include <utility>
#include <iostream>

namespace yl::discord {
namespace {

task<> run_connection(std::shared_ptr<detail::Connection> connection, std::string token) {
    co_await connection->run(std::move(token));
}

// Keeping the callable in the parent frame also keeps captured coroutine-lambda
// state alive across suspension and command/listener registry changes.
template <typename Handler, typename Event>
task<> invoke(std::shared_ptr<Handler> handler, Event event) {
    co_await (*handler)(std::move(event));
}

template <typename Event>
task<> invoke_hook(Client* client, task<> (Client::*hook)(Event), Event event) {
    co_await (client->*hook)(std::move(event));
}

} // namespace

Client::Client(ClientOptions options) : options_(std::move(options)) {
    connection_ = std::make_shared<detail::Connection>(io_.get_executor(), options_.connection, options_.intents);
    transport_ = connection_->rest;
    connection_->message = [this](Message message) { dispatch(std::move(message)); };
    connection_->ready = [this](Ready event) { dispatch(std::move(event)); };
    connection_->raw = [this](RawEvent event) { dispatch(std::move(event)); };
}

Client::Client(std::shared_ptr<MessageTransport> transport, ClientOptions options)
    : transport_(std::move(transport)), options_(options) {
    if (!transport_) {
        throw std::invalid_argument("Client requires a message transport");
    }
}

Client::~Client() { io_.stop(); }

Client& Client::listen_message(MessageHandler handler) {
    (void)subscribe_message(std::move(handler));
    return *this;
}

Client& Client::listen_ready(ReadyHandler handler) {
    (void)subscribe_ready(std::move(handler));
    return *this;
}

Client& Client::listen_raw(RawHandler handler) {
    (void)subscribe_raw(std::move(handler));
    return *this;
}

ListenerConnection Client::subscribe_message(MessageHandler handler) { return message_handlers_->subscribe(std::move(handler)); }
ListenerConnection Client::subscribe_ready(ReadyHandler handler) { return ready_handlers_->subscribe(std::move(handler)); }
ListenerConnection Client::subscribe_raw(RawHandler handler) { return raw_handlers_->subscribe(std::move(handler)); }

void Client::dispatch(Message message) {
    asio::post(io_, [this, message = std::move(message)]() mutable {
        message.transport_ = transport_;
        const auto handlers = message_handlers_->snapshot();
        message_handlers_->publish(message);
        for (const auto& handler : handlers) spawn(invoke(handler, message), "message listener");
        handle_message(std::move(message));
    });
}

void Client::dispatch(Ready event) {
    asio::post(io_, [this, event = std::move(event)] {
        const auto handlers = ready_handlers_->snapshot();
        ready_handlers_->publish(event);
        spawn(invoke_hook(this, &Client::on_ready, event), "on_ready");
        for (const auto& handler : handlers) {
            spawn(invoke(handler, event), "ready listener");
        }
    });
}

void Client::dispatch(RawEvent event) {
    asio::post(io_, [this, event = std::move(event)] {
        const auto handlers = raw_handlers_->snapshot();
        raw_handlers_->publish(event);
        spawn(invoke_hook(this, &Client::on_raw_event, event), "on_raw_event:" + event.name);
        for (const auto& handler : handlers) {
            spawn(invoke(handler, event), "raw listener:" + event.name);
        }
    });
}

void Client::handle_message(Message message) {
    spawn(invoke_hook(this, &Client::on_message, message), "on_message");
}

void Client::spawn(task<> work, std::string source) {
    asio::co_spawn(io_, std::move(work), [this, source = std::move(source)](std::exception_ptr error) {
        if (error) {
            if (closing_) {
                try { std::rethrow_exception(error); }
                catch (const asio::system_error& e) { if (is_cancelled(e.code())) return; }
                catch (...) {}
            }
            report_error(TaskError{source, error});
        }
    });
}

void Client::report_error(TaskError error) {
    try {
        auto work = on_error(error);
        asio::co_spawn(io_, std::move(work), [this, error](std::exception_ptr failure) mutable {
            if (failure) {
                error.exception = failure;
                errors_.push_back(std::move(error));
            }
        });
    } catch (...) {
        error.exception = std::current_exception();
        errors_.push_back(std::move(error));
    }
}

void Client::run() {
    if (running_) {
        throw std::logic_error("Client::run cannot be called recursively");
    }
    running_ = true;
    io_.restart();
    try {
        io_.run();
    } catch (...) {
        running_ = false;
        throw;
    }
    running_ = false;
    if (!errors_.empty()) {
        auto failure = errors_.front().exception;
        errors_.erase(errors_.begin());
        std::rethrow_exception(failure);
    }
}

void Client::stop() noexcept { io_.stop(); }

void Client::run(std::string token) { spawn(start(std::move(token)), "gateway"); run(); }
task<> Client::start(std::string token) {
    if (!connection_) throw std::logic_error("A custom-transport Client cannot start a built-in Gateway");
    return run_connection(connection_, std::move(token));
}
void Client::close() {
    asio::post(io_, [this] {
        closing_ = true;
        message_handlers_->cancel_waits(); ready_handlers_->cancel_waits(); raw_handlers_->cancel_waits();
        if (connection_) connection_->stop();
    });
}
RestClient& Client::rest() {
    if (!connection_) throw std::logic_error("This Client has a custom transport");
    return *connection_->rest;
}

task<Message> Client::send_message(Snowflake channel, std::string content) {
    Message source;
    source.channel_id = channel;
    source.transport_ = transport_;
    return Message::send_impl(std::move(source), std::move(content), false);
}

Message Client::message_reference(Snowflake channel, Snowflake message) const {
    Message result;
    result.id = message;
    result.channel_id = channel;
    result.transport_ = transport_;
    return result;
}
task<Message> Client::send_message(Snowflake channel, MessageOptions options) {
    return Message::send_impl(message_reference(channel, Snowflake{}), std::move(options), false);
}
task<Message> Client::fetch_message(Snowflake channel, Snowflake message) {
    return message_reference(channel, message).fetch();
}
task<Message> Client::edit_message(Snowflake channel, Snowflake message, std::string content) {
    return message_reference(channel, message).edit(std::move(content));
}
task<Message> Client::edit_message(Snowflake channel, Snowflake message, MessageEditOptions options) {
    return message_reference(channel, message).edit(std::move(options));
}
task<> Client::delete_message(Snowflake channel, Snowflake message) {
    return message_reference(channel, message).delete_message();
}
task<> Client::add_reaction(Snowflake channel, Snowflake message, std::string emoji) {
    return message_reference(channel, message).add_reaction(std::move(emoji));
}
task<> Client::remove_reaction(Snowflake channel, Snowflake message, std::string emoji) {
    return message_reference(channel, message).remove_reaction(std::move(emoji));
}

std::vector<TaskError> Client::take_errors() { return std::exchange(errors_, {}); }

task<> Client::on_ready(Ready) { co_return; }
task<> Client::on_message(Message) { co_return; }
task<> Client::on_raw_event(RawEvent) { co_return; }
task<> Client::on_error(TaskError error) {
    try { std::rethrow_exception(error.exception); }
    catch (const std::exception& failure) { std::cerr << "yl-discord.cpp " << error.source << ": " << failure.what() << '\n'; }
    catch (...) { std::cerr << "yl-discord.cpp " << error.source << ": non-standard exception\n"; }
    std::rethrow_exception(error.exception);
    co_return;
}

} // namespace yl::discord
