// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
    bot.command<int>("ping", [](Context ctx, int value) -> task<> {
        auto sent = co_await ctx.reply("Pong " + std::to_string(value) + "!");
        auto edited = co_await sent.edit("updated");
        if ((co_await edited.fetch()).content != "updated") throw std::runtime_error("Installed edit/fetch failed");
        co_await edited.add_reaction("fixture");
        co_await edited.remove_reaction("fixture");
        co_await edited.delete_message();
    }, {.aliases = {"p"}, .checks = {checks::dm_only()}, .cooldown = Cooldown{}});
    if (!bot.get_command("p") || bot.help_text("p").find("<integer>") == std::string::npos) return 1;
    Message message;
    message.id = Snowflake{100};
    message.channel_id = Snowflake{42};
    message.content = "!p 42";
    bot.dispatch(std::move(message));
    bot.run();
    bool replied = false;
    bool probed = false;
    for (const auto& request : transport->sent()) {
        replied = replied || (request.content == "Pong 42!" && request.reply_to == Snowflake{100});
        probed = probed || (request.content == "transport probe" && !request.reply_to);
    }
    if (transport->sent().size() != 2 || transport->actions().size() != 5 || !replied || !probed) {
        return 1;
    }
    std::cout << "yl-discord.cpp " << version << ": installed package works\n";
}
