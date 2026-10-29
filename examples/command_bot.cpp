// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

using namespace yl::discord;
using namespace std::chrono_literals;

class Calculator final : public Bot {
public:
    explicit Calculator(std::shared_ptr<MessageTransport> transport) : Bot(std::move(transport)) {
        command<int, int>("add", [](Context ctx, int left, int right) -> task<> {
            // Bound the arithmetic for this example; converters already check each int's range.
            co_await ctx.reply(std::to_string(static_cast<long long>(left) + right));
        }, {.aliases = {"sum"}, .description = "Add two integers",
            .cooldown = Cooldown{.rate = 2, .per = 5s}});
        command<std::string, std::optional<int>>("repeat", [](Context ctx, std::string text, std::optional<int> count) -> task<> {
            const auto times = count.value_or(1);
            if (times < 1 || times > 3) throw BadArgument("Repeat count must be between 1 and 3");
            for (int i = 0; i < times; ++i) co_await ctx.send(text);
        }, {.description = "Repeat quoted text up to three times"});
        command("private", [](Context ctx) -> task<> { co_await ctx.send("Hello in DMs!"); },
            {.checks = {checks::dm_only()}});
        add_help_command();
    }

    task<> on_command_error(Context context, std::exception_ptr error) override {
        std::string message;
        try { std::rethrow_exception(error); }
        catch (const CommandOnCooldown&) { message = "Please wait before using that command again."; }
        catch (const ArgumentError& failure) { message = std::string(failure.what()) + "\n" + help_text(context.command_name); }
        catch (const CheckFailure&) { message = "This command is unavailable here."; }
        // Other errors still reach Client::on_error().
        if (message.empty()) co_await Bot::on_command_error(std::move(context), error);
        else co_await context.reply(std::move(message));
    }
};

int main() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Calculator bot{transport};
    for (const auto& content : {"!sum 20 22", "!repeat \"hello world\" 2", "!add nope 2", "!help"}) {
        Message message;
        message.id = Snowflake{100};
        message.channel_id = Snowflake{42};
        message.author = User{Snowflake{7}, "developer", false};
        message.content = content;
        bot.dispatch(std::move(message));
        bot.run();
    }
    for (const auto& sent : transport->sent()) std::cout << sent.content << '\n';
}
