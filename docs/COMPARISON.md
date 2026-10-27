# Current scope compared with discord.py

This is a capability/scope comparison, not a benchmark. It does not imply API
compatibility or complete coverage of discord.py.

| Area | discord.py | yl-discord.cpp v0.1.1 |
| --- | --- | --- |
| Async functions | Python async/await | C++20 task/co_await, Asio |
| Class event overrides | Supported | Implemented for the initial models |
| Prefix commands / Cogs | Established extension | Small offline implementation |
| Token login / Gateway | Implemented | Bot sessions, heartbeat, resume |
| REST / rate limits | Implemented | Async messages and low-level requests, bucket/global limits |
| Slash commands / UI | Implemented | Planned |
| Guild/member/role/cache APIs | Broad coverage | Initial User/Message models only |
| Voice | Supported | Not implemented |
| Raw access | Raw events and advanced APIs | Live raw dispatch, low-level REST and executor |
| Offline test transport | Application/tooling dependent | Included MemoryTransport |
| Distribution | Python packages | Source/CMake, Conan recipe, vcpkg overlay, MSYS2 assets |

The C++ API aims for familiar concepts, not Python syntax emulation. No
performance advantage has been measured. See [the roadmap](ROADMAP.md) and
the [discord.py documentation](https://discordpy.readthedocs.io/en/stable/).
