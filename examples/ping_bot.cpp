// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <asio/signal_set.hpp>
#include <csignal>
#include <iostream>

using namespace yl::discord;

class PingBot final : public Bot {
public:
    using Bot::Bot;
    asio::signal_set* signals = nullptr;
    task<> on_ready(Ready event) override {
        std::cout << "Connected as " << event.user.name << '\n';
        co_return;
    }
    task<> on_error(TaskError error) override {
        // A pending signal wait must also end after a fatal connection failure.
        if (error.source == "gateway" && signals) signals->cancel();
        std::rethrow_exception(error.exception);
        co_return;
    }
};

int main() {
    try {
        PingBot bot{BotOptions{.intents = Intents::defaults().with(Intent::message_content)}};
        asio::signal_set signals(bot.executor(), SIGINT, SIGTERM);
        bot.signals = &signals;
        signals.async_wait([&bot](asio::error_code error, int) { if (!error) bot.close(); });
        bot.command("ping", [](Context ctx) -> task<> { co_await ctx.reply("Pong!"); });
        bot.command("echo", [](Context ctx) -> task<> {
            if (!ctx.args.empty()) co_await ctx.send(ctx.args.front());
        });
        bot.run(token_from_env());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
