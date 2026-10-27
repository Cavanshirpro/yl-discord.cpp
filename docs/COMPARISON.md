# Current scope compared with discord.py

This is a capability/scope comparison, not a benchmark. It does not imply API
compatibility or complete coverage of discord.py.

| Area | discord.py | yl-discord.cpp v0.1.0 |
| --- | --- | --- |
| Async functions | Python async/await | C++20 task/co_await, Asio |
| Class event overrides | Supported | Implemented for the initial models |
| Prefix commands / Cogs | Established extension | Small offline implementation |
| Token login / Gateway | Implemented | Planned |
| REST / rate limits | Implemented | Transport interface only |
| Slash commands / UI | Implemented | Planned |
| Guild/member/role/cache APIs | Broad coverage | Initial User/Message models only |
| Voice | Supported | Not implemented |
| Raw access | Raw events and advanced APIs | Manual raw dispatch and executor access |
| Offline test transport | Application/tooling dependent | Included MemoryTransport |
| Distribution | Python packages | Source/CMake, Conan recipe, vcpkg overlay, MSYS2 assets |

The C++ API aims for familiar concepts, not Python syntax emulation. No
performance advantage has been measured. See [the roadmap](ROADMAP.md) and
the [discord.py documentation](https://discordpy.readthedocs.io/en/stable/).
