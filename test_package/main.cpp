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

yl::discord::task<> wait_for_probe(yl::discord::Client& client, bool& observed) {
    auto message = co_await client.wait_for<yl::discord::Message>(std::chrono::seconds(1),
        [](const yl::discord::Message& event) { return event.content == "!p 42"; });
    observed = message.channel_id == yl::discord::Snowflake{42};
}

yl::discord::task<> cancelled_request_probe(yl::discord::Client& client, bool& observed) {
    try { (void)co_await client.rest().request("GET", "/never-sent"); }
    catch (const asio::system_error& error) {
        observed = yl::discord::is_cancelled(error.code());
        if (!observed) std::cerr << "Cancellation code " << error.code().value() << " category " << error.code().category().name()
            << "; expected " << static_cast<int>(asio::error::operation_aborted) << " category " << asio::error::get_system_category().name() << '\n';
    }
}

int main() {
    using namespace yl::discord;
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    bool observed = false;
    int listeners = 0;
    auto listener = bot.subscribe_message([&](Message) -> task<> { ++listeners; co_return; });
    bot.spawn(wait_for_probe(bot, observed));
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
    if (!bot.get_command("p") || bot.help_text("p").find("<integer>") == std::string::npos) {
        std::cerr << "Installed command metadata failed\n"; return 1;
    }
    Message message;
    message.id = Snowflake{100};
    message.channel_id = Snowflake{42};
    message.content = "!p 42";
    bot.dispatch(std::move(message));
    bot.run();
    listener.disconnect();
    bool replied = false;
    bool probed = false;
    for (const auto& request : transport->sent()) {
        replied = replied || (request.content == "Pong 42!" && request.reply_to == Snowflake{100});
        probed = probed || (request.content == "transport probe" && !request.reply_to);
    }
    if (transport->sent().size() != 2 || transport->actions().size() != 5 || !replied || !probed || !observed || listeners != 1 || listener.connected()) {
        std::cerr << "Installed consumer mismatch: sends=" << transport->sent().size() << " actions=" << transport->actions().size()
            << " replied=" << replied << " probed=" << probed << " waited=" << observed << " listened=" << listeners
            << " connected=" << listener.connected() << '\n';
        return 1;
    }
    // Cancellation must also be recognized when its Asio category originated in a DLL.
    Client cancelled;
    cancelled.rest().authenticate("package-fixture-credential");
    cancelled.close();
    bool cancellation_seen = false;
    cancelled.spawn(cancelled_request_probe(cancelled, cancellation_seen));
    cancelled.run();
    if (!cancellation_seen) { std::cerr << "Installed cancellation probe failed\n"; return 1; }
    std::cout << "yl-discord.cpp " << version << ": installed package works\n";
}
