# Distribution

The package name is **yl-discord-cpp**, repository **yl-discord.cpp**, CMake
package **ylDiscord**, target **yl::discord**, include **yl/discord.hpp**.
v0.2.0 packages include the live bot REST/Gateway transport and the offline testing core.

## Source archives / CMake

Download the `.zip` or `.tar.gz` source from
[GitHub Releases](https://github.com/Cavanshirpro/yl-discord.cpp/releases).
Build using the [installation guide](BUILDING.md), or add the extracted source
with `add_subdirectory()`. Source archives exclude credentials, build outputs and
fetched dependencies. `SHA256SUMS` accompanies release assets.

Installed consumers use `find_package(ylDiscord 0.2 CONFIG REQUIRED)` and
`target_link_libraries(my_bot PRIVATE yl::discord)`. Asio >= 1.30.2, libcurl with TLS/ws/wss/async DNS and nlohmann/json are required
separately. Match the compiler ABI, architecture and runtime for binary packages.

## Conan 2

ConanCenter does not currently carry this project. Build the local recipe:

```sh
python -m pip install 'conan==2.33.0'
conan profile detect
conan create . --build=missing -s compiler.cppstd=20
```

Use a compiler profile appropriate for your environment. For MinGW on Windows,
provide a GCC/MinGW profile rather than accepting a detected MSVC profile.
The recipe supports `-o '&:shared=True'`, resolves Asio, libcurl and nlohmann/json, builds/installs
the library and runs `test_package`. With that recipe in your local cache or a
configured remote, consumers can add:

```ini
[requires]
yl-discord-cpp/0.2.0

[generators]
CMakeDeps
CMakeToolchain
```

Then `conan install . --build=missing -s compiler.cppstd=20` and configure CMake
with the generated toolchain. The [publication guide](PUBLISHING.md) describes
automatic upload to an owner-configured Conan remote. No upload is possible
until that remote and credentials are configured.

## vcpkg overlay

Download/extract the release's `yl-discord.cpp-v0.2.0-vcpkg-overlay.zip`.
It contains `vcpkg-overlay/yl-discord-cpp/portfile.cmake` and `vcpkg.json`.
The generated port pins an exact source commit and SHA512; no placeholders remain.

```sh
vcpkg install yl-discord-cpp:x64-linux --overlay-ports=/path/to/vcpkg-overlay
```

Use `x64-windows`, `x64-windows-static` or another matching triplet as appropriate.
Configure consumers with vcpkg's CMake toolchain. The port disables dependency
downloads and gets Asio through vcpkg. This is a project-hosted overlay, not a
listing in Microsoft's default registry.

To generate an overlay for a commit already pushed to GitHub:
`python scripts/make_vcpkg_overlay.py --ref HEAD`. The upstream archive is downloaded
and hashed; an unpublished local commit cannot be used with that generator.

## MSYS2 packages

Releases attach MINGW64 and UCRT64 `.pkg.tar.zst` packages and a source recipe ZIP.
Install only the package matching your active terminal, for example:

```sh
pacman -U ./mingw-w64-x86_64-yl-discord-cpp-0.2.0-1-any.pkg.tar.zst
```

Pacman resolves the declared Asio/runtime dependencies. The library DLL, import
library, public/generated headers, CMake/pkg-config metadata and license notices
are included. See [the MSYS2 guide](../packaging/msys2/README.md).
`pacman -S yl-discord-cpp` is not claimed: a central MSYS2 listing requires review.
