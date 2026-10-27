// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <chrono>
#include <iostream>

using namespace yl::discord;
using namespace std::chrono_literals;

int main() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport, {.command_prefix = "!", .intents = Intents::defaults().with(Intent::message_content)}};

    bot.command("ping", [](Context ctx) -> task<> {
        co_await sleep_for(5ms); // Suspends without blocking other events.
        co_await ctx.reply("Pong!");
    }, {.aliases = {"p"}, .description = "Check responsiveness"});

    bot.command("echo", [](Context ctx) -> task<> {
        if (ctx.args.empty()) {
            co_await ctx.send("Usage: !echo \"hello world\"");
        } else {
            co_await ctx.send(ctx.args.front());
        }
    });

    // Simulated incoming events. This example never connects to Discord.
    for (const auto& content : {"!ping", "!echo \"hello world\""}) {
        Message message;
        message.id = Snowflake{100};
        message.channel_id = Snowflake{42};
        message.author = User{Snowflake{7}, "developer", false};
        message.content = content;
        bot.dispatch(std::move(message));
    }
    bot.run();
    for (const auto& sent : transport->sent()) {
        std::cout << "[channel " << sent.channel_id.str() << "] " << sent.content << '\n';
    }
}
