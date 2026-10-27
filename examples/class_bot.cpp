// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>

using namespace yl::discord;

class Greetings final : public Cog {
public:
    std::vector<Command> commands() override {
        return {{"hello", [this](Context ctx) -> task<> {
            ++greetings_;
            co_await ctx.reply("Hello, " + ctx.message.author.name + "! (#" + std::to_string(greetings_) + ")");
        }, {.aliases = {"hi"}, .description = "Greet a user"}}};
    }

private:
    int greetings_ = 0;
};

class MyBot final : public Bot {
public:
    using Bot::Bot;

    task<> on_ready(Ready event) override {
        std::cout << "Offline ready event: " << event.user.name << '\n';
        co_return;
    }

    task<> on_message(Message message) override {
        std::cout << message.author.name << ": " << message.content << '\n';
        // Registered commands are processed independently of this hook.
        co_return;
    }

    task<> on_error(TaskError error) override {
        try {
            std::rethrow_exception(error.exception);
        } catch (const std::exception& e) {
            std::cerr << error.source << ": " << e.what() << '\n';
        }
        co_return;
    }
};

int main() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    MyBot bot{transport};
    bot.add_cog(std::make_shared<Greetings>());
    bot.dispatch(Ready{User{Snowflake{1}, "example-bot", true}});
    Message message;
    message.id = Snowflake{100};
    message.channel_id = Snowflake{42};
    message.author = User{Snowflake{7}, "developer", false};
    message.content = "!hi";
    bot.dispatch(std::move(message));
    bot.run();
    for (const auto& sent : transport->sent()) {
        std::cout << sent.content << '\n';
    }
}
