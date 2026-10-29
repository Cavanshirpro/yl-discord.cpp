# yl-discord.cpp v0.4.0

This normal minor release adds bounded, buffered multipart uploads and explicit
attachment editing. Package version: **0.4.0**.

## Files and messages

- FileUpload owns immutable shared byte buffers with independent filename,
  description and content-type metadata. Copies do not duplicate large buffers.
- Text and binary construction preserve embedded NULs. There are no implicit
  filesystem reads, URL downloads or transcoding operations.
- MessageOptions::files supports file-only sends/replies and attachment:// embeds.
- REST uses libcurl MIME read/seek callbacks over the existing async multi engine,
  without constructing a second full multipart byte string.
- Server 429 retries use fresh upload cursors; uncertain writes are not retried.
- MessageEditOptions requires explicit retained attachment IDs when adding files;
  omitted fields preserve files, an empty retention vector clears/replaces them.
- Response Attachment models retain IDs, names, uint64 sizes, descriptions, media
  types and URL metadata. Message values remain owned snapshots.
- MemoryTransport stores synthetic metadata and records upload/retention intent.
  Unsupported older text-edit transports reject attachment edits explicitly.

## Bounds and lifecycle

Typed limits are 20 MiB per file, 24 MiB combined new bytes and 10 total retained/
new attachments. The multipart builder checks a 25 MiB request budget with 64 KiB
reserved for framing; large JSON metadata can reduce the remaining file budget.
Filename/MIME/description validation prevents control/header injection and rejects
ambiguous upload labels. Discord still enforces its own current permissions,
media checks and account/channel limits.

Uploads share TLS validation, rate-limit queues, deadlines, User-Agent metadata
and close cancellation. Tests cancel an active 8 MiB upload without waiting for
the server to drain its socket. Rebuild consumers: public models extend in this minor.

## Validation and distribution

Validation includes 13 core, 10 command, 15 event, 6 embed, 6 file, 16 local
HTTP/WebSocket and 11 release-tool scenarios. Multipart fixtures verify binary
bytes, UTF-8 names, NULs, 429 replay, retained/new/cleared files and file-only replies.
Installed consumers exercise the new file payload across static/shared boundaries.

CI covers Linux GCC/Clang, Windows MSVC/MinGW, ASan/UBSan, Conan, vcpkg and native
MSYS2 MINGW64/UCRT64. Six offline examples and a live ping bot compile; automated
runs never contact Discord or load a real bot credential. Authenticated Discord
smoke testing remains owner-initiated.

Release assets include source ZIP/TAR.GZ, vcpkg overlay, native MINGW64/UCRT64
packages, MSYS2 recipe, release notes and SHA256SUMS. Optional Conan remote upload
requires owner-configured credentials; no central-registry listing is claimed.

Slash commands, UI, stream/filesystem upload sources, progress hooks, broad guild/
member APIs, voice, sharding and production caches remain planned. No general
performance advantage over discord.py is claimed.

- [Source ZIP](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.4.0/yl-discord.cpp-v0.4.0.zip)
- [Source TAR.GZ](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.4.0/yl-discord.cpp-v0.4.0.tar.gz)
- [Files guide](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.4.0/docs/FILES.md)
- [Upload example](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.4.0/examples/upload_bot.cpp)
- [Live setup](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.4.0/docs/LIVE_BOTS.md)

Licensed under **LGPL-3.0-or-later**.
