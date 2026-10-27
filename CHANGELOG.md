# Changelog

Versions follow [MAJOR.MINOR.PATCH](docs/VERSIONING.md).

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
