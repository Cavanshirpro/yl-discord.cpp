# Third-party dependencies and references

**Standalone Asio**, Christopher M. Kohlhoff and contributors, uses the Boost
Software License 1.0 (BSL-1.0). Source-build fallback and Conan use **1.30.2**,
commit `42a93679dc4c8c5caf3d3082542f1bfa2438271d`. System/vcpkg/MSYS2 builds
can use compatible versions >= 1.30.2.

- [Pinned source](https://github.com/chriskohlhoff/asio/tree/42a93679dc4c8c5caf3d3082542f1bfa2438271d)
- [Asio license](https://github.com/chriskohlhoff/asio/blob/42a93679dc4c8c5caf3d3082542f1bfa2438271d/asio/LICENSE_1_0.txt)

Asio remains a separate dependency. Source archives do not bundle it; CMake may
fetch it into the build tree. Installed packages require its headers separately.
Preserve Asio's own license notices when redistributing it.

**libcurl** provides HTTP/TLS and WebSocket framing. Conan and CI use 8.23.0;
compatible system builds >= 7.86 need TLS, asynchronous DNS and ws/wss support.
It uses the curl license. **nlohmann/json** supplies internal JSON conversion,
under the MIT license; Conan uses 3.12.0 (minimum 3.11).

Dependency license texts are included under [third_party/licenses](third_party/licenses/README.md)
and installed alongside this library's notices. Dependency sources/binaries are
still resolved separately rather than bundled into source release archives.

The C++ implementation contains no Python source from discord.py.
[discord.py](https://github.com/Rapptz/discord.py) is a design/usability reference.
Release tooling follows the maintainer's
[discord.cpp-self](https://github.com/Cavanshirpro/discord.cpp-self) project;
adapted tooling remains LGPL-3.0-or-later.

GitHub Actions uses separately maintained checkout, Python setup, artifact and
MSYS2 setup actions. Their licenses and updates remain upstream. Workflow
references are commit-pinned and checked by Dependabot.
