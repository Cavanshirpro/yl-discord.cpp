# Architecture

The goal is a short path from a small async bot to an application that controls
its transports, event flow, cache budget and execution policy. The high-level
API uses familiar concepts: `Client`, `Bot`, `Message`, `Context` and `Cog`.
C++ functions, lambdas and virtual methods replace Python decorators.

| Layer | Responsibility | v0.1.0 |
| --- | --- | --- |
| User API | Events, prefix commands, Context and Cog | Implemented |
| Models | Owned User/Message values, Snowflake, intents | Small initial scope |
| Runtime | Spawn, timers, completion and errors | Standalone Asio |
| Message transport | Awaitable send/reply port | Interface and memory implementation |
| REST | HTTP/TLS, buckets, retries and timeouts | Planned |
| Gateway | WebSocket, heartbeat, identify/resume | Planned |
| State | Partial models, cache and shard coordination | Planned |

`MessageTransport` covers message sending only; it is not a general HTTP client.
A future REST implementation can satisfy the same port without making commands
depend on a test implementation.

## Coroutine ownership

`task<T>` is `asio::awaitable<T>`. Tasks use Asio's frames, executors and I/O
operations. They are lazy: await or spawn them. `sleep_for()` suspends a steady
timer without creating a thread per task.

Events and Context are passed by value. Registered callbacks are kept alive in
parent coroutine frames so suspended coroutine lambdas retain their closures.
Command entries own their Cog until the command finishes, even after removal
from the registry. Caller-owned captured references must still outlive use.

`send()`/`reply()` snapshot their input before returning a lazy task. Returned
messages retain their transport, so they can be replied to again. Constructed
messages acquire a transport through `Client::dispatch()`; replying to an unbound
message raises `std::logic_error` when awaited.

## Runtime contract

- `run()` drains queued work and timers; it returns when no work remains.
- `dispatch()` is thread-safe ingress. Other mutation belongs before `run()` or
  on its executor. Do not run a Client concurrently from several threads.
- Hooks, listeners and commands are independent tasks. Completion order is not
  guaranteed; `on_ready()` is not a barrier before message processing.
- `stop()` pauses the loop without cancelling tasks. Another `run()` can resume them.
- Destroying the Client destroys remaining Asio work. Do not run the loop from
  subclass/Cog destructors. Graceful cancellation is planned.
- Blocking I/O and CPU work still block the loop. Queue bounds/backpressure are
  not implemented yet.

`executor()` exposes Asio directly. This is an intentional public dependency,
not a promise of runtime independence. A future Gateway adapter must keep an
Asio work item alive during its connection; today's `run()` does not wait forever
for future input.

## Events and errors

Bot forwards messages to both listeners and command processing. Overriding
`on_message()` does not disable commands. Bot-authored messages reach listeners
but never invoke commands. Unknown commands are ignored. Malformed argument
quoting/escaping raises `ArgumentError` through the error hook.

Names/aliases are case-sensitive and duplicate registrations are rejected.
`add_cog()` rolls back its registrations on conflict. Removing by any alias
removes every name for that command while preserving in-flight ownership.
Listeners use a snapshot per event; changes affect subsequent event snapshots.

Failures reach `on_error(TaskError)`. The default hook rethrows. After work drains,
`run()` throws the first unhandled error; additional errors can be retrieved with
`take_errors()`. A custom hook can handle/log failures instead.

## References

- [Asio C++20 coroutines](https://think-async.com/Asio/asio-1.30.2/doc/asio/overview/composition/cpp20_coroutines.html)
- [discord.py commands](https://discordpy.readthedocs.io/en/stable/ext/commands/commands.html)
- [Official Discord Gateway protocol](https://github.com/discord/discord-api-docs/blob/main/developers/events/gateway.mdx)
- [Official Discord rate limits](https://github.com/discord/discord-api-docs/blob/main/developers/topics/rate-limits.mdx)
