// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

using namespace yl::discord;

int main() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    bot.command("status", [](Context context) -> task<> {
        Embed embed;
        embed.title = "Bot status"; embed.description = "Connected and ready."; embed.color = 0x57f287;
        embed.fields.push_back({"Commands", "Typed parameters, checks and cooldowns", false});
        embed.footer = EmbedFooter{"yl-discord.cpp", {}};
        MessageOptions options; options.embeds.push_back(std::move(embed));
        // allowed_mentions stays at its safe default. Whitelist IDs explicitly when needed.
        auto sent = co_await context.reply(options);
        MessageEditOptions update;
        update.embeds = sent.embeds;
        update.embeds->front().description = "Status updated.";
        (void)co_await sent.edit(std::move(update));
    });
    Message message; message.id = Snowflake{100}; message.channel_id = Snowflake{42}; message.content = "!status";
    bot.dispatch(std::move(message)); bot.run();
    for (const auto& request : transport->sent()) std::cout << request.embeds.front().title << '\n';
    for (const auto& action : transport->actions()) if (action.edit_options && action.edit_options->embeds)
        std::cout << action.edit_options->embeds->front().description << '\n';
}
