# Building and installation

Use C++20, CMake >= 3.25, standalone Asio >= 1.30.2, libcurl >= 7.86 with TLS,
asynchronous DNS and ws/wss, and nlohmann/json >= 3.11. Ninja is required by the
provided presets. Python >= 3.10 is needed for release tooling, not the C++ library.
The first fallback Asio download needs Git and network access.

## Dependency discovery

The build first accepts an existing `asio::asio` CMake target/config package,
then suitable standalone Asio headers found by `find_path`. If neither is
available, `YL_DISCORD_FETCH_ASIO=ON` downloads the pinned 1.30.2 commit.
An older system header installation is not used by the fallback.

For a network-free build provide your own headers:

```sh
cmake -S . -B build/offline -G Ninja -DYL_DISCORD_FETCH_ASIO=OFF \
  -DYL_DISCORD_ASIO_INCLUDE_DIR=/absolute/path/to/asio/include
cmake --build build/offline
ctest --test-dir build/offline --output-on-failure
```

The installed CMake config never downloads dependencies. Consumers need Asio,
libcurl and nlohmann/json through their package manager/CMAKE_PREFIX_PATH, or
the explicit Asio include-path option. Some distribution curl builds omit ws/wss;
use the Conan/vcpkg recipes or build curl with WebSockets in that case.

## Linux / Ninja

Install a compiler with C++20 coroutine support, Git, CMake and Ninja. System Asio
is optional for source builds; older distributions may provide < 1.30.2.

```sh
cmake --preset default
cmake --build --preset default --parallel 2
ctest --preset default
./build/default/yl_offline_bot
./build/default/yl_class_bot
```

To choose Clang, configure a fresh build directory with
`cmake -S . -B build/clang -G Ninja -DCMAKE_CXX_COMPILER=clang++`.

## Windows / MSYS2

Use the dependencies and terminal from the same MINGW64 or UCRT64 environment.
See the [README](../README.md) for the MINGW64 package command; replace the package
prefix with `mingw-w64-ucrt-x86_64` for UCRT64. Use the `default` preset in either
MSYS2 terminal. PowerShell can use:

```powershell
./scripts/build.ps1
```

The script and `mingw64` preset default to `C:/msys64`. Set the script's
`-Msys2Root` parameter for another installation. The matching `bin` directory
must be on PATH when executables or a DLL-based consumer run.

## Windows / MSVC

From a Visual Studio 2022 developer terminal with current C++ tooling:

```powershell
cmake -S . -B build/msvc -A x64
cmake --build build/msvc --config Release --parallel 2
ctest --test-dir build/msvc -C Release --output-on-failure
```

Generated export annotations support DLL/static consumers. Do not combine a
MinGW library with an MSVC application; compiler, runtime and architecture must
match. GitHub CI tests MSVC and MinGW separately. If including Windows headers
before the library, define `WIN32_LEAN_AND_MEAN` and `NOMINMAX` before
`<windows.h>` to avoid legacy Winsock and min/max macro conflicts.

## Options

| Option | Default | Effect |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | OFF | Build a shared library instead of static |
| `YL_DISCORD_BUILD_EXAMPLES` | ON at top level | Build both offline examples |
| `YL_DISCORD_BUILD_TESTS` | ON at top level | Build the core behavior suite |
| `YL_DISCORD_FETCH_ASIO` | ON | Allow source-build dependency download |
| `YL_DISCORD_ASIO_INCLUDE_DIR` | Discovered | Explicit standalone Asio header directory |

Tests/examples default to OFF when used through `add_subdirectory`. Configure
before opening headers in an editor: version and DLL-export headers are generated.

## Install and consume

```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build/release
cmake --install build/release --prefix /absolute/path/to/install
cmake -S tests/consumer -B build/consumer -G Ninja \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/install \
  -DYL_DISCORD_ASIO_INCLUDE_DIR=/absolute/path/to/asio/include
cmake --build build/consumer
./build/consumer/consumer
```

For multi-config generators pass `--config Release` to build/install and `-C
Release` to CTest. Installation includes headers, library, `ylDiscord` CMake
config, `yl::discord` target, `yl-discord.pc` and license notices. Asio is separate.
Consumers inherit C++20, Asio and platform dependencies through `yl::discord`.
For pkg-config, use `pkg-config --cflags --libs yl-discord`; add `--static` when
linking statically and enable C++20 in your compiler flags.
