// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <asio/post.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace yl::discord;
using namespace std::chrono_literals;

namespace {

void check(bool condition, const char* expression) {
    if (!condition) {
        throw std::runtime_error(expression);
    }
}
#define CHECK(expression) check(static_cast<bool>(expression), #expression)

template <typename Exception, typename Callback>
void throws(Callback callback) {
    bool caught = false;
    try {
        callback();
    } catch (const Exception&) {
        caught = true;
    }
    CHECK(caught);
}

Message incoming(std::string content, bool bot = false) {
    Message message;
    message.id = Snowflake{100};
    message.channel_id = Snowflake{42};
    message.author = User{Snowflake{7}, "tester", bot};
    message.content = std::move(content);
    return message;
}

class RecordingBot : public Bot {
public:
    using Bot::Bot;
    std::vector<TaskError> failures;
    int messages = 0;
    task<> on_message(Message) override {
        ++messages;
        co_return;
    }
    task<> on_error(TaskError error) override {
        failures.push_back(std::move(error));
        co_return;
    }
};

void snowflakes_and_intents() {
    CHECK(Snowflake{"18446744073709551615"}.value() == std::numeric_limits<std::uint64_t>::max());
    CHECK(Snowflake{"175928847299117063"}.str() == "175928847299117063");
    CHECK(Snowflake{0}.created_at().time_since_epoch().count() == 1420070400000LL);
    for (auto invalid : {"", "-1", "+1", " 1", "1x", "18446744073709551616"}) {
        throws<std::invalid_argument>([&] { (void)Snowflake{std::string_view{invalid}}; });
    }
    CHECK(Intents::defaults().contains(Intent::guild_messages));
    CHECK(!Intents::defaults().contains(Intent::message_content));
    CHECK(!Intents::defaults().contains(Intent::guild_members));
    CHECK(Intents::defaults().with(Intent::message_content).contains(Intent::message_content));
}

void argument_parser() {
    CHECK(parse_arguments("  echo  one\ttwo ") == (std::vector<std::string>{"echo", "one", "two"}));
    const auto quoted = parse_arguments(R"args("two words" 'three words' "" escaped\ space)args");
    CHECK(quoted ==
          (std::vector<std::string>{"two words", "three words", "", "escaped space"}));
    const auto escaped = parse_arguments(R"args(pre"fix" 'a\b' "say \"hi\"")args");
    CHECK(escaped ==
          (std::vector<std::string>{"prefix", "a\\b", "say \"hi\""}));
    CHECK(parse_arguments("\t \n").empty());
    throws<ArgumentError>([] { (void)parse_arguments("\"unclosed"); });
    throws<ArgumentError>([] { (void)parse_arguments("trailing\\"); });
}

void command_dispatch_and_concurrency() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    int listeners = 0;
    bot.listen_message([&](Message) -> task<> { ++listeners; co_return; });
    bot.command("slow", [](Context ctx) -> task<> {
        co_await sleep_for(2ms);
        auto response = co_await ctx.reply("slow");
        co_await response.reply("chained reply");
    }, {.aliases = {"s"}});
    bot.command("echo", [](Context ctx) -> task<> {
        CHECK(ctx.command_name == "echo");
        CHECK(ctx.args == (std::vector<std::string>{"two words", ""}));
        co_await ctx.send("fast");
    });
    bot.dispatch(incoming("!s"));
    bot.dispatch(incoming("!echo \"two words\" \"\""));
    bot.dispatch(incoming("!echo ignored", true));
    bot.dispatch(incoming("ordinary text"));
    bot.dispatch(incoming("!unknown"));
    bot.dispatch(incoming("!"));
    bot.run();
    CHECK(bot.failures.empty());
    CHECK(bot.messages == 6);
    CHECK(listeners == 6);
    CHECK(transport->sent().size() == 3);
    CHECK(transport->sent()[0].content == "fast");
    CHECK(!transport->sent()[0].reply_to.has_value());
    CHECK(transport->sent()[1].content == "slow");
    CHECK(transport->sent()[1].reply_to == Snowflake{100});
    CHECK(transport->sent()[2].reply_to == Snowflake{2});
    CHECK(transport->sent()[2].channel_id == Snowflake{42});
}

void callback_and_parse_errors_are_isolated() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    bot.command("bad", [](Context) -> task<> {
        co_await sleep_for(1ms);
        throw std::runtime_error("command failure");
    });
    bot.command("good", [](Context ctx) -> task<> { co_await ctx.send("still running"); });
    bot.dispatch(incoming("!bad"));
    bot.dispatch(incoming("!good"));
    bot.dispatch(incoming("!good \"unclosed"));
    bot.run();
    CHECK(bot.failures.size() == 2);
    CHECK(bot.failures[0].source == "commands");
    CHECK(transport->sent().size() == 1);
    CHECK(transport->sent()[0].content == "still running");
    CHECK(bot.take_errors().empty());
}

void default_errors_are_visible() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    bot.command("fail", [](Context) -> task<> { throw std::runtime_error("visible"); co_return; });
    bot.dispatch(incoming("!fail"));
    bot.dispatch(incoming("!fail"));
    throws<std::runtime_error>([&] { bot.run(); });
    auto remaining = bot.take_errors();
    CHECK(remaining.size() == 1);
    CHECK(remaining[0].source == "commands");
    bot.run();
}

class ThrowingHookBot final : public RecordingBot {
public:
    using RecordingBot::RecordingBot;
    // A task-returning function is allowed to throw before returning an awaitable.
    task<> on_message(Message) override { throw std::runtime_error("synchronous hook failure"); }
};

void synchronous_hook_errors_are_isolated() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    ThrowingHookBot bot{transport};
    bot.command("good", [](Context ctx) -> task<> { co_await ctx.send("still running"); });
    bot.dispatch(incoming("!good"));
    bot.run();
    CHECK(bot.failures.size() == 1);
    CHECK(bot.failures[0].source == "on_message");
    CHECK(transport->sent().size() == 1);
}

class RemovingCog final : public Cog {
public:
    RemovingCog(Bot& bot, bool& destroyed) : bot_(bot), destroyed_(destroyed) {}
    ~RemovingCog() override { destroyed_ = true; }
    std::vector<Command> commands() override {
        return {{"self_remove", [this](Context ctx) -> task<> {
            CHECK(bot_.remove_command("self_remove"));
            CHECK(!destroyed_);
            co_await sleep_for(1ms);
            CHECK(!destroyed_);
            co_await ctx.send("alive after removal");
        }, {}}};
    }

private:
    Bot& bot_;
    bool& destroyed_;
};

void cog_lifetime_and_inflight_removal() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    bool destroyed = false;
    auto cog = std::make_shared<RemovingCog>(bot, destroyed);
    std::weak_ptr<Cog> weak = cog;
    bot.add_cog(cog);
    cog.reset();
    CHECK(!weak.expired());
    bot.dispatch(incoming("!self_remove"));
    bot.run();
    CHECK(destroyed);
    CHECK(weak.expired());
    CHECK(bot.command_names().empty());
    CHECK(transport->sent().size() == 1);
}

class DuplicateCog final : public Cog {
public:
    std::vector<Command> commands() override {
        auto noop = [](Context) -> task<> { co_return; };
        return {{"temporary", noop, {}}, {"existing", noop, {}}};
    }
};

void registrations_are_validated_and_rolled_back() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};
    auto noop = [](Context) -> task<> { co_return; };
    bot.command("existing", noop, {.aliases = {"alias", "alias2"}});
    throws<std::invalid_argument>([&] { bot.command("new", noop, {.aliases = {"unused", "alias"}}); });
    throws<std::invalid_argument>([&] { bot.command("two words", noop); });
    throws<std::invalid_argument>([&] { bot.command("empty", {}); });
    throws<std::invalid_argument>([&] { bot.command("dup", noop, {.aliases = {"dup"}}); });
    throws<std::invalid_argument>([&] { bot.add_cog(std::make_shared<DuplicateCog>()); });
    CHECK(bot.command_names() == (std::vector<std::string>{"existing"}));
    CHECK(!bot.remove_command("temporary"));
    CHECK(!bot.remove_command("unused"));
    CHECK(bot.remove_command("alias"));
    CHECK(!bot.remove_command("alias2"));
    CHECK(bot.command_names().empty());
    throws<std::invalid_argument>([&] { Bot invalid{transport, {.command_prefix = ""}}; });
    throws<std::invalid_argument>([] { Client invalid{nullptr}; });
}

void listener_snapshots_and_raw_events() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Client client{transport};
    int first = 0;
    int added = 0;
    int ready = 0;
    int raw = 0;
    client.listen_message([&, payload = std::string("capture stays alive")](Message) -> task<> {
        ++first;
        if (first == 1) {
            for (int i = 0; i < 100; ++i) {
                client.listen_message([&](Message) -> task<> { ++added; co_return; });
            }
        }
        co_await sleep_for(1ms);
        CHECK(payload == "capture stays alive");
    });
    client.listen_ready([&](Ready event) -> task<> { CHECK(event.user.bot); ++ready; co_return; });
    client.listen_raw([&](RawEvent event) -> task<> {
        CHECK(event.name == "CUSTOM_EVENT");
        CHECK(event.json == R"({"value":1})");
        CHECK(event.sequence == 9);
        ++raw;
        co_return;
    });
    // dispatch() can be used by an external Gateway thread.
    std::thread producer([&] {
        client.dispatch(incoming("first"));
        client.dispatch(Ready{User{Snowflake{1}, "bot", true}});
        client.dispatch(RawEvent{"CUSTOM_EVENT", R"({"value":1})", 9});
    });
    producer.join();
    client.run();
    CHECK(first == 1);
    CHECK(added == 0);
    CHECK(ready == 1 && raw == 1);
    client.dispatch(incoming("second"));
    client.run();
    CHECK(first == 2);
    CHECK(added == 100);
}

task<> send_from_executor(Client& client) {
    auto sent = co_await client.send_message(Snowflake{42}, "direct");
    co_await sent.reply("bound result");
}

void executor_and_unbound_message() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Client client{transport};
    asio::post(client.executor(), [&] { client.spawn(send_from_executor(client)); });
    client.run();
    CHECK(transport->sent().size() == 2);
    CHECK(transport->sent()[1].reply_to == Snowflake{1});
    // reply() snapshots its Message before returning the lazy task.
    auto work = incoming("unbound").reply("no transport");
    client.spawn(std::move(work));
    throws<std::logic_error>([&] { client.run(); });
}

task<> pause_then_resume(Client& client, int& state) {
    state = 1;
    client.stop();
    co_await sleep_for(1ms);
    state = 2;
}

void stop_pauses_and_run_resumes() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Client client{transport};
    int state = 0;
    client.spawn(pause_then_resume(client, state));
    client.run();
    CHECK(state == 1);
    client.run();
    CHECK(state == 2);
}

task<> message_operations(Client& client, std::shared_ptr<testing::MemoryTransport> transport) {
    auto sent = co_await client.send_message(Snowflake{42}, "original");
    const auto id = sent.id;
    transport->store(sent); // Must not retain a bound transport inside the fixture store.
    { auto unstarted = sent.edit("never executed"); }
    auto original = co_await client.fetch_message(Snowflake{42}, id);
    CHECK(original.content == "original");
    auto pending = original.edit("edited");
    original = Message{};
    auto edited = co_await std::move(pending);
    CHECK(edited.content == "edited" && edited.id == id);
    CHECK(sent.content == "original"); // Models are snapshots, not a hidden shared cache.
    auto refreshed = co_await sent.fetch();
    CHECK(refreshed.content == "edited");
    co_await refreshed.add_reaction("emoji");
    co_await refreshed.remove_reaction("emoji");
    auto edited_again = co_await client.edit_message(Snowflake{42}, id, "again");
    CHECK(edited_again.content == "again");
    co_await client.add_reaction(Snowflake{42}, id, "other");
    co_await client.remove_reaction(Snowflake{42}, id, "other");
    co_await edited_again.delete_message();
    bool missing = false;
    try { co_await refreshed.fetch(); }
    catch (const std::out_of_range&) { missing = true; }
    CHECK(missing);
    CHECK(transport->actions().size() == 9);
    CHECK(transport->actions()[0].kind == testing::MessageActionKind::fetch);
    CHECK(transport->actions()[1].kind == testing::MessageActionKind::edit);
    CHECK(transport->actions().back().kind == testing::MessageActionKind::erase);
    auto retained_snapshot = co_await client.send_message(Snowflake{42}, "stored without transport cycle");
    transport->store(retained_snapshot);
}

void messages_are_owned_lazy_bound_snapshots() {
    std::weak_ptr<testing::MemoryTransport> weak;
    {
        auto transport = std::make_shared<testing::MemoryTransport>();
        weak = transport;
        Client client{transport};
        client.spawn(message_operations(client, transport)); client.run();
        client.spawn(Message{}.edit("unbound"));
        throws<std::logic_error>([&] { client.run(); });
    }
    CHECK(weak.expired());
}

class SendOnlyTransport : public MessageTransport {
public:
    task<Message> send_message(MessageRequest request) override {
        Message result; result.id = Snowflake{1}; result.channel_id = request.channel_id;
        result.content = std::move(request.content); co_return result;
    }
};

task<> legacy_transport_operations(Client& client) {
    auto message = co_await client.send_message(Snowflake{42}, "sent");
    CHECK(message.content == "sent");
    bool unsupported = false;
    try { co_await message.edit("unsupported"); }
    catch (const std::logic_error&) { unsupported = true; }
    CHECK(unsupported);
}

void send_only_custom_transports_remain_usable() {
    auto transport = std::make_shared<SendOnlyTransport>();
    Client client{transport};
    client.spawn(legacy_transport_operations(client)); client.run();
}

} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests{
        {"snowflakes and intents", snowflakes_and_intents},
        {"argument parser", argument_parser},
        {"command dispatch and concurrency", command_dispatch_and_concurrency},
        {"error isolation", callback_and_parse_errors_are_isolated},
        {"default error visibility", default_errors_are_visible},
        {"synchronous hook error isolation", synchronous_hook_errors_are_isolated},
        {"Cog lifetime during command removal", cog_lifetime_and_inflight_removal},
        {"registration validation and rollback", registrations_are_validated_and_rolled_back},
        {"listener snapshots and raw events", listener_snapshots_and_raw_events},
        {"executor and unbound message", executor_and_unbound_message},
        {"stop and resume", stop_pauses_and_run_resumes},
        {"owned lazy message operations", messages_are_owned_lazy_bound_snapshots},
        {"send-only custom transport compatibility", send_only_custom_transports_remain_usable}
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
