// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 Cavanshirpro
// Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>
#include <iostream>
#include <limits>

using namespace yl::discord;
using namespace std::chrono_literals;

// Applications can supply their own owning argument types without modifying Bot.
struct Positive { int value; };
template <> Positive yl::discord::convert_argument<Positive>(std::string_view text) {
    const auto value = convert_argument<int>(text);
    if (value <= 0) throw BadArgument("Expected a positive integer");
    return {value};
}

namespace {
void check(bool result, const char* expression) { if (!result) throw std::runtime_error(expression); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
template <typename Error, typename Fn> void throws(Fn fn) {
    bool caught = false;
    try { fn(); } catch (const Error&) { caught = true; }
    CHECK(caught);
}
Message incoming(std::string content, std::uint64_t user = 7, std::uint64_t channel = 42,
                 std::optional<Snowflake> guild = std::nullopt) {
    Message result;
    result.id = Snowflake{100}; result.channel_id = Snowflake{channel};
    result.author = User{Snowflake{user}, "tester", false};
    result.guild_id = guild; result.content = std::move(content);
    return result;
}
class RecordingBot : public Bot {
public:
    using Bot::Bot;
    std::vector<Context> contexts;
    std::vector<std::exception_ptr> errors;
    task<> on_command_error(Context context, std::exception_ptr error) override {
        contexts.push_back(std::move(context)); errors.push_back(error); co_return;
    }
    template <typename Error> void last_error() {
        CHECK(!errors.empty());
        throws<Error>([&] { std::rethrow_exception(errors.back()); });
    }
    void invoke(Message message) { dispatch(std::move(message)); run(); }
};

void converters_reject_ambiguous_and_overflowing_input() {
    CHECK(convert_argument<int>("-42") == -42);
    CHECK(convert_argument<unsigned>("42") == 42);
    CHECK(convert_argument<std::uint64_t>("18446744073709551615") == std::numeric_limits<std::uint64_t>::max());
    CHECK(convert_argument<double>("1.25e2") == 125.0);
    CHECK(convert_argument<bool>("yes") && !convert_argument<bool>("off"));
    CHECK(convert_argument<std::string>("").empty());
    for (auto text : {"", "1x", " 1", "+1", "2147483648", "-2147483649"})
        throws<BadArgument>([&] { (void)convert_argument<int>(text); });
    throws<BadArgument>([] { (void)convert_argument<unsigned>("-1"); });
    throws<BadArgument>([] { (void)convert_argument<double>("nan"); });
    throws<BadArgument>([] { (void)convert_argument<float>("inf"); });
    throws<BadArgument>([] { (void)convert_argument<bool>("perhaps"); });
    for (auto text : {"42", "<@42>", "<@!42>", "<@&42>", "<#42>"}) CHECK(convert_argument<Snowflake>(text) == Snowflake{42});
    for (auto text : {"<@>", "<@!!42>", "<@42>tail", "<@+42>", "-42"})
        throws<BadArgument>([&] { (void)convert_argument<Snowflake>(text); });
}

void typed_commands_preserve_context_and_strict_arity() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    bot.command<int, std::string, std::optional<bool>>("typed", [](Context context, int number, std::string text, std::optional<bool> flag) -> task<> {
        CHECK(context.command_name == "typed" && context.invoked_with == "t");
        CHECK(number == 42 && text == "two words");
        co_await sleep_for(1ms);
        co_await context.reply(flag ? (*flag ? "true" : "false") : "omitted");
    }, {.aliases = {"t"}});
    bot.invoke(incoming("!t 42 \"two words\""));
    bot.invoke(incoming("!t 42 \"two words\" false"));
    CHECK(transport->sent().size() == 2 && transport->sent()[0].content == "omitted" && transport->sent()[1].content == "false");
    bot.invoke(incoming("!t")); bot.last_error<MissingRequiredArgument>();
    CHECK(bot.contexts.back().command_name == "typed");
    bot.invoke(incoming("!t nope text")); bot.last_error<BadArgument>();
    bot.invoke(incoming("!t 42 text true extra")); bot.last_error<TooManyArguments>();
    bot.invoke(incoming("!t 42 text maybe")); bot.last_error<BadArgument>();
    CHECK(transport->sent().size() == 2);
    CHECK(bot.get_command("t")->usage == "<integer> <text> [bool]");
    bot.command<Positive>("custom", [](Context ctx, Positive value) -> task<> { co_await ctx.send(std::to_string(value.value)); });
    bot.invoke(incoming("!custom 3"));
    CHECK(transport->sent().back().content == "3");
    bot.invoke(incoming("!custom 0")); bot.last_error<BadArgument>();
    Context context;
    CHECK(!context.argument<std::optional<int>>(0));
    throws<MissingRequiredArgument>([&] { (void)context.argument<int>(0); });
}

void checks_suspend_and_keep_snapshots_alive() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    int global = 0, local = 0, calls = 0;
    bool retained = false;
    auto marker = std::make_shared<int>(42);
    std::weak_ptr<int> weak = marker;
    bot.add_check([&, marker](Context) -> task<bool> {
        ++global;
        bot.clear_checks();
        CHECK(bot.remove_command("checked"));
        co_await sleep_for(1ms);
        retained = *marker == 42;
        co_return true;
    });
    marker.reset();
    bot.command("checked", [&](Context) -> task<> { ++calls; co_return; },
        {.checks = {[&](Context) -> task<bool> { ++local; co_return true; }}});
    bot.invoke(incoming("!checked"));
    CHECK(global == 1 && local == 1 && calls == 1 && retained && weak.expired());
    bot.add_check(checks::user(Snowflake{7}));
    bot.command("guild", [&](Context) -> task<> { ++calls; co_return; }, {.checks = {checks::guild_only()}});
    bot.command("dm", [&](Context) -> task<> { ++calls; co_return; }, {.checks = {checks::dm_only()}});
    bot.invoke(incoming("!guild")); bot.last_error<CheckFailure>();
    bot.invoke(incoming("!guild", 8, 42, Snowflake{10})); bot.last_error<CheckFailure>();
    bot.invoke(incoming("!guild", 7, 42, Snowflake{10}));
    bot.invoke(incoming("!dm"));
    bot.invoke(incoming("!dm", 7, 42, Snowflake{10})); bot.last_error<CheckFailure>();
    CHECK(calls == 3);
    throws<std::invalid_argument>([&] { bot.add_check({}); });
    throws<std::invalid_argument>([&] { bot.command("bad", [](Context) -> task<> { co_return; }, {.checks = {CommandCheck{}}}); });
}

void cooldowns_share_aliases_and_bound_buckets() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    int calls = 0;
    bot.command("limited", [&](Context) -> task<> { ++calls; co_return; },
        {.aliases = {"l"}, .cooldown = Cooldown{.rate = 2, .per = 10s, .max_buckets = 2}});
    bot.invoke(incoming("!limited")); bot.invoke(incoming("!l")); bot.invoke(incoming("!l"));
    bot.last_error<CommandOnCooldown>();
    try { std::rethrow_exception(bot.errors.back()); }
    catch (const CommandOnCooldown& error) { CHECK(error.retry_after() > 0s && error.retry_after() <= 10s); }
    bot.invoke(incoming("!l", 8)); CHECK(calls == 3);
    bot.invoke(incoming("!l", 9)); bot.last_error<CooldownCapacityExceeded>(); CHECK(calls == 3);
    Context context; context.message = incoming("", 7);
    CHECK(bot.reset_cooldown("l", context)); CHECK(!bot.reset_cooldown("l", context));
    bot.invoke(incoming("!l", 9)); CHECK(calls == 4);
    CHECK(bot.remove_command("l"));
    bot.command("limited", [&](Context) -> task<> { ++calls; co_return; }, {.cooldown = Cooldown{.per = 10s}});
    bot.invoke(incoming("!limited")); CHECK(calls == 5);
    for (auto cooldown : {Cooldown{.rate = 0}, Cooldown{.per = 0ms}, Cooldown{.max_buckets = 0},
                          Cooldown{.scope = static_cast<CooldownScope>(99)}})
        throws<std::invalid_argument>([&] { bot.command("invalid", [](Context) -> task<> { co_return; }, {.cooldown = cooldown}); });
}

void cooldown_scopes_and_expiry() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    int calls = 0;
    for (const auto& [name, scope] : {std::pair{"global", CooldownScope::global}, {"channel", CooldownScope::channel}, {"guild", CooldownScope::guild}})
        bot.command(name, [&](Context) -> task<> { ++calls; co_return; }, {.cooldown = Cooldown{.per = 10s, .scope = scope}});
    bot.invoke(incoming("!global", 7, 42)); bot.invoke(incoming("!global", 8, 43)); bot.last_error<CommandOnCooldown>();
    bot.invoke(incoming("!channel", 7, 42)); bot.invoke(incoming("!channel", 8, 42)); bot.last_error<CommandOnCooldown>();
    bot.invoke(incoming("!channel", 8, 43));
    bot.invoke(incoming("!guild", 7, 42, Snowflake{10})); bot.invoke(incoming("!guild", 8, 43, Snowflake{10})); bot.last_error<CommandOnCooldown>();
    bot.invoke(incoming("!guild", 7, 42)); // DM scope is separate from guild.
    CHECK(calls == 5);
    bot.command("expiry", [&](Context) -> task<> { ++calls; co_return; }, {.cooldown = Cooldown{.per = 5ms, .max_buckets = 1}});
    bot.invoke(incoming("!expiry"));
    bot.spawn(sleep_for(20ms)); bot.run();
    bot.invoke(incoming("!expiry", 8)); CHECK(calls == 7); // Expired bucket is reclaimed, not active eviction.
    CHECK(bot.errors.size() == 3);
}

void rejected_checks_do_not_consume_cooldowns() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    int calls = 0;
    bot.command("g", [&](Context) -> task<> { ++calls; co_return; },
        {.checks = {checks::guild_only()}, .cooldown = Cooldown{.per = 10s}});
    bot.invoke(incoming("!g")); bot.last_error<CheckFailure>();
    bot.invoke(incoming("!g", 7, 42, Snowflake{10})); CHECK(calls == 1);
    bot.invoke(incoming("!g", 7, 42, Snowflake{10})); bot.last_error<CommandOnCooldown>();
}

void concurrent_invocations_cannot_bypass_cooldowns() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    int started = 0, finished = 0;
    bot.command("race", [&](Context ctx) -> task<> {
        ++started;
        co_await sleep_for(5ms);
        ++finished;
        co_await ctx.send("done");
    }, {.aliases = {"r"}, .checks = {[](Context) -> task<bool> { co_await sleep_for(1ms); co_return true; }},
        .cooldown = Cooldown{.rate = 3, .per = 10s}});
    for (int i = 0; i < 20; ++i) bot.dispatch(incoming(i % 2 ? "!race" : "!r"));
    bot.run();
    CHECK(started == 3 && finished == 3 && transport->sent().size() == 3 && bot.errors.size() == 17);
    for (auto error : bot.errors) throws<CommandOnCooldown>([&] { std::rethrow_exception(error); });
}

class ErrorReplyBot : public Bot {
public:
    using Bot::Bot;
    task<> on_command_error(Context ctx, std::exception_ptr) override {
        co_await sleep_for(1ms);
        co_await ctx.reply(ctx.invoked_with + ":" + ctx.args.at(0));
    }
};

class ThrowingErrorBot : public Bot {
public:
    using Bot::Bot;
    int calls = 0;
    task<> on_command_error(Context, std::exception_ptr) override {
        ++calls;
        throw std::logic_error("Error hook failure");
        co_return;
    }
};

void command_error_hooks_can_await_and_fail_without_recursion() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    ErrorReplyBot bot{transport};
    bot.command<int>("number", [](Context, int) -> task<> { co_return; }, {.aliases = {"n"}});
    bot.dispatch(incoming("!n invalid")); bot.run();
    CHECK(transport->sent().size() == 1 && transport->sent().back().content == "n:invalid");
    ThrowingErrorBot throwing{transport};
    throwing.command("error", [](Context) -> task<> { throw std::runtime_error("original"); co_return; });
    throwing.dispatch(incoming("!error"));
    throws<std::logic_error>([&] { throwing.run(); });
    CHECK(throwing.calls == 1 && throwing.take_errors().empty());
}

void help_uses_metadata_and_paginates_utf8() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    auto noop = [](Context) -> task<> { co_return; };
    bot.command("zebra", noop, {.aliases = {"z"}, .description = "Last"});
    bot.command("secret", noop, {.description = "Hidden", .hidden = true});
    bot.command<int>("alpha", [](Context, int) -> task<> { co_return; }, {.description = "First"});
    CHECK(bot.help_text().find("!alpha <integer> - First") < bot.help_text().find("!zebra"));
    CHECK(bot.help_text().find("secret") == std::string::npos);
    CHECK(bot.help_text("z").find("Aliases: !z") != std::string::npos);
    CHECK(bot.help_text("secret").find("Hidden") != std::string::npos);
    CHECK(bot.help_text("unknown") == "Unknown command.");
    CHECK(!bot.get_command("unknown"));
    auto snapshot = bot.get_command("z"); bot.remove_command("z"); CHECK(snapshot->description == "Last");
    std::string description;
    for (int i = 0; i < 1600; ++i) description += "\xf0\x9f\x98\x80";
    bot.command("long", noop, {.description = description});
    bot.add_help_command();
    bot.invoke(incoming("!help long"));
    std::string joined;
    for (const auto& sent : transport->sent()) {
        CHECK(!sent.content.empty() && sent.content.size() <= 1900);
        CHECK((static_cast<unsigned char>(sent.content.front()) & 0xc0) != 0x80);
        joined += sent.content;
    }
    CHECK(joined == bot.help_text("long") && transport->sent().size() > 1);
    bot.invoke(incoming("!help a b")); bot.last_error<TooManyArguments>();
    throws<std::invalid_argument>([&] { bot.add_help_command(); });
}

class UnloadingCog : public Cog {
public:
    UnloadingCog(Bot& bot, bool& destroyed) : bot_(bot), destroyed_(destroyed) {}
    ~UnloadingCog() override { destroyed_ = true; }
    std::vector<Command> commands() override {
        return {{"first", [this](Context ctx) -> task<> {
            CHECK(bot_.remove_cog(owner.lock()));
            CHECK(bot_.command_names().empty());
            co_await sleep_for(1ms);
            CHECK(!destroyed_);
            co_await ctx.reply("retained");
        }, {.aliases = {"alias"}}}, {"second", [](Context) -> task<> { co_return; }, {}}};
    }
    std::weak_ptr<Cog> owner;
private:
    Bot& bot_; bool& destroyed_;
};

void cog_unload_retains_inflight_captures() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    RecordingBot bot{transport};
    bool destroyed = false;
    auto cog = std::make_shared<UnloadingCog>(bot, destroyed);
    cog->owner = cog; bot.add_cog(cog); cog.reset();
    bot.invoke(incoming("!alias"));
    CHECK(destroyed && transport->sent().back().content == "retained");
    CHECK(!bot.remove_cog({}));
}
} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> cases{
        {"converters", converters_reject_ambiguous_and_overflowing_input},
        {"typed command arity and Context", typed_commands_preserve_context_and_strict_arity},
        {"async check ownership", checks_suspend_and_keep_snapshots_alive},
        {"cooldown aliases and capacity", cooldowns_share_aliases_and_bound_buckets},
        {"cooldown scopes and expiry", cooldown_scopes_and_expiry},
        {"checks before cooldown", rejected_checks_do_not_consume_cooldowns},
        {"concurrent cooldown admission", concurrent_invocations_cannot_bypass_cooldowns},
        {"command error hook suspension and failure", command_error_hooks_can_await_and_fail_without_recursion},
        {"help metadata and UTF-8 pagination", help_uses_metadata_and_paginates_utf8},
        {"Cog unload ownership", cog_unload_retains_inflight_captures}
    };
    int failures = 0;
    for (const auto& [name, test] : cases) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
