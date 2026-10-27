// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

int main() {
    using namespace yl::discord;
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    bot.command("ping", [](Context ctx) -> task<> { co_await ctx.reply("Pong!"); });
    Message message;
    message.id = Snowflake{100};
    message.channel_id = Snowflake{42};
    message.content = "!ping";
    bot.dispatch(std::move(message));
    bot.run();
    if (transport->sent().size() != 1 || transport->sent()[0].content != "Pong!"
        || transport->sent()[0].reply_to != Snowflake{100}) {
        return 1;
    }
    std::cout << "yl-discord.cpp " << version << ": installed package works\n";
}
