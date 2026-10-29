# Commands

Prefix commands run on the Bot's single event-loop executor. They work with live
Gateway messages and with the offline MemoryTransport. Names are case-sensitive;
arguments support quoting and escaping. User messages invoke commands independently
of `on_message()` overrides. Messages authored by bots never invoke commands.

## Typed parameters

```cpp
bot.command<int, int>("add", [](Context ctx, int left, int right) -> task<> {
    co_await ctx.reply(std::to_string(static_cast<long long>(left) + right));
}, {.aliases = {"sum"}, .description = "Add two integers"});

bot.command<std::string, std::optional<bool>>("echo",
    [](Context ctx, std::string text, std::optional<bool> loud) -> task<> {
        co_await ctx.send(text + (loud.value_or(false) ? "!" : ""));
    });
```

`!sum 20 22` supplies two integers. `!echo "two words"` leaves the optional bool
empty; `!echo "two words" yes` supplies true. Typed commands reject extra arguments,
missing required arguments, partial numeric parses, overflow and nonfinite floats.
Optional parameters must appear last. Parameters and Context must be owning values,
so their storage survives coroutine suspension. No implicit HTTP lookup is involved.

Supported built-in converters:

| Type | Input |
| --- | --- |
| `std::string` | One parsed token, including a quoted empty string |
| Standard integer types | A complete base-10 value within the target type's range |
| `float`, `double`, `long double` | A complete finite value; exponent notation is accepted |
| `bool` | Lowercase `true/false`, `yes/no`, `on/off`, `1/0` |
| `Snowflake` | Decimal ID or user/role/channel mention |
| `std::optional<T>` | A trailing value converted as T, or nullopt if absent |

Numeric conversion does not accept a leading `+`, whitespace or numeric separators.
String and ID conversion do not resolve account names or validate that an object exists.
Application types can specialize `convert_argument<MyType>(std::string_view)`;
throw `BadArgument` for rejected input. Context also exposes `argument<T>(index)`
for manual commands, with zero-based indices. Raw `ctx.args` always stays available.

The original `command(name, task<>(Context), options)` accepts arbitrary argument
counts for applications that want their own parsing. Typed commands generate a
usage string such as `<integer> <text> [bool]`; set `CommandOptions::usage` to override it.

## Checks

```cpp
bot.add_check(checks::user(Snowflake{123})); // Applies to every command.
bot.command("guild", [](Context ctx) -> task<> {
    co_await ctx.reply("This message belongs to a guild.");
}, {.checks = {checks::guild_only()}});
```

`CommandCheck` is `std::function<task<bool>(Context)>`. Global checks run first,
then the command's checks, in registration order. Return false to raise
`CheckFailure`; an exception also aborts that invocation. Checks may await I/O.
`checks::dm_only()` and `checks::user(id)` are also provided. These are local message
checks, not Discord permission or guild-owner lookups.

`clear_checks()` affects future invocations. An invocation snapshots the global
check list and retains callbacks across awaits, even if a check removes itself,
its command or its Cog. All checks must succeed before consuming a cooldown.

## Cooldowns

```cpp
using namespace std::chrono_literals;
bot.command("ping", [](Context ctx) -> task<> {
    co_await ctx.reply("Pong!");
}, {.cooldown = Cooldown{.rate = 2, .per = 5s, .scope = CooldownScope::user}});
```

This allows two starts per user in a sliding five-second window. Aliases share
the same counter. Scopes are `global`, `user`, `channel` and `guild`; guild scope
uses a separate per-channel bucket in DMs. Counters use the monotonic steady clock.
Concurrent invocations consume a slot atomically on the event loop before the
callback's first suspension. A failing callback, including typed conversion,
still consumes its slot. Check rejection does not consume a slot.

`CommandOnCooldown::retry_after()` returns a steady-clock duration until the next
slot. Rates and bucket capacities must be positive; the period must be positive
and at most one year. `max_buckets` defaults to 4096. When full, expired buckets
are reclaimed; active buckets are never evicted to admit new keys. If no key can
be admitted, `CooldownCapacityExceeded` fails the invocation. This bounds the
number of remembered keys without allowing eviction to bypass a cooldown.

`reset_cooldown(command_or_alias, context)` removes the bucket selected by that
Context and returns whether it existed. Removing/re-registering a command creates
fresh cooldown state. Changes to the registry, checks and cooldowns belong on the
Bot executor or before `run()`, just like command registration.

## Command errors

Override `task<> on_command_error(Context, std::exception_ptr)` to handle failures
with their original message, canonical command name, `invoked_with` alias and args.
The default rethrows to the existing `Client::on_error(TaskError)` path; it does
not silently swallow programming errors. A parsing failure before command lookup
can have an empty command name. Unknown well-formed command names are ignored.

The exception hierarchy is `CommandError` → `ArgumentError` → `BadArgument`,
`MissingRequiredArgument` and `TooManyArguments`. `CheckFailure`,
`CommandOnCooldown` and `CooldownCapacityExceeded` also derive from CommandError.
`MissingRequiredArgument::index()` is zero-based. User callback exceptions retain
their original type. A throwing error handler is reported once by Client; there
is no recursive command-error loop.

C++ does not allow `co_await` inside a catch handler. Save the error message in
the handler, then await `ctx.reply()` after leaving the catch block. See the
complete [command_bot.cpp](../examples/command_bot.cpp) example, which also forwards
unrecognized errors instead of hiding them.

## Help and metadata

`get_command(name_or_alias)` returns an optional owned CommandInfo snapshot.
`command_names()` returns sorted canonical names, including hidden commands.
`help_text()` lists visible commands in name order; `help_text(name_or_alias)`
includes its usage, description and aliases. A hidden command is omitted from the
list but still has directly requested help. Help does not execute checks and is
not an access-control boundary.

Call `add_help_command()` to register `!help` / `!help command`. It is opt-in, so
applications can keep their own help implementation. Name collisions are rejected
normally. Built-in help splits long text into at most 1900 bytes per message,
preserving UTF-8 boundaries for valid text. It uses the same transport, mention
defaults and rate-limit queue as other sends. A custom transport must enforce
its own mention policy if needed.

## Cog removal

`remove_cog(shared_ptr<Cog>)` removes that instance's currently registered commands
and all their aliases. It returns false if none were registered. Commands already
in progress retain their command, checks and Cog owner until completion. An
external shared_ptr also keeps its Cog alive; release it to allow destruction.
There are no automatic load/unload hooks or managed background-task cancellation.
