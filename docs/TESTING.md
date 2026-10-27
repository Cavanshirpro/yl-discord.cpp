# Testing

All current tests are offline. No token, real account or network request is
needed to execute the C++ tests/examples. Dependency and package downloads can
need network access during setup.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
python scripts/check_release.py
python -m unittest discover -s tests -p test_release.py -v
```

The core executable covers Snowflake bounds, intents, quoted argument parsing,
concurrent commands, reply references, independent hooks/listeners, error
isolation, default error propagation, synchronous hook failure, Cog ownership
during removal, registration rollback, raw events, repeated run, executor use
and stop/resume behavior. Assertions remain active in Release builds because
the suite does not use the disabled `assert` macro.

`tests/consumer` links/runs against the installed CMake package. Conan's
`test_package` checks an actual command/reply through the installed dependency.
CI also relocates CMake installation prefixes and tests shared/static builds.

Release-tool tests reject version mismatch, broken links, credential paths,
unsafe archive entries and unfinished release notes. Source ZIP/TAR archives
are validated after creation. `check_release.py` scans only tracked/unignored
project files; it does not read ignored local credential files. Never force-add
such files. The scanner is a release check, not a guarantee of secret detection.

Current Actions coverage includes Linux GCC/Clang, Windows MSVC/MinGW, Conan
static/shared consumers, vcpkg overlay installation and MSYS2 MINGW64/UCRT64.
Do not add a live-token check to PR workflows. Future live transport needs local
HTTP/Gateway fixture tests before manual Discord smoke tests.
