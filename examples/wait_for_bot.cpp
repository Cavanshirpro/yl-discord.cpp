// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

using namespace yl::discord;
using namespace std::chrono_literals;

task<> simulate_answer(Client& client) {
    co_await sleep_for(2ms);
    Message message; message.id = Snowflake{101}; message.channel_id = Snowflake{42};
    message.author = User{Snowflake{7}, "developer", false}; message.content = "yes";
    client.dispatch(std::move(message));
}

int main() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    auto audit = bot.subscribe_message([](Message message) -> task<> {
        std::cout << "Observed: " << message.content << '\n'; co_return;
    });
    bot.command("confirm", [&bot](Context context) -> task<> {
        co_await context.send("Reply yes in this channel within five seconds.");
        bool expired = false;
        try {
            auto answer = co_await bot.wait_for<Message>(5s, [author = context.message.author.id, channel = context.message.channel_id](const Message& message) {
                return !message.author.bot && message.author.id == author && message.channel_id == channel && message.content == "yes";
            });
            co_await answer.reply("Confirmed.");
        } catch (const EventTimeout&) { expired = true; }
        if (expired) co_await context.reply("Confirmation expired.");
    });
    Message command; command.id = Snowflake{100}; command.channel_id = Snowflake{42};
    command.author = User{Snowflake{7}, "developer", false}; command.content = "!confirm";
    bot.dispatch(std::move(command)); bot.spawn(simulate_answer(bot)); bot.run();
    audit.disconnect(); // Future events no longer run this listener.
    for (const auto& sent : transport->sent()) std::cout << sent.content << '\n';
}
