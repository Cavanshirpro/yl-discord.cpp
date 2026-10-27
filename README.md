# yl-discord.cpp

[![Build](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/ci.yml)
[![Packages](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/packages.yml/badge.svg)](https://github.com/Cavanshirpro/yl-discord.cpp/actions/workflows/packages.yml)
[![Release](https://img.shields.io/github/v/release/Cavanshirpro/yl-discord.cpp?include_prereleases)](https://github.com/Cavanshirpro/yl-discord.cpp/releases)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![License: LGPL v3 or later](https://img.shields.io/badge/license-LGPL--3.0--or--later-blue.svg)](LICENSE)

A C++20 Discord **bot** library built around coroutines, classes and a small API.
A personal project by [Cavanshirpro](https://github.com/Cavanshirpro), inspired by
the ease of use of [discord.py](https://github.com/Rapptz/discord.py).

**v0.1.0 is an offline foundation prerelease. It does not connect to Discord.**
The async event/command core works with a memory transport. HTTP REST, WebSocket
Gateway, bot-token login and slash commands are planned. This is an independent
implementation, not a drop-in replacement or a complete discord.py port.

**Downloads:** [GitHub Releases](https://github.com/Cavanshirpro/yl-discord.cpp/releases)
| [Source ZIP](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/yl-discord.cpp-v0.1.0.zip)
| [Source TAR.GZ](https://github.com/Cavanshirpro/yl-discord.cpp/releases/download/v0.1.0/yl-discord.cpp-v0.1.0.tar.gz)
| [Release notes](release.md)

## A small example

```cpp
#include <yl/discord.hpp>
#include <yl/discord/testing/memory_transport.hpp>

using namespace yl::discord;

int main() {
    auto transport = std::make_shared<testing::MemoryTransport>();
    Bot bot{transport};

    bot.command("ping", [](Context ctx) -> task<> {
        co_await ctx.reply("Pong!");
    });

    // Simulated input: this example never contacts Discord.
    Message message;
    message.id = Snowflake{100};
    message.channel_id = Snowflake{42};
    message.author = User{Snowflake{7}, "developer", false};
    message.content = "!ping";
    bot.dispatch(std::move(message));
    bot.run();
    // transport->sent()[0].content == "Pong!"
}
```

Use `task<T>` for async return values, `co_await` to await an operation, and
`co_return` to return its result. Tasks are lazy: await them or pass them to
`spawn()` to run them. A coroutine does not make blocking I/O or CPU work async.

## What works

- Asio-backed C++20 coroutines and nonblocking timers on one event-loop thread.
- `Client`/`Bot` subclasses, async event hooks and multiple listeners.
- Prefix commands, aliases, quoted arguments and `Context::send/reply`.
- Class-based command collections through `Cog`, including in-flight ownership.
- Command processing that stays active when `on_message()` is overridden.
- Error hooks, raw event dispatch and direct Asio executor access.
- A replaceable message transport and deterministic offline bot tests.
- Static/shared libraries, installed CMake targets, Conan recipe, vcpkg overlay
  generation and MSYS2 MINGW64/UCRT64 package automation.

There is no live Gateway/REST transport, interaction/UI support, voice, cache or
sharding in this release. Intents are configuration data only. See the
[feature comparison](docs/COMPARISON.md) and [roadmap](docs/ROADMAP.md).

## Build on Windows with MSYS2

Use **MINGW64** or **UCRT64** consistently; do not mix their dependencies.
For MINGW64:

```sh
pacman -S --needed git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-asio mingw-w64-x86_64-python
git clone https://github.com/Cavanshirpro/yl-discord.cpp.git
cd yl-discord.cpp
cmake --preset default
cmake --build --preset default --parallel 2
ctest --preset default
./build/default/yl_offline_bot.exe
```

From PowerShell with MSYS2 at `C:/msys64`, use `./scripts/build.ps1` or the
`mingw64` preset. Requirements: a C++20 compiler, CMake 3.25+, Ninja and standalone
Asio >= 1.30.2. CMake fetches a pinned Asio 1.30.2 commit when a suitable dependency
is unavailable; that first configuration needs Git and internet access.

On Linux with a C++20 compiler and Ninja, the same `default` preset builds and
tests the library. See [building and installation](docs/BUILDING.md) for MSVC,
offline builds, shared libraries and dependency discovery.

## Add it to your project

Build from source as a subdirectory:

```cmake
add_subdirectory(external/yl-discord.cpp)
target_link_libraries(my_bot PRIVATE yl::discord)
```

Or use an installed CMake package, with Asio available separately:

```cmake
find_package(ylDiscord 0.1 CONFIG REQUIRED)
target_link_libraries(my_bot PRIVATE yl::discord)
```

The [distribution guide](docs/DISTRIBUTION.md) covers Conan, vcpkg and MSYS2.
Release automation attaches source archives, a commit-pinned vcpkg overlay,
native MSYS2 packages, a source recipe, `release.md` and `SHA256SUMS`.
These are project-hosted packages. Central ConanCenter/vcpkg/MSYS2 listings
require separate upstream acceptance; none is claimed here.

## Documentation

- [Documentation index](docs/README.md)
- [Examples](examples/README.md) and [API reference](docs/API.md)
- [Architecture and ownership](docs/ARCHITECTURE.md)
- [Testing](docs/TESTING.md) and [editor setup](docs/EDITORS.md)
- [Versioning](docs/VERSIONING.md), [release process](docs/RELEASING.md)
  and [publication setup](docs/PUBLISHING.md)
- [Contributing](CONTRIBUTING.md), [security reports](SECURITY.md)
  and [changelog](CHANGELOG.md)

The API is experimental. No measured speed or memory advantage over discord.py
is claimed. Performance comparisons will follow a working live transport.

Copyright (C) 2026 Cavanshirpro. **LGPL-3.0-or-later**; see [LICENSE](LICENSE),
[COPYING](COPYING), [NOTICE](NOTICE) and [third-party notices](THIRD_PARTY.md).
