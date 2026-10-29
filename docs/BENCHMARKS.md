# Offline dispatch benchmark

The optional benchmark measures prebuilt incoming Message dispatch, a prefix
`!ping` command and an awaitable in-memory `Pong!` reply. It records throughput,
batch-sensitive p50/p95/p99 completion latency and peak resident process memory.
It measures a synthetic framework path, not live Discord performance.

## Recorded sample — 2026-10-29

One Windows 11 AMD64 development desktop run used 10,000 events per process,
batch size 128, five fresh-process repeats and alternating implementation order.
Other applications were running, so this is an uncontrolled local sample.
C++ used GCC 16.2.0, shared Release with -O3; Python used CPython 3.13.5 and
discord.py 2.7.1. The source tree was clean at commit
`54fce8924f2bb7d33d3ea58ca181a2dd4be5bdd8`.

Medians across those five processes:

| Implementation | Commands/s | p50 (µs) | p95 (µs) | p99 (µs) | Peak resident (MiB) |
| --- | ---: | ---: | ---: | ---: | ---: |
| yl-discord.cpp 0.4.0 | 219,296 | 226.1 | 556.0 | 763.7 | 13.04 |
| discord.py 2.7.1 | 24,072 | 2,260.1 | 4,711.3 | 7,755.1 | 67.79 |

[All raw samples and environment metadata](benchmarks/2026-10-29-windows-offline.json)
are retained. These numbers apply only to the fixture below. Model/feature sets,
startup costs, retained fixture storage and instrumentation differ; this table
does not establish equivalent live-bot performance or general superiority.

### Batch-size sensitivity

Two additional runs on the same desktop used 10,000 events per process and three
fresh-process repeats at clean commit
`f1c837c46c313c1d56e6fc4cd1bb8f73e3f650d2`. The library implementation was unchanged;
the desktop was still uncontrolled. Medians were:

| Batch size | Implementation | Commands/s | p50 (µs) | p95 (µs) | p99 (µs) |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | yl-discord.cpp 0.4.0 | 200,905 | 2.2 | 4.3 | 9.1 |
| 1 | discord.py 2.7.1 | 13,575 | 39.6 | 107.5 | 224.8 |
| 32 | yl-discord.cpp 0.4.0 | 244,827 | 49.3 | 109.3 | 185.9 |
| 32 | discord.py 2.7.1 | 19,990 | 739.2 | 1,676.1 | 3,006.0 |

The [batch-1 samples](benchmarks/2026-10-29-windows-batch1.json) and
[batch-32 samples](benchmarks/2026-10-29-windows-batch32.json) include all memory
measurements and environment metadata. Batched dispatch queues work before each
drain, so its completion latency includes that queueing. These values are not
network response latency. Batch size, process variation and fixture behavior must
be kept explicit when comparing or repeating the results.

## Reproduce

The benchmark tools were added after the immutable v0.4.0 release tag. Use a
checkout of `54fce8924f2bb7d33d3ea58ca181a2dd4be5bdd8` or a later main commit
containing `benchmarks/`; the v0.4.0 release source archive does not include them.

Configure with the same [dependencies](BUILDING.md) as the library, using Release:

```sh
cmake -S . -B build/bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DYL_DISCORD_BUILD_BENCHMARKS=ON
cmake --build build/bench --target yl_offline_benchmark
python -m venv build/benchmark-venv
# Linux/macOS; use build/benchmark-venv/Scripts/python.exe on Windows:
build/benchmark-venv/bin/python -m pip install -r benchmarks/requirements.txt
build/benchmark-venv/bin/python benchmarks/run.py --cpp build/bench/yl_offline_benchmark \
  --iterations 10000 --batch-size 128 --repeats 5 --output build/offline-results.json
```

For the local MINGW64 preset, keep its runtime on PATH. Multi-config generators
need `--config Release` and the corresponding executable path. Custom shared curl
prefixes also need the [runtime search path](BUILDING.md) configured. The benchmark
target is OFF by default and is not installed as part of the production package.

The Python adapter is pinned to discord.py 2.7.1 and the recorded dependency set.
Python >= 3.10 is supported by the harness; record the actual runtime version with
the results. Updating the comparator requires consciously reviewing the adapter
and dependency pins. No voice dependencies are required.

## Exact scope

Both implementations use prefix `!`, the common intent mask 4609, no automatic
help command, user 7 in channel 42, and mention parsing/reply pings disabled.
Inputs are prebuilt outside each timed interval. Dispatch/drain timing includes
per-event start timestamps, command handling, reply construction, fixture storage
and completion bookkeeping. Warmup uses up to 256 events before the measured run.
Each batch drains before the next one; total elapsed is the sum of those measured
dispatch/drain intervals. Latency starts at each dispatch and ends after its reply
returns. Batch size directly affects the latency distribution.

C++ uses Bot::dispatch(), Context::reply() and MemoryTransport, retaining send
requests and reply snapshots. Python uses Bot.dispatch(), its ordinary on_message/
command path and Context.reply(); a synthetic HTTP send seam records the payload
and returns local message data. The resulting Message is retained by the harness.
A fixed nonce avoids unrelated random-number generation in the Python reply.

The Python fixture seeds a local ClientUser and uses a local channel descriptor.
These pinned adapter seams are not login or application setup recommendations.
HTTP request and WebSocket methods are replaced with guards that fail if called.
The C++ MemoryTransport makes no network calls. There is no token environment
read, account login, Guild cache population or real REST endpoint in either run.

The runner alternates process order on successive repeats and saves every raw
sample plus median metrics. It records source commit, dirty state, versions,
configuration, platform and architecture without recording a hostname or local
workspace paths. A clean source tree should be used for published measurements.

## Interpretation

Peak memory is the process peak working set on Windows or peak RSS on Unix, in
bytes. It includes runtime/import/startup memory, loaded dependencies, warmup,
fixture storage and measurement arrays. It is not retained heap size or an
incremental per-bot memory figure. CPU utilization and build time are not measured.

The model and feature sets differ: discord.py has much broader Message/state/
command capabilities. Its objects, payload generation, caches and allocator are
not identical to this C++ library. Synthetic fixture storage also differs. This
is a common operation with different framework implementations, not proof of
feature parity or a complete application benchmark.

Gateway parsing, TLS, HTTP scheduling, rate limits, real network latency, broad
guild/member caches, voice, sharding and long-running memory behavior are excluded.
No universal faster/lighter claim should be inferred from these numbers. Repeat
on controlled hardware and equivalent live workloads before making product claims.
