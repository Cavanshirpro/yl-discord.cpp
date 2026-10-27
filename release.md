# yl-discord.cpp v0.1.0

The first offline foundation prerelease of a C++20 Discord bot library inspired
by discord.py's ease of use. Package version: **0.1.0**.

**This release does not connect to Discord.** It provides the async runtime,
event/command API and memory transport for development and testing. HTTP REST,
WebSocket Gateway, token login, slash commands and voice are not implemented.
The public API is experimental; no speed or memory advantage has been measured.

## Included

- `task<T>`, `co_await`, nonblocking timers and a single Asio event loop.
- Client/Bot subclasses, async hooks, multiple listeners and error handling.
- Prefix commands, aliases, quoted arguments, Context send/reply and Cog classes.
- In-flight callback/Cog ownership, raw event dispatch and executor access.
- MemoryTransport, offline behavior tests and two working example programs.
- Static/shared builds, generated version/DLL headers and installed CMake consumers.
- Conan 2 recipe, commit-pinned vcpkg overlay and MSYS2 package automation.
- API/build/distribution/release documentation and MAJOR.MINOR.PATCH validation.

## Downloads

From [this release](https://github.com/Cavanshirpro/yl-discord.cpp/releases/tag/v0.1.0):

- [Source ZIP](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/yl-discord.cpp-v0.1.0.zip)
- [Source TAR.GZ](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/yl-discord.cpp-v0.1.0.tar.gz)
- [vcpkg overlay](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/yl-discord.cpp-v0.1.0-vcpkg-overlay.zip)
- [MINGW64 package](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/mingw-w64-x86_64-yl-discord-cpp-0.1.0-1-any.pkg.tar.zst)
- [UCRT64 package](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/mingw-w64-ucrt-x86_64-yl-discord-cpp-0.1.0-1-any.pkg.tar.zst)
- [MSYS2 source recipe](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/yl-discord.cpp-v0.1.0-msys2-recipe.zip)
- [Release notes](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/release.md)
- [SHA256SUMS](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/SHA256SUMS)

Source archives include source, examples, tests, documentation, recipes and
license notices. Dependencies are not bundled. Native MSYS2 packages use
declared Asio/runtime dependencies and must match your compiler environment.
Project-hosted packages are not central ConanCenter/vcpkg/MSYS2 listings.

## Build and try

Use a C++20 compiler, CMake >= 3.25, Ninja and standalone Asio >= 1.30.2. CMake
can fetch a pinned Asio commit when needed. Python >= 3.10 supports release tools.

```sh
cmake --preset default
cmake --build --preset default --parallel 2
ctest --preset default
./build/default/yl_offline_bot
```

On Windows add `.exe`; use the matching MINGW64/UCRT64 runtime on PATH.
Consumers use `find_package(ylDiscord 0.1 CONFIG REQUIRED)` and `yl::discord`.
For setup and package instructions see
[the documentation](https://github.com/Cavanshirpro/yl-discord.cpp/tree/v0.1.0/docs).

Copyright (C) 2026 Cavanshirpro. **LGPL-3.0-or-later**. Asio remains BSL-1.0.
