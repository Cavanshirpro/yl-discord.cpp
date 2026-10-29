// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <asio/steady_timer.hpp>
#include <chrono>
#include <iostream>

using namespace yl::discord;
using namespace std::chrono_literals;

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

class NetworkBot final : public Bot {
public:
    using Bot::Bot;
    int messages = 0;
    int raw_events = 0;
    int ready_events = 0;
    task<> on_ready(Ready event) override {
        check(event.user.bot && event.user.name == "fixture-bot", "READY user parsing failed");
        ++ready_events;
        co_return;
    }
    task<> on_message(Message message) override {
        check(message.author.name == "fixture-user", "MESSAGE_CREATE user parsing failed");
        ++messages;
        co_return;
    }
    task<> on_raw_event(RawEvent event) override {
        check(event.sequence.has_value(), "Missing raw sequence");
        ++raw_events;
        co_return;
    }
};

task<> rest_scenario(Client& client) {
    auto first = co_await client.rest().request("GET", "/channels/42/limited");
    check(first.status == 200, "429 retry failed");
    auto second = co_await client.rest().request("GET", "/channels/42/shared");
    check(second.status == 200, "Shared bucket request failed");
    co_await client.rest().request("GET", "/channels/42/limited");
    co_await client.rest().request("GET", "/global");
    co_await client.rest().request("GET", "/after-global");
    bool unauthorized = false;
    try { co_await client.rest().request("GET", "/unauthorized"); }
    catch (const HttpError& error) { unauthorized = error.status() == 401; }
    check(unauthorized, "401 was not exposed as HttpError");
    unauthorized = false;
    try { co_await client.rest().request("GET", "/must-not-send"); }
    catch (const HttpError& error) { unauthorized = error.status() == 401; }
    check(unauthorized, "Invalid authentication was not retained");
}
task<> timeout_scenario(Client& client) {
    bool timed_out = false;
    try { co_await client.rest().request("GET", "/slow"); }
    catch (const TransportError&) { timed_out = true; }
    check(timed_out, "HTTP deadline not enforced");
}
task<> cancel_request(Client& client) { co_await client.rest().request("GET", "/long-limit"); }
task<> close_soon(Client& client) { co_await sleep_for(80ms); client.close(); }

task<> message_operations_scenario(Client& client) {
    auto message = co_await client.fetch_message(Snowflake{42}, Snowflake{321});
    check(message.content == "initial", "Fetch message failed");
    auto pending = message.edit("edited @everyone <@7> \"quoted\"");
    message = Message{};
    auto edited = co_await std::move(pending);
    check(edited.content == "edited @everyone <@7> \"quoted\"", "Edit result was not parsed");
    auto refreshed = co_await edited.fetch();
    check(refreshed.id == Snowflake{321} && refreshed.content == edited.content, "Fetch result lost its binding");
    co_await refreshed.add_reaction("\xf0\x9f\x91\x8d");
    co_await refreshed.remove_reaction("\xf0\x9f\x91\x8d");
    co_await client.add_reaction(Snowflake{42}, Snowflake{321}, "<a:party:123>");
    co_await client.remove_reaction(Snowflake{42}, Snowflake{321}, "party:123");
    for (const auto id : {322ULL, 323ULL}) {
        bool invalid = false;
        try { co_await client.fetch_message(Snowflake{42}, Snowflake{id}); }
        catch (const ProtocolError&) { invalid = true; }
        check(invalid, "Malformed message response was not a ProtocolError");
    }
    bool forbidden = false;
    try { co_await client.edit_message(Snowflake{42}, Snowflake{324}, "denied"); }
    catch (const HttpError& error) { forbidden = error.status() == 403 && error.discord_code() == 50013; }
    check(forbidden, "Permission failure was not retained");
    for (auto emoji : {"", ".", "..", "bad\nemoji"}) {
        bool invalid = false;
        try { co_await refreshed.add_reaction(emoji); }
        catch (const std::invalid_argument&) { invalid = true; }
        check(invalid, "Invalid reaction was sent");
    }
    co_await refreshed.delete_message();
    bool absent = false;
    try { co_await client.fetch_message(Snowflake{42}, Snowflake{321}); }
    catch (const HttpError& error) { absent = error.status() == 404 && error.discord_code() == 10008; }
    check(absent, "Deleted message was still available");
}

task<> embeds_scenario(Client& client) {
    Embed embed;
    embed.title = "Status"; embed.description = "UTF-8 \xf0\x9f\x98\x80"; embed.color = 0x123456;
    embed.url = "https://example.invalid/"; embed.timestamp = "2026-10-29T00:00:00Z";
    embed.fields = {{"name", "value", true}};
    embed.footer = EmbedFooter{"footer", "https://example.invalid/footer.png"};
    embed.author = EmbedAuthor{"author", "https://example.invalid/author", "https://example.invalid/icon.png"};
    embed.image_url = "https://example.invalid/image.png"; embed.thumbnail_url = "https://example.invalid/thumb.png";
    MessageOptions options; options.content = "hello <@7>"; options.embeds.push_back(embed);
    options.allowed_mentions.users = {Snowflake{7}}; options.allowed_mentions.roles = {Snowflake{8}};
    auto message = co_await client.send_message(Snowflake{42}, options);
    check(message.embeds.size() == 1 && message.embeds[0].fields[0].inline_field &&
        message.embeds[0].author->name == "author" && message.embeds[0].color == 0x123456, "Rich response parsing failed");
    MessageEditOptions clear; clear.embeds = std::vector<Embed>{};
    auto cleared = co_await message.edit(clear);
    check(cleared.content == "hello <@7>" && cleared.embeds.empty(), "Embed clearing changed content");
    MessageEditOptions replace; replace.content = ""; replace.embeds = std::vector<Embed>{embed};
    auto replaced = co_await cleared.edit(replace);
    check(replaced.content.empty() && replaced.embeds.size() == 1, "Embed-only edit failed");
    MessageOptions reply; reply.embeds = {embed}; reply.allowed_mentions = AllowedMentions::all();
    auto response = co_await replaced.reply(reply);
    check(response.content.empty() && response.embeds[0].type == "rich", "Embed-only reply failed");
    bool invalid = false;
    MessageOptions invalid_options; invalid_options.content = std::string(2001, 'a');
    try { (void)co_await client.send_message(Snowflake{42}, invalid_options); }
    catch (const std::invalid_argument&) { invalid = true; }
    check(invalid, "Invalid payload was not rejected before HTTP");
}
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        const std::string scenario = argv[1];
        ConnectionOptions connection;
        connection.rest_url = argv[2];
        connection.request_timeout = scenario == "timeout" ? 80ms : 2s;
        connection.connect_timeout = 1s;
        connection.reconnect_delay = 1ms;
        connection.max_reconnect_attempts = 2;
        NetworkBot bot{BotOptions{.command_prefix = "!", .intents = Intents::defaults().with(Intent::message_content), .connection = connection}};
        int replies = 0;
        bot.command("ping", [&](Context ctx) -> task<> {
            check(ctx.args == std::vector<std::string>{"two words"}, "Live command parser failed");
            auto sent = co_await ctx.reply("Pong! \"quoted\" \n UTF-8: \xc3\xbc");
            check(sent.content == "reply accepted", "REST message parsing failed");
            ++replies;
            // A server-requested Resume is exercised before the second command.
            if (replies == 2 || scenario == "fragment") bot.close();
        });
        if (scenario == "rest" || scenario == "timeout" || scenario == "cancel" || scenario == "cancel-transfer" || scenario == "messages" || scenario == "embeds") {
            bot.rest().authenticate("fixture-only-credential");
            if (scenario == "rest") bot.spawn(rest_scenario(bot));
            if (scenario == "messages") bot.spawn(message_operations_scenario(bot));
            if (scenario == "embeds") bot.spawn(embeds_scenario(bot));
            if (scenario == "timeout") bot.spawn(timeout_scenario(bot));
            if (scenario == "cancel") { bot.spawn(cancel_request(bot)); bot.spawn(close_soon(bot)); }
            if (scenario == "cancel-transfer") { bot.spawn(bot.rest().request("GET", "/slow")); bot.spawn(close_soon(bot)); }
            bot.run();
        } else if (scenario == "bad-discovery") {
            bool rejected = false;
            try { bot.run("fixture-only-credential"); }
            catch (const ProtocolError& error) { rejected = std::string(error.what()) == "Invalid Gateway discovery response"; }
            check(rejected, "Malformed discovery leaked a JSON diagnostic or used the Gateway");
        } else if (scenario == "close-handshake") {
            bot.spawn(close_soon(bot));
            bot.run("fixture-only-credential");
        } else if (scenario == "pause-destroy") {
            bot.listen_ready([&](Ready) -> task<> { bot.stop(); co_return; });
            bot.run("fixture-only-credential");
            check(bot.ready_events == 1, "Pause did not observe Ready");
        } else if (scenario == "fatal" || scenario == "missing-ack" || scenario == "handshake-timeout") {
            bool failed = false;
            try { bot.run("fixture-only-credential"); }
            catch (const ProtocolError& error) { failed = scenario == "fatal" && std::string(error.what()).find("4004") != std::string::npos; }
            catch (const TransportError&) { failed = scenario == "missing-ack" || scenario == "handshake-timeout"; }
            check(failed, "Gateway failure/reconnect budget not enforced");
        } else {
            bot.run("fixture-only-credential");
            check(replies == (scenario == "fragment" ? 1 : 2), "Live replies were lost or duplicated");
            check(bot.ready_events == 1, "Resume must not produce a new Ready event");
            check(bot.messages == replies && bot.raw_events >= replies + 1, "Live dispatch did not reach independent hooks");
        }
        std::cout << "PASS network " << scenario << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL network: " << error.what() << '\n';
        return 1;
    }
}
