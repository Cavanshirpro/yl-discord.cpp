# yl-discord.cpp v0.1.1

The first normal release in the v0.1 line, completing the live bot message flow
on top of the v0.1.0 offline foundation. Package version: **0.1.1**.

## Live bot support

- Default Client/Bot constructors, `run(token)`, awaitable `start()`, `close()`
  and `token_from_env()` for DISCORD_BOT_TOKEN.
- Bot-authenticated REST and Gateway API v10 discovery/Hello/Identify/Ready.
- Heartbeat jitter/ACK, reconnect/resume, fatal close handling and identify budgets.
- MESSAGE_CREATE → prefix command → `co_await ctx.reply(...)` → actual HTTP request.
- Nonblocking curl multi HTTP, verified TLS, deadlines and bounded response bodies.
- Route/shared-bucket/global limits with asynchronous 429 waiting and bounded retries.
- A 401 stops later requests; uncertain POST failures are not retried automatically.
- Fragmented WebSocket messages, control frames and repeated-sequence suppression.
- Network shutdown during requests, handshakes and rate-limit waits.
- Live ping/echo example with class-based events and Ctrl+C handling.

## Release automation

VERSION/release metadata changes on main now create an annotated version tag and
**call full publication directly**. A GITHUB_TOKEN-created tag therefore still
leads to a published GitHub Release with files. Manually pushed tags also work.
Assets upload to a draft first; only a completed upload becomes public. Retrying
an already complete release keeps its files unchanged.

## Validation and compatibility

Core tests, eleven local HTTP/WebSocket scenarios, installed consumers and package
builds cover the actual frame/JSON flow. CI includes Linux GCC/Clang, ASan/UBSan,
Windows MSVC/MinGW, Conan static/shared, vcpkg and both MSYS2 environments.
Automated tests do not use a real Discord credential. An authenticated Discord
smoke test remains an owner-initiated step.

**Rebuild v0.1.0 consumers.** Public object layouts expanded for networking.
Pre-1.0 shared libraries now use full-version soname/DLL runtime names to prevent
an old application from loading a new incompatible layout. Match compiler,
runtime and dependency versions for binary packages.

The API remains experimental. Slash commands, interactions/UI, voice, compression,
sharding, cache and broad guild/member models are not implemented. No measured
performance advantage over discord.py is claimed. Outgoing messages disable
automatic mentions. Prefix commands need Message Content Intent in code and in
the Discord Developer Portal.

## Downloads

- [Source ZIP](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/yl-discord.cpp-v0.1.1.zip)
- [Source TAR.GZ](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/yl-discord.cpp-v0.1.1.tar.gz)
- [vcpkg overlay](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/yl-discord.cpp-v0.1.1-vcpkg-overlay.zip)
- [MINGW64 package](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/mingw-w64-x86_64-yl-discord-cpp-0.1.1-1-any.pkg.tar.zst)
- [UCRT64 package](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/mingw-w64-ucrt-x86_64-yl-discord-cpp-0.1.1-1-any.pkg.tar.zst)
- [MSYS2 recipe](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/yl-discord.cpp-v0.1.1-msys2-recipe.zip)
- [Release notes](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/release.md)
- [SHA256SUMS](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.1/SHA256SUMS)

These are project-hosted packages; central registry inclusion is separate.
Optional Conan remote upload needs owner-configured credentials.

See [live setup](https://github.com/Cavanshirpro/yl-discord.cpp/blob/v0.1.1/docs/LIVE_BOTS.md)
and [documentation](https://github.com/Cavanshirpro/yl-discord.cpp/tree/v0.1.1/docs).
Copyright (C) 2026 Cavanshirpro. **LGPL-3.0-or-later**; dependencies retain their licenses.
