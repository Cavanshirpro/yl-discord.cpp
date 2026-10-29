# yl-discord.cpp

[![Build](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/ci.yml)
[![Packages](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/packages.yml/badge.svg)](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/packages.yml)
[![Release](https://img.shields.io/github/v/release/Cavanshirpro/yl-discord.cpp)](https://github.com/Cavanshirpro/yl-discord.cpp/releases)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![License: LGPL v3 or later](https://img.shields.io/badge/license-LGPL--3.0--or--later-blue.svg)](LICENSE)

A C++20 Discord **bot** library built around coroutines, classes and a small API.
A personal project by [Cavanshirpro](https://github.com/Cavanshirpro), inspired by
the ease of use of [discord.py](https://github.com/Rapptz/discord.py).

**v0.4.0 adds bounded multipart file uploads:** shared binary buffers, file-only
replies and explicit attachment retention, alongside event waits, rich embeds,
typed commands, checks, cooldowns and live REST/Gateway connections.
It is an independent implementation with an experimental API, not a complete
discord.py port. Slash commands, UI, voice, sharding and broad guild/member APIs
are still planned.

**Downloads:** [GitHub Releases](https://github.com/Cavanshirpro/yl-discord.cpp/releases)
| [Source ZIP](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.4.0/yl-discord.cpp-v0.4.0.zip)
| [Source TAR.GZ](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.4.0/yl-discord.cpp-v0.4.0.tar.gz)
| [Release notes](release.md)

## A small bot

```cpp
#include <yl/discord.hpp>

using namespace yl::discord;

int main() {
    Bot bot{BotOptions{
        .intents = Intents::defaults().with(Intent::message_content)
    }};

    bot.command("ping", [](Context ctx) -> task<> {
        co_await ctx.reply("Pong!");
    });

    bot.run(token_from_env()); // DISCORD_BOT_TOKEN: raw bot token
}
```

Enable Message Content Intent in the Developer Portal for guild prefix commands.
See [live setup](docs/LIVE_BOTS.md) and [ping_bot.cpp](examples/ping_bot.cpp) for
permissions, token setup and Ctrl+C shutdown. Automated tests use local fixtures;
no real Discord credential is used by CI. The authenticated smoke test remains
an owner-initiated step.

Use `task<T>`, `co_await` and `co_return` in asynchronous functions. Tasks are
lazy: await or spawn them. Blocking I/O and CPU work still block the event loop.

## What works

- Bot-token REST authentication and Gateway discovery/Hello/Identify/Ready.
- Heartbeat jitter/ACK tracking, fatal close handling and bounded reconnect/resume.
- Message events, prefix commands, aliases, quoted arguments and Context send/reply.
- Bound message fetch/edit/delete and Unicode/custom own reactions.
- Typed rich embeds, preserve/clear edits, Unicode-aware limits and explicit mention whitelists.
- Shared binary upload buffers, multipart sends/replies and explicit attachment retention.
- Typed/optional command parameters, custom converters, async checks, bounded cooldowns and opt-in help.
- Client/Bot subclasses, multiple listeners and Cog-owned class callbacks.
- Cog-wide command removal that retains callbacks already in progress.
- Commands that keep working when `on_message()` is overridden.
- Nonblocking HTTP, verified TLS, request deadlines and asynchronous bucket/global limits.
- Network shutdown that interrupts transfers, handshakes and rate-limit waiting.
- Raw dispatch events, low-level authenticated REST and Asio executor access.
- Predicate-based event waits with deadlines/cancellation and removable listener subscriptions.
- MemoryTransport plus local HTTP/WebSocket fixtures for token-free tests.
- Static/shared libraries, installed CMake targets, Conan, vcpkg overlays and MSYS2 assets.

Outgoing messages disable automatic mentions. Repeated Gateway sequences are
ignored. Uncertain POST failures are not retried automatically. See the
[API reference](docs/API.md) for ownership, lifecycle and supported scope.

Commands can bind parameters directly:

```cpp
bot.command<int, int>("add", [](Context ctx, int a, int b) -> task<> {
    co_await ctx.reply(std::to_string(static_cast<long long>(a) + b));
}, {.description = "Add two integers"});
bot.add_help_command(); // !help and !help add
```

See [the command guide](docs/COMMANDS.md) for checks, cooldowns, custom converters,
optional parameters and invocation-aware error handling.

## Build

Requirements: C++20, CMake >= 3.25, Ninja, standalone Asio >= 1.30.2,
libcurl >= 7.86 with **TLS, asynchronous DNS, ws/wss**, and nlohmann/json >= 3.11.
Python >= 3.10 is required for integration/release tests. CMake can fetch a pinned
Asio commit when suitable headers are unavailable; other dependencies are separate.

MSYS2 **MINGW64**:

```bash
pacman -S --needed git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-asio mingw-w64-x86_64-curl mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-python
git clone https://github.com/Cavanshirpro/yl-discord.cpp.git
cd yl-discord.cpp
cmake --preset default
cmake --build --preset default --parallel 2
ctest --preset default
```

Use matching UCRT64 packages in UCRT64. From PowerShell with MSYS2 at `C:/msys64`,
use `./scripts/build.ps1` or the `mingw64` preset. On Linux, the same preset works
with the required development dependencies. Some distribution curl builds omit
WebSockets; use Conan/vcpkg or build curl with ws/wss enabled.
See [building and installation](docs/BUILDING.md) for MSVC and offline configuration.

## Consume

```cmake
find_package(ylDiscord 0.4 CONFIG REQUIRED)
target_link_libraries(my_bot PRIVATE yl::discord)
```

Or use `add_subdirectory(external/yl-discord.cpp)` and the same target. The
[distribution guide](docs/DISTRIBUTION.md) covers Conan, vcpkg overlays and MSYS2.
Packages are project-hosted; no central registry listing is claimed.

## Releases

Versions use **MAJOR.MINOR.PATCH** and `vMAJOR.MINOR.PATCH` tags. Updating VERSION
and release metadata on main starts `release-version`: create the immutable tag,
call the full build/package workflow, upload assets and publish the GitHub Release.
This explicitly calls publication even when GITHUB_TOKEN created the tag.
Manual tag pushes still publish releases through the same workflow.

Releases include source ZIP/TAR.GZ, vcpkg overlay, native MINGW64/UCRT64 packages,
MSYS2 source recipe, release notes and SHA256SUMS. Optional Conan remote upload
requires the [documented credentials](docs/PUBLISHING.md).

[Documentation](docs/README.md) · [Architecture](docs/ARCHITECTURE.md) ·
[Roadmap](docs/ROADMAP.md) · [Testing](docs/TESTING.md) ·
[Release process](docs/RELEASING.md) · [Changelog](CHANGELOG.md)

No measured performance advantage over discord.py is claimed. This release
expands event and message control; broader functionality follows the roadmap.

Copyright (C) 2026 Cavanshirpro. **LGPL-3.0-or-later**. See [LICENSE](LICENSE),
[COPYING](COPYING), [NOTICE](NOTICE) and [third-party notices](THIRD_PARTY.md).
