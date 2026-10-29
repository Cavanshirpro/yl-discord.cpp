# Roadmap

Future items are plans, not release dates or current capabilities.

## Foundation — v0.1.0

- [x] C++20 coroutines, timers and one event-loop thread.
- [x] Client/Bot subclasses, async hooks and multiple listeners.
- [x] Prefix commands, aliases, quoted arguments and Context helpers.
- [x] Cog ownership and in-flight command removal safety.
- [x] Raw event ingress and a replaceable message transport.
- [x] Offline behavior tests and examples.
- [x] CMake install/export, package recipes and publication workflows.

## Live messages — v0.1.1

- [x] Nonblocking TLS HTTP and WebSocket transports.
- [x] Bot-token authentication without credentials in logs.
- [x] Gateway discovery, API v10, Hello, Identify and Ready.
- [x] Heartbeat jitter, ACK tracking and immediate heartbeat responses.
- [x] Sequence/session persistence, reconnect/resume and close-code policy.
- [x] JSON conversion for supported User/Message/Ready models.
- [x] MESSAGE_CREATE → Bot → awaitable reply → REST send.
- [x] Server-driven bucket/global limits and 429 waiting.
- [x] Request/handshake deadlines, bounded retries and network shutdown.
- [x] Local HTTP/WebSocket integration fixtures.
- [ ] Owner-initiated authenticated Discord smoke test.

Fixtures cover REST waiting without blocking heartbeats, reconnect on missing
ACK, Resume, 429 waiting, fatal authentication close and network cancellation.
Guild prefix commands require message content intent in code and in the Developer
Portal. Compression and generic task-group cancellation remain future work.

## Everyday bot features

- [ ] Slash commands, sync, interaction response/defer/followup.
- [ ] Buttons, selects, modals and Views.
- [ ] Guild, channel, member, role and permission APIs.
- [x] Message fetch/edit/delete and own-reaction helpers.
- [ ] Embeds and attachments.
- [x] Typed parameters, converters, checks, cooldowns and generated help.
- [x] Cog-wide command removal with in-flight ownership.
- [ ] Cog lifecycle hooks and managed background tasks.

## Advanced control

- [ ] Cache budgets, opt-out, TTL and eviction policy.
- [x] Predicate-based `wait_for<Event>()` with cancellation/deadlines.
- [x] Removable Message/Ready/RawEvent subscriptions with in-flight ownership.
- [ ] Event/command hooks and optional middleware.
- [ ] REST latency, heartbeat and rate-limit queue metrics.
- [ ] Low-level REST/Gateway access using the same safeguards.
- [ ] Sharding, identify coordination, queue bounds and backpressure.

Offline testing already has a working foundation. Event recording/replay,
typed command signatures, explicit cache budgets and opt-in diagnostics are
candidates for improving debugging and large-bot operation. Recording needs
a deliberate policy for credentials, message content and retention.

## Performance evidence

After live transports work, compare equivalent workloads, intents and caches
against discord.py on the same machine. Measure parse/dispatch throughput,
command latency p50/p95/p99, CPU, RSS, build time and distribution size. Separate
network latency and Discord limits from CPU benchmarks. No advantage is claimed
before measurements exist.
