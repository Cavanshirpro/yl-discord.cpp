# Message operations

Client and bound Message snapshots expose awaitable helpers for sending, fetching,
editing, deleting and managing the current bot's reactions. Every live helper uses
the same REST client, rate-limit queue, TLS validation, deadlines and cancellation.

```cpp
bot.command("status", [](Context ctx) -> task<> {
    auto status = co_await ctx.send("Working...");
    co_await sleep_for(std::chrono::milliseconds(100));
    auto updated = co_await status.edit("Done!");
    co_await updated.add_reaction("\xf0\x9f\x91\x8d"); // 👍 as UTF-8
});
```

| Bound Message method | Result |
| --- | --- |
| `reply(content)` | A new bound Message with a reference to this message |
| `fetch()` | A newly fetched bound Message snapshot |
| `edit(content)` | The updated bound Message returned by Discord |
| `delete_message()` | `task<>`; delete this message |
| `add_reaction(emoji)` | `task<>`; add this bot's reaction |
| `remove_reaction(emoji)` | `task<>`; remove only this bot's reaction |

Client also exposes `fetch_message(channel, message)`,
`edit_message(channel, message, content)`, `delete_message(channel, message)`,
`add_reaction(channel, message, emoji)` and `remove_reaction(channel, message, emoji)`.
Use these when you have IDs rather than a previously received Message. The built-in
RestClient provides the same ID-based operations; its raw return values are unbound,
so use Client if you want convenient chained Message methods.

## Snapshots and lazy tasks

Message values are owned snapshots. `edit()` returns a new value; it does not
mutate older copies. Fetch explicitly to refresh a snapshot. No hidden message cache
is updated. Calling an operation captures a snapshot and its transport before
returning its lazy task, so it is safe to await later after the original Message
has been replaced or destroyed. Dropping an unstarted task performs no operation.
Await tasks on the Client's executor while the Client and event loop are alive.

Incoming Message events and values returned by Client/Context are bound to their
transport. A manually constructed Message is unbound; awaiting an operation on it
throws `std::logic_error`. It is not authenticated just because it contains IDs.

## Live behavior

Content edits always include `allowed_mentions` with an empty parse list and
`replied_user: false`, matching new messages. Discord does not inherit the create
request's mention policy during editing. Content-only edits omit attachments,
embeds, components and flags so they remain unchanged. Empty content is sent to
Discord, which decides whether other message content makes that edit valid.

Emoji input is raw UTF-8, `name:id`, `<:name:id>` or `<a:name:id>`. The helper
URL-encodes it as a single path component; do not pre-encode it. Empty values,
ASCII controls/whitespace, path-marker values and inputs over 256 bytes are
rejected locally. This is structural validation; Discord still decides whether
the emoji exists and whether this bot can use it. Animated markup is normalized
to the same REST `name:id` format. Reactions return no message body.

Permissions and ownership are enforced by Discord. Editing content generally
requires the original author; deleting another user's guild message requires
Manage Messages. Reaction creation requires Read Message History, and adding a
new reaction also requires Add Reactions. Missing permissions or deleted messages
raise `HttpError`; `status()` and `discord_code()` remain available. Malformed
successful message responses raise `ProtocolError` with a generic diagnostic.
These helpers do not request extra permissions or retry uncertain writes after
transport failures. Follow the [Discord message reference](https://docs.discord.com/developers/resources/message)
for the current endpoint rules.

## Custom and offline transports

MessageTransport keeps `send_message()` as its only pure virtual method. Existing
custom transports compile without implementing the new operations; their defaults
throw a lazy `std::logic_error` describing the unsupported operation. Override the
ID-based methods to support more functionality. Rebuild consumers for the new
minor version because virtual interfaces and object layouts have changed.

MemoryTransport stores snapshots of sent messages and implements all five
operations. Use `store(Message)` to seed incoming fixtures without recording a
send. Seeded values never retain their bound transport, avoiding ownership cycles.
`actions()` records successful fetch/edit/delete/reaction operations with
MessageActionKind, channel ID, message ID and optional content/emoji value.
`sent()` still contains only the original send requests. Fetch/edit/delete/reaction
on an unknown channel/message pair throws `std::out_of_range`.

The test double records reaction requests without simulating reaction counts,
permissions, Discord validation or rate limits. Stored snapshots and action logs
are intentionally retained for the test's lifetime; this is not a production cache.
Access them on the event-loop thread or after `run()` returns.
