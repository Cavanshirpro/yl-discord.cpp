# Editor setup

Configure with CMake first. `<yl/discord/version.hpp>` and `export.hpp` are
generated under the build directory, and the target's include paths expose them.
Opening headers without configuration can make those includes appear missing.

For Ninja/clangd:

```sh
cmake --preset default -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Point clangd at `build/default` with `--compile-commands-dir=build/default`.
For VS Code use CMake Tools as the C/C++ configuration provider, or configure
clangd with the same compilation database. On Windows use the compiler and
headers from one MSYS2 environment, or configure an MSVC build separately.
The public headers contain concise comments; [API.md](API.md) documents the
full runtime/ownership contract. A Doxygen executable is not required.

Compile consumers through `yl::discord` to inherit C++20, dependency paths and
the correct DLL/static definitions. Do not manually add a build-machine path
to installed public headers.
