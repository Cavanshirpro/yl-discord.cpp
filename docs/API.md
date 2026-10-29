# API reference — v0.4.0

Include `<yl/discord.hpp>` and use namespace `yl::discord`. Generated headers
come from CMake; link the `yl::discord` target. The testing transport has a
separate include: `<yl/discord/testing/memory_transport.hpp>`.

## Coroutines

`task<T = void>` aliases `asio::awaitable<T>`. Use `co_await`/`co_return` and take
events/Context by value in coroutine callbacks. `sleep_for(duration)` awaits an
Asio steady timer. Tasks are lazy; dropping an unstarted task does not perform
the operation. `Client::spawn(task, source)` runs it and discards any return value.
Blocking functions remain blocking; see [ownership rules](ARCHITECTURE.md).
`is_cancelled(asio::error_code)` recognizes operation_aborted across DLL category instances.

## Client

Construct `Client(ClientOptions = {})` for built-in networking, or
`Client(std::shared_ptr<MessageTransport>, ClientOptions = {})` for a custom/offline
transport. Null custom transport is rejected. Copying is disabled.

| Method | Behavior |
| --- | --- |
| `listen_message(handler)` | Add a `task<>(Message)` callback; returns Client& |
| `listen_ready(handler)` | Add a `task<>(Ready)` callback |
| `listen_raw(handler)` | Add a `task<>(RawEvent)` callback |
| `subscribe_message/ready/raw(handler)` | Return an explicit ListenerConnection |
| `wait_for<Event>(timeout, predicate, stop_token)` | Await a future Message, Ready or RawEvent |
| `dispatch(Message/Ready/RawEvent)` | Thread-safe queued event ingress |
| `run()` | Drain work/timers; throw first unhandled error after draining |
| `run(bot_token)` | Start the Gateway and run until close/fatal failure |
| `start(bot_token)` | Lazy live-session task on this Client's executor |
| `close()` | Thread-safe network shutdown, including pending REST waits |
| `rest()` | Built-in RestClient; custom-transport Clients reject this |
| `stop()` | Pause the event loop without cancelling tasks |
| `spawn(task, source = "background")` | Start work on this Client's executor |
| `send_message(channel_id, content)` | Return a lazy `task<Message>` |
| `fetch_message(channel_id, message_id)` | Fetch a bound message snapshot |
| `edit_message(channel_id, message_id, content)` | Return an updated bound snapshot |
| `delete_message(channel_id, message_id)` | Delete a message |
| `add_reaction(channel_id, message_id, emoji)` | Add this bot's reaction |
| `remove_reaction(channel_id, message_id, emoji)` | Remove this bot's reaction |
| `executor()` | Access `asio::any_io_executor` for advanced integration |
| `options()` | Read configured ClientOptions |
| `take_errors()` | Remove/return additional unhandled TaskErrors |

Override `task<> on_ready(Ready)`, `on_message(Message)`,
`on_raw_event(RawEvent)` and `on_error(TaskError)` in subclasses. The first three
default to no-ops. The default error hook logs the source/error to stderr and
rethrows its exception; if a custom
hook also throws, its failure is retained rather than recursively reported.

`TaskError` has `source` and `std::exception_ptr exception`. Sources currently
identify an event hook/listener, `commands`, or the caller-provided spawn label.
Command errors can first be handled with Bot::on_command_error(Context, exception_ptr).
ListenerConnection supports explicit disconnect; see the [event guide](EVENTS.md).
In a live session, an unhandled event/command error is logged immediately and
retained until `run()` drains after shutdown. Override `on_error()` to handle it
or call `close()` there for fail-fast behavior.

Mutate registries/options before `run()` or on its executor. Only `dispatch()`
and `close()`/`stop()` are intended for cross-thread entry. Never call `run()` concurrently
or recursively. Hook/listener/command completion order is unspecified.

## Bot, commands and Context

`Bot(BotOptions = {})` uses built-in networking; `Bot(transport, BotOptions = {})`
uses a custom transport. Both add a command prefix (default `!`) and intents.
Prefix must be nonempty and contain no whitespace. Names/aliases are
case-sensitive and cannot contain whitespace, quotes or backslashes.

```cpp
bot.command("echo", [](Context ctx) -> task<> {
    if (!ctx.args.empty()) {
        co_await ctx.send(ctx.args.front());
    }
}, {.aliases = {"say"}, .description = "Send the first argument"});
```

`command(name, callback, CommandOptions = {})` returns Bot&. CommandOptions holds
`aliases`, `description`, `usage`, `hidden`, asynchronous `checks` and optional
`cooldown`. `command<Args...>(name, callback, options)` binds owning typed parameters
after Context, with trailing std::optional parameters supported. Empty callbacks or duplicate names/aliases throw
`std::invalid_argument`. `command_names()` returns sorted canonical names.
`remove_command(name_or_alias)` removes every alias of that command and returns
whether a command was found. Already-running commands can finish.

Context owns `message`, canonical `command_name`, `invoked_with` (the name/alias used)
and parsed `args` (excluding the name). `argument<T>(index)` converts a token.
`send(content)` sends to that message's channel; `reply(content)` also
sets its message reference. Both return a bound `task<Message>` result.

Overriding `on_message()` does not disable commands. Bot-authored messages still
reach hooks/listeners but never invoke commands. Unknown names are ignored.
`parse_arguments(text)` splits whitespace, supports single/double quotes,
preserves quoted empty arguments and allows backslash escaping outside single
quotes. It does not expand variables, globs or shell commands. A trailing
escape or unclosed quote throws `ArgumentError`.

`add_check()` / `clear_checks()` manage global asynchronous checks. Per-command
checks run next; cooldown admission follows successful checks. Override
`on_command_error(Context, exception_ptr)` for invocation-aware failures. The
default forwards the original exception to Client's error handling.

`get_command(name_or_alias)` returns an optional CommandInfo metadata snapshot;
`help_text(name = {})` formats usage/aliases/descriptions. `add_help_command()`
registers opt-in, paginated help. `reset_cooldown(name, context)` clears one bucket.
See the [command guide](COMMANDS.md) for converters, error classes, cooldown scopes,
capacity bounds and complete examples.

## Cog

Derive from `Cog` and implement `std::vector<Command> commands()`. Each Command
has `name`, `callback` and `options`. Pass a shared instance to `bot.add_cog(cog)`.
Registered and running entries retain the Cog, so callbacks may capture `this`.
A conflicting definition rolls back that Cog's registrations.
`remove_cog(shared_ptr<Cog>)` removes all its current commands/aliases while allowing
in-flight work to finish. Lifecycle hooks and managed tasks are future work.
See the complete [class example](../examples/class_bot.cpp).

## Models

- **Snowflake:** explicit `uint64_t` or decimal `string_view` construction;
  `value()`, `str()`, comparisons and `created_at()` in UTC milliseconds. Empty,
  malformed, negative or overflowing decimal strings throw `std::invalid_argument`.
  The default numeric value is zero; it is not proof of a real Discord object.
- **User:** `id`, `name`, `bot`.
- **Message:** `id`, `channel_id`, optional `guild_id`, `author`, `content`, `embeds` and
  `reply(content)`, `fetch()`, `edit(content)`, `delete_message()`, `add_reaction(emoji)`
  and `remove_reaction(emoji)`. Operations need a transport bound through Client.
  Edits return new snapshots; they do not mutate older values. See [message operations](MESSAGES.md).
- **Embed / MessageOptions / MessageEditOptions / AllowedMentions:** owned rich
  fields, typed outgoing validation and explicit mention control. All send/reply
  methods accept MessageOptions; edit methods accept MessageEditOptions. See [embeds](EMBEDS.md).
- **FileUpload / Attachment:** immutable shared request bytes and owned response
  metadata. MessageOptions::files uploads bytes; MessageEditOptions adds files and
  explicit retained_attachment_ids. Message::attachments contains snapshots. See [files](FILES.md).
- **Ready:** `user`. Dispatching Ready does not authenticate a session.
- **RawEvent:** `name`, owned `json`, optional `sequence`. Built-in Gateway dispatches
  provide the full JSON envelope alongside supported typed events. Custom raw
  dispatch preserves the supplied JSON and does not synthesize typed events.
- **Intents:** `defaults()`, `with(Intent)`, `contains(Intent)`, `value()`.
  Defaults include only guilds, guild messages and direct messages; this is not
  the full discord.py default set. Supported enum names also include guild
  members, guild presences and message content. They are configuration data,
  passed to Gateway Identify by built-in live connections. Privileged intents
  also require enabling in the Discord Developer Portal.

## Message transport and test support

Implement `MessageTransport::send_message(MessageRequest) -> task<Message>` for an
alternative transport. MessageRequest owns `channel_id`, `content` and optional
`reply_to`, `embeds` and `allowed_mentions`. An implementation must suspend for I/O instead of blocking the loop.
Error reporting uses normal exceptions, propagated through the caller's task.
Optional virtual fetch/edit/delete/reaction methods default to lazy unsupported
operation errors, so send-only custom transports remain source-compatible.

`testing::MemoryTransport(User self = ...)` records requests and returns
synthetic messages. `sent()` returns a const reference to records. Access those
records on the event-loop thread or after `run()` returns. It makes no network
calls and does not emulate Discord's validation or rate limits.
It also stores message snapshots, implements fetch/edit/delete/reaction operations,
accepts seeded fixtures with `store(Message)` and records these in `actions()`.
Its stored messages never retain a bound transport.

## Version information

`yl::discord::version` is a constexpr string view. `YL_DISCORD_VERSION` and
`YL_DISCORD_VERSION_MAJOR/MINOR/PATCH` macros are generated from [VERSION](../VERSION).
The public API remains experimental; see [versioning](VERSIONING.md).

## Live transport

ClientOptions/BotOptions have a `connection` field containing ConnectionOptions:
REST base URL (Discord API v10), request/connect timeouts (30/15 seconds),
reconnect delay (one second), reconnect budget (8), 429 retry budget (5) and
decoded response limit (8 MiB). Reconnect delay must be within 0–30 seconds.
HTTPS/WSS are required except for loopback/localhost fixture URLs. TLS verification
cannot be disabled through this API.

`RestClient(executor, options)` also works independently. Call
`authenticate(raw_bot_token)` before its first request. `request(method, route,
json_body = {})` returns `task<HttpResponse>` with status, lowercase headers and
body. Routes are API-relative; methods are GET/POST/PUT/PATCH/DELETE. All requests
share route/bucket/global rate-limit handling. Authentication cannot change after
requests begin; 401 prevents further requests with that client. Only 429 is
automatically retried, not potentially committed POSTs after network failures.

`HttpError` exposes `status()` and `discord_code()` without response contents or
credentials in `what()`. `TransportError` reports I/O/deadline failures;
`ProtocolError` reports invalid payloads, fatal Gateway close codes or session
limits. Outgoing message mentions are disabled by default.

`token_from_env()` reads DISCORD_BOT_TOKEN; pass another variable name if needed.
Use a raw bot token, without the `Bot ` HTTP prefix. `run(token)` authenticates
the Client's RestClient automatically before Gateway discovery.

A live Client starts once and reconnects internally, retaining sequence/session/
resume URL. Construct a new Client after close. Sessions healthy for at least
60 seconds reset the reconnect budget. Fatal close codes do not retry; invalid
sessions re-identify after a randomized protocol delay. Compression/sharding
are not implemented. Raw events contain the full dispatch JSON envelope. Resume
does not synthesize Ready; repeated sequence numbers are ignored.

`close()` cancels network work and event waits, not application-owned timers, signals or unrelated
tasks. Cancel those yourself. Network cancellation during close is not an
unhandled task error. `stop()` retains pause/resume semantics. See [live setup](LIVE_BOTS.md).
