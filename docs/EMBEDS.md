# Rich messages and mention control

Text-only send/reply/edit methods remain available. Pass MessageOptions to send
content, embeds and an explicit mention policy together:

```cpp
Embed embed;
embed.title = "Status";
embed.description = "Ready.";
embed.color = 0x57f287;
embed.fields.push_back({"Latency", "Measured by your application", true});
MessageOptions options;
options.embeds.push_back(std::move(embed));
auto message = co_await ctx.reply(options);
```

Client::send_message(channel, options), Context::send/reply(options) and
Message::reply(options) return owned, bound Message snapshots. An embed-only
message may have empty content. Message::embeds contains the supported fields
parsed from incoming Gateway and REST messages. See [embed_bot.cpp](../examples/embed_bot.cpp)
for a runnable offline example.

## Embed fields

Embed owns title, description, URL, optional 24-bit color, timestamp, fields,
optional footer/author, image URL and thumbnail URL. EmbedField has name, value
and inline_field; EmbedFooter has text/icon_url; EmbedAuthor has name/url/icon_url.
The incoming type string is informational: outgoing embeds use Discord's rich
format. Provider/video/proxy/dimension fields and component embeds are outside
this typed subset. Low-level REST and raw events remain available for other fields.

High-level sends and edits validate the supported outgoing schema before I/O:

- Message content: at most 2000 units; at most 10 embeds.
- Embed title/author name/field name: at most 256 units each.
- Description: 4096; field value: 1024; footer text: 2048.
- At most 25 fields per embed and 6000 combined text units across all embeds.
- Fields require nonempty names/values; author/footer objects require name/text.
- An embed must contain displayable content; color alone is insufficient.

Validation uses conservative UTF-16 code-unit counts and includes surrounding
whitespace. Non-BMP characters count as two units; grapheme clusters can contain
multiple code points. Malformed UTF-8 is rejected. No automatic truncation occurs.
Discord applies its own current limits/rendering rules, including trimming; raw
REST is available if your application needs different preflight policy.

URLs require lower-case http:// or https://; media/icon fields also accept
attachment:// references. Controls and unescaped whitespace are rejected, with
an 8192-unit project cap per URL. Timestamp is a UTF-8 string capped at 128 units;
provide ISO8601. Discord validates its date format and URL accessibility. The
library does not fetch embed media or upload attachment:// resources for you.
File uploads use [FileUpload](FILES.md); the caller supplies file bytes explicitly.

`validate_message_options(options)` / `validate_message_edit(options)` expose
the same checks for explicit preflight. Validation failures use std::invalid_argument
without embedding the rejected content in diagnostics. Callers that bypass the
typed helpers through RestClient::request() own their JSON validation.

## Explicit edits

```cpp
MessageEditOptions update;
update.embeds = message.embeds;
update.embeds->front().description = "Updated.";
auto changed = co_await message.edit(update);

MessageEditOptions clear;
clear.embeds = std::vector<Embed>{}; // Engaged empty vector removes all embeds.
(void)co_await changed.edit(clear);
```

MessageEditOptions::content and embeds are optional. Disengaged fields stay
unchanged; an engaged empty content string clears text and an engaged empty
vector clears embeds. Use `content = ""` / `embeds = std::vector<Embed>{}` to
express that intent; assigning `{}` to an optional disengages it. At least one
editable field must be supplied. Omitted attachments/components/flags are preserved.
The content-only string overload preserves existing embeds.

Edits return new snapshots without mutating older copies. Mention policy is
always included with the edit and defaults to suppressing automatic mentions;
it does not inherit an earlier opt-in policy.

## Allowed mentions

AllowedMentions defaults to all automatic parsing and reply pings disabled.
Use ID whitelists or deliberate parse flags:

```cpp
options.content = "Hello <@123>";
options.allowed_mentions.users = {Snowflake{123}};
// Other mentions remain disabled.
```

The boolean flags are everyone, all_users, all_roles and replied_user. Everyone
also controls @here according to Discord's policy. Users/roles whitelists accept
at most 100 distinct nonzero IDs each. A parse-all flag cannot be combined with
the same type's whitelist. `AllowedMentions::all()` explicitly enables every
category including reply-user pings. Discord permissions and actual mention
syntax still determine which notifications occur.

## Transports and tests

MessageRequest now includes embeds and allowed_mentions. Custom transports must
handle these fields when supporting rich sends. A new structured edit virtual
method defaults to the older text-edit overload only for content-only/default-policy
edits; richer edits on an older implementation fail explicitly. Rebuild consumers
for this minor because public layouts and virtual interfaces changed.

MemoryTransport validates the same outgoing schema, stores rich snapshots and
records structured edits in MessageAction::edit_options. It does not simulate
Discord permissions, rendering, notifications or rate limits. The loopback fixture
checks actual JSON for create/reply, whitelists, explicit all-mentions, embed clear
and replace, supported response parsing, and preflight rejection before HTTP.

The schema follows the [Discord message reference](https://docs.discord.com/developers/resources/message).
