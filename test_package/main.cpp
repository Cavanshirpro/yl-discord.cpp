// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#if defined(_WIN32)
#define UNICODE
#define _UNICODE
#include <windows.h>
#endif
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

int main() {
    using namespace yl::discord;
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    // Exercise the public payload across the ABI with Win32 Unicode macros active.
    bot.spawn(transport->send_message(MessageRequest{Snowflake{42}, "transport probe", std::nullopt}));
    bot.command("ping", [](Context ctx) -> task<> { co_await ctx.reply("Pong!"); });
    Message message;
    message.id = Snowflake{100};
    message.channel_id = Snowflake{42};
    message.content = "!ping";
    bot.dispatch(std::move(message));
    bot.run();
    bool replied = false;
    bool probed = false;
    for (const auto& request : transport->sent()) {
        replied = replied || (request.content == "Pong!" && request.reply_to == Snowflake{100});
        probed = probed || (request.content == "transport probe" && !request.reply_to);
    }
    if (transport->sent().size() != 2 || !replied || !probed) {
        return 1;
    }
    std::cout << "yl-discord.cpp " << version << ": installed package works\n";
}
