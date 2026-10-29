# yl-discord.cpp v0.2.0

This release extends the live bot core with an everyday command toolkit and
awaitable message operations. Package version: **0.2.0**. Normal GitHub release.

## Commands

- `command<Args...>()` binds owning typed parameters after Context; trailing
  `std::optional<T>` parameters are supported.
- Built-in string, integer, finite floating-point, bool and Snowflake/mention
  converters; applications can specialize `convert_argument<T>()`.
- Strict arity and conversion errors, plus `Context::argument<T>()` for manual parsing.
- Global and command-level asynchronous checks, including guild-only, DM-only
  and explicit user-ID helpers.
- Sliding-window cooldowns with global/user/channel/guild scopes, shared aliases,
  monotonic retry durations, reset support and bounded bucket counts.
- Owned command metadata, generated usage, hidden help entries and opt-in
  `add_help_command()` with UTF-8-safe pagination.
- Invocation-aware `on_command_error(Context, exception_ptr)` and original aliases.
- Cog-wide command removal with in-flight callback/check ownership preserved.

## Messages

- Client, bound Message and RestClient fetch/edit/delete helpers.
- Unicode/custom reaction creation and removal of the current bot's reaction.
- Mention suppression repeated on edits; content-only edits preserve other fields.
- URL-encoded emoji paths, normal empty 204 handling and shared rate-limit/cancellation behavior.
- Generic ProtocolError diagnostics for malformed successful message responses.
- MemoryTransport seeded snapshots and action records, with no transport ownership cycles.
- Existing send-only custom transports remain source-compatible; optional operations
  default to lazy unsupported-operation errors.

## Compatibility and validation

Rebuild consumers for this minor release: the public virtual transport interface
and experimental object layouts changed. The original manual Context callback
overload remains supported. Help is opt-in and checks do not request Discord permissions.
Typed conversion failures consume a cooldown slot after checks have succeeded.

Validation includes 13 core scenarios, 10 command scenarios, 12 local HTTP/WebSocket
scenarios, installed package consumers and release-tool tests. CI covers Linux
GCC/Clang, Windows MSVC/MinGW, static/shared builds, ASan/UBSan, Conan, vcpkg and
MSYS2 MINGW64/UCRT64. Tests use local fixtures, never a real Discord credential.
The authenticated Discord smoke test remains owner-initiated.

Slash commands, UI, embeds, attachments, broad guild/member APIs, voice, sharding
and production caches remain planned. No performance advantage is claimed without
equivalent-workload measurements.

## Downloads and guides

Release assets include source ZIP/TAR.GZ, vcpkg overlay, MINGW64/UCRT64 native
packages, MSYS2 recipe, release notes and SHA256SUMS. Optional Conan remote upload
still requires owner-configured credentials; no central-registry listing is claimed.

- [Source ZIP](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.2.0/yl-discord.cpp-v0.2.0.zip)
- [Source TAR.GZ](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.2.0/yl-discord.cpp-v0.2.0.tar.gz)
- [Commands](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.2.0/docs/COMMANDS.md)
- [Messages](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.2.0/docs/MESSAGES.md)
- [Live setup](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.2.0/docs/LIVE_BOTS.md)

Licensed under **LGPL-3.0-or-later**.
