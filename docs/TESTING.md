# Testing

All current tests run without a real Discord account. Network integration tests
use local HTTP/WebSocket fixture servers. No token, real account or network request is
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

The command suite covers strict numeric/ID conversion, custom converters, optional
parameters, arity errors, preserved invocation Context, asynchronous check ownership,
cooldown alias sharing/scopes/reset/expiry/capacity, failed-check admission,
UTF-8 help pagination and Cog-wide removal during a suspended callback.
Core tests also verify lazy message operations, snapshot binding and send-only
custom transport compatibility. The message network scenario checks fetch/edit/
delete, mention suppression on edits, encoded Unicode/custom reactions, 204 empty
responses, reaction 429 retries, malformed response diagnostics, 403 and 404.

The event suite verifies removable listener ownership, weak/stale handles,
filtered broadcast, Message binding, Ready/RawEvent waits, predicate release,
deadlines, pre-requested/cross-thread stop tokens, native Asio cancellation,
close cancellation, predicate errors, completion races, paused destruction and
expiry, registry mutation during predicates, foreign-executor rejection and
unstarted-task cleanup.

The embed suite covers UTF-8/UTF-16 boundaries, per-field and combined limits,
URL policy, mention flag/whitelist conflicts, owned rich snapshots, preserve/clear
edit semantics and older custom edit implementations. The thirteenth network
scenario verifies full rich JSON, mention policies and embed-only create/reply/edit.

`tests/consumer` links/runs against the installed CMake package. Conan's
`test_package` checks an actual command/reply through the installed dependency.
CI also relocates CMake installation prefixes and tests shared/static builds.

Release-tool tests reject version mismatch, broken links, credential paths,
unsafe archive entries and unfinished release notes. Publisher tests verify create/upload/final publication and idempotent retry.
Source ZIP/TAR archives are validated after creation. `check_release.py` scans only tracked/unignored
project files; it does not read ignored local credential files. Never force-add
such files. The scanner is a release check, not a guarantee of secret detection.

Current Actions coverage includes Linux GCC/Clang (including a static ASan/UBSan
configuration), Windows MSVC/MinGW, Conan
static/shared consumers, vcpkg overlay installation and MSYS2 MINGW64/UCRT64.
Do not add a live-token check to PR workflows. The local integration suite verifies real JSON/frame traffic, delayed REST replies
with independent heartbeats, Resume, fragments, 429/global/shared limits, 401,
request/handshake deadlines, missing ACK, fatal close and cancellation during
network operations. Authenticated Discord smoke tests remain owner-initiated.
