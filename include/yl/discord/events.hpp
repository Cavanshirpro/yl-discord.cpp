// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <yl/discord/models.hpp>
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/redirect_error.hpp>
#include <algorithm>
#include <concepts>
#include <exception>
#include <functional>
#include <stdexcept>
#include <stop_token>
#include <vector>

namespace yl::discord {
namespace detail {
struct ListenerControl {
    virtual ~ListenerControl() = default;
    virtual void disconnect() noexcept = 0;
    virtual bool connected() const noexcept = 0;
};
template <typename Event> struct EventSource;
} // namespace detail

/// @brief Explicit listener handle. Dropping it does not disconnect the listener.
/// Access on the Client executor or before/after run(), like registry mutation.
class YL_DISCORD_EXPORT ListenerConnection {
public:
    ListenerConnection() = default;
    void disconnect() noexcept;
    [[nodiscard]] bool connected() const noexcept;
    explicit operator bool() const noexcept { return connected(); }
private:
    explicit ListenerConnection(std::weak_ptr<detail::ListenerControl> control) : control_(std::move(control)) {}
    std::weak_ptr<detail::ListenerControl> control_;
    template <typename Event> friend struct detail::EventSource;
};

class YL_DISCORD_EXPORT EventTimeout : public std::runtime_error {
public:
    EventTimeout() : std::runtime_error("Timed out waiting for an event") {}
};

namespace detail {
template <typename Event>
struct EventWait {
    using Predicate = std::function<bool(const Event&)>;
    asio::steady_timer timer;
    Predicate predicate;
    std::optional<Event> result;
    std::exception_ptr error;
    bool done = false;

    EventWait(asio::any_io_executor executor, std::chrono::steady_clock::duration timeout, Predicate predicate)
        : timer(executor, timeout), predicate(std::move(predicate)) {}

    void finish(std::exception_ptr failure = {}) {
        if (done) return;
        error = failure;
        done = true;
        timer.cancel();
    }
    void cancel() { finish(std::make_exception_ptr(asio::system_error(asio::error::operation_aborted))); }
    void accept(const Event& event) {
        if (done) return;
        if (std::chrono::steady_clock::now() >= timer.expiry()) {
            finish(std::make_exception_ptr(EventTimeout{}));
            return;
        }
        try {
            if (!predicate || predicate(event)) {
                result = event;
                finish();
            }
        } catch (...) { finish(std::current_exception()); }
    }
};

template <typename Event>
struct EventSource {
    using Handler = std::function<task<>(Event)>;
    struct Entry : ListenerControl {
        std::shared_ptr<Handler> handler;
        explicit Entry(Handler value) : handler(std::make_shared<Handler>(std::move(value))) {}
        void disconnect() noexcept override { handler.reset(); }
        bool connected() const noexcept override { return static_cast<bool>(handler); }
    };
    std::vector<std::shared_ptr<Entry>> entries;
    std::vector<std::weak_ptr<EventWait<Event>>> waits;
    bool stopped = false;

    ListenerConnection subscribe(Handler handler) {
        if (!handler) throw std::invalid_argument("Event handler cannot be empty");
        std::erase_if(entries, [](const auto& entry) { return !entry->handler; });
        auto entry = std::make_shared<Entry>(std::move(handler));
        entries.push_back(entry);
        return ListenerConnection{entry};
    }
    std::vector<std::shared_ptr<Handler>> snapshot() {
        std::erase_if(entries, [](const auto& entry) { return !entry->handler; });
        std::vector<std::shared_ptr<Handler>> result;
        result.reserve(entries.size());
        for (const auto& entry : entries) result.push_back(entry->handler);
        return result;
    }
    void publish(const Event& event) {
        std::erase_if(waits, [](const auto& weak) { return weak.expired(); });
        // A predicate may register another wait; never iterate a mutable registry.
        const auto pending = waits;
        for (const auto& weak : pending) if (auto state = weak.lock()) state->accept(event);
    }
    void cancel_waits() {
        stopped = true;
        const auto pending = waits;
        for (const auto& weak : pending) if (auto state = weak.lock()) state->cancel();
        waits.clear();
    }
};

template <typename Event>
task<Event> wait_event(std::shared_ptr<EventSource<Event>> source, asio::any_io_executor expected_executor,
                      std::chrono::steady_clock::duration timeout,
                      std::function<bool(const Event&)> predicate, std::stop_token token) {
    if (timeout <= std::chrono::steady_clock::duration::zero() || timeout > std::chrono::hours(24))
        throw std::invalid_argument("Event timeout must be positive and at most 24 hours");
    auto executor = co_await asio::this_coro::executor;
    if (&asio::query(executor, asio::execution::context) != &asio::query(expected_executor, asio::execution::context))
        throw std::logic_error("wait_for must run on the Client's event loop");
    if (source->stopped || token.stop_requested()) throw asio::system_error(asio::error::operation_aborted);
    auto state = std::make_shared<EventWait<Event>>(executor, timeout, std::move(predicate));
    std::erase_if(source->waits, [](const auto& weak) { return weak.expired(); });
    source->waits.push_back(state);
    auto cancel = [weak = std::weak_ptr<EventWait<Event>>(state), executor] {
        // std::stop_source can be requested by another thread. State belongs to this executor.
        asio::post(executor, [weak] { if (auto active = weak.lock()) active->cancel(); });
    };
    std::optional<std::stop_callback<decltype(cancel)>> cancellation;
    if (token.stop_possible()) cancellation.emplace(token, cancel);
    asio::error_code error;
    co_await state->timer.async_wait(asio::redirect_error(asio::use_awaitable, error));
    if (state->error) std::rethrow_exception(state->error);
    if (state->result) co_return std::move(*state->result);
    if (error) throw asio::system_error(error);
    throw EventTimeout{};
}
} // namespace detail
} // namespace yl::discord
