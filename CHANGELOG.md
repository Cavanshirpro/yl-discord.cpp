# Changelog

Versions follow [MAJOR.MINOR.PATCH](docs/VERSIONING.md).

## [0.1.1] - 2026-10-27

Completed the initial live bot message flow:

- Default Client/Bot networking, raw bot token login, run(token), start(), close() and REST access.
- Verified TLS, nonblocking curl multi HTTP, deadlines and bounded decoded response bodies.
- Gateway API v10, Hello/Identify/Ready, heartbeat jitter/ACK and reconnect/resume.
- Fatal close handling, identify limits, duplicate-sequence suppression and fragmented WebSockets.
- Route/shared-bucket/global rate-limit waits, bounded 429 retries and rejection after 401.
- Network cancellation during requests, rate-limit waits and handshakes; safe paused destruction.
- Live ping/echo example with token environment setup and Ctrl+C handling.
- Eleven local HTTP/WebSocket integration scenarios and Linux ASan/UBSan coverage.
- Automatic version-change → annotated tag → tested assets → published GitHub Release.
- Idempotent publication retries that keep an already complete release unchanged.
- Pre-1.0 versioned shared-library runtime names; rebuild consumers for this release.

Normal GitHub release (not a prerelease). v0.1.0 remains unchanged as historical
foundation. CI uses fixtures, not a real Discord token; the authenticated Discord
smoke test is an owner-initiated step. Slash commands/UI/voice/sharding remain planned.

## [0.1.0] - 2026-10-27

Initial offline foundation prerelease:

- C++20 task/co_await API, Asio event loop and nonblocking timers.
- Client/Bot event hooks, prefix commands, aliases, parser, Context and Cog.
- In-flight callback ownership, error isolation, raw events and executor access.
- Memory message transport, behavior tests and two offline examples.
- Static/shared CMake builds, installed consumers, version/export headers.
- Conan recipe, vcpkg overlay and MSYS2 MINGW64/UCRT64 package automation.
- Version/tag validation, source archives, release notes, SHA256SUMS and GitHub workflows.
- API, ownership, setup, distribution, release and contributor documentation.

Live Discord Gateway/REST, bot login, interactions, voice and caching are not
implemented in this release.
