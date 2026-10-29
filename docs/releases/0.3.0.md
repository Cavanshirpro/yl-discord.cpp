# yl-discord.cpp v0.3.0

This normal minor release adds event control and rich messages to the live bot
and command core. Package version: **0.3.0**.

## Event control

- `wait_for<Message/Ready/RawEvent>()` observes future dispatches with synchronous
  predicates, owned results and monotonic deadlines.
- Standard stop tokens (including cross-thread requests), native Asio cancellation
  and `close()` cancellation release pending waits and predicate captures.
- Removable Message/Ready/RawEvent subscriptions return weak ListenerConnections.
  Disconnection retains callbacks already snapshotted for an event.
- Predicate failures are isolated; foreign event loops are rejected. Paused time
  still counts toward a wait's deadline. Unstarted waits register nothing.
- `is_cancelled(error_code)` recognizes header-only Asio cancellation across
  shared-library error-category instances.

## Rich messages

- Owned Embed, EmbedField, EmbedFooter and EmbedAuthor models.
- MessageOptions for content/embed sends and replies with explicit AllowedMentions.
- MessageEditOptions distinguishes preserving omitted content/embeds from clearing
  them with an explicitly empty string/vector.
- Outgoing UTF-8 validation, conservative UTF-16 limits, per-field/combined embed
  bounds, URL policy and mention flag/whitelist validation before HTTP.
- Mention parsing and reply pings remain disabled unless explicitly enabled.
- Supported rich fields parse from Gateway and REST into bound message snapshots.
- MemoryTransport stores rich snapshots and records structured edit intent.
- Older text-edit custom transports retain their fallback for content-only edits;
  unsupported rich edits fail explicitly.

## Packaging and validation

Rebuild consumers: this minor extends public models and virtual interfaces.
Installed shared consumers now verify their configured curl runtime as well as
command, message, event and rich-payload behavior. Runtime discovery instructions
cover custom dynamic curl prefixes. Five offline examples and one live ping bot
compile in CI; automated examples never use a real Discord credential.

Validation includes 13 core, 10 command, 15 event, 6 embed, 14 local HTTP/WebSocket
and 11 release-tool scenarios. Platform coverage includes Linux GCC/Clang,
Windows MSVC/MinGW, static/shared builds, ASan/UBSan, Conan, vcpkg and native
MSYS2 MINGW64/UCRT64 consumers. Authenticated Discord smoke testing remains
owner-initiated. No measured advantage over discord.py is claimed.

Attachments, slash commands, UI, broad guild/member APIs, voice, sharding,
production caches and managed Cog background tasks remain planned.

## Assets and guides

Release assets include source ZIP/TAR.GZ, vcpkg overlay, native MINGW64/UCRT64
packages, MSYS2 recipe, release notes and SHA256SUMS. Optional Conan remote upload
requires owner-configured credentials; no central-registry listing is claimed.

- [Source ZIP](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.3.0/yl-discord.cpp-v0.3.0.zip)
- [Source TAR.GZ](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.3.0/yl-discord.cpp-v0.3.0.tar.gz)
- [Events](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.3.0/docs/EVENTS.md)
- [Embeds](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.3.0/docs/EMBEDS.md)
- [Live setup](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.3.0/docs/LIVE_BOTS.md)

Licensed under **LGPL-3.0-or-later**.
