// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#pragma once

#include <asio/awaitable.hpp>
#include <asio/steady_timer.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <utility>
#include <asio/version.hpp>

static_assert(ASIO_VERSION >= 103002, "yl-discord.cpp requires standalone Asio >= 1.30.2");

namespace yl::discord {

// The executor is intentionally exposed: advanced users can await Asio I/O directly.
template <typename T = void>
using task = asio::awaitable<T>;

namespace detail {
template <typename T>
task<> discard_result(task<T> work) {
    (void)co_await std::move(work);
}
} // namespace detail

template <typename Rep, typename Period>
task<> sleep_for(std::chrono::duration<Rep, Period> duration) {
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor);
    timer.expires_after(std::chrono::duration_cast<asio::steady_timer::duration>(duration));
    co_await timer.async_wait(asio::use_awaitable);
}

} // namespace yl::discord
