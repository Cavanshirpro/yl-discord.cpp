# Architecture

The high-level API uses familiar Client/Bot, Message, Context and Cog concepts.
Ordinary C++ functions, lambdas and virtual methods replace Python decorators.
The same command code runs with built-in networking or MemoryTransport.

| Layer | Responsibility | v0.1.1 |
| --- | --- | --- |
| User API | Hooks, listeners, prefix commands and Cog | Implemented |
| Models | Owned User/Message/Ready, Snowflake and intents | Initial scope |
| Runtime | Coroutine tasks, timers, executor and errors | Standalone Asio |
| Message transport | Awaitable message/reply port | REST and memory implementations |
| REST | Verified TLS, deadlines, buckets/global limits | libcurl multi |
| Gateway | WebSocket, heartbeat and session resume | JSON API v10 |
| Broader state | Cache, partial guild models, shard coordination | Planned |

## Ownership and execution

`task<T>` aliases `asio::awaitable<T>`. Tasks are lazy; await or spawn them.
Events/Context are owned values. Parent frames keep registered callback closures
alive across suspension and command removal. Command entries retain their Cog.
User-captured external references must still outlive use.

`send()`/`reply()` snapshot input and bind returned messages to their transport.
An unbound manually constructed Message cannot reply until Client dispatch binds
it. Pending network frames retain connection/WebSocket state through shutdown
and context destruction; socket observers release borrowed native handles before
libcurl cleans them up.

One event-loop thread drives a shared curl multi engine for concurrent HTTP
transfers. Its timer pump runs only with active transfers. Completed REST
connections can be reused by curl. Gateway reads/writes wait on socket readiness;
idle connections do not run a polling timer. DNS must be asynchronous in libcurl.
No C++ thread is created per command/request by the library.

## Runtime contract

- `run()` drains current work. `run(token)` starts a live session first.
- `dispatch()` and `close()` are thread-safe ingress; other mutation belongs on
  the executor or before running. Never run one Client concurrently/recursively.
- Hooks, listeners and commands are independent; completion order is unspecified.
- `on_message()` overrides do not disable commands. Bot messages reach listeners
  but do not invoke commands. Unknown names are ignored.
- `close()` cancels network work; application timers/signals remain caller-owned.
  `stop()` pauses the loop and allows another `run()` to resume pending work.
- Destroying Client discards remaining Asio work. Do not run the loop in destructors.
- Blocking I/O/CPU work still blocks the loop; queue bounds/backpressure are planned.

Registry changes preserve in-flight ownership. Cog registration rolls back on
name conflicts. Removing any alias removes the whole command. Listener snapshots
apply at event dispatch time. Default errors reach `on_error()` and then `run()`;
additional failures can be retrieved with `take_errors()`.

## REST and Gateway

REST serializes each learned route/bucket, includes major-resource IDs in bucket
scope, and coordinates a global deadline. Server headers/body determine waiting.
Only 429 is retried automatically; 401 disables further requests. Response bodies
are bounded after decompression. Authentication is never included in error text.

Gateway tracks sequence, session ID and resume URL, and keeps heartbeats separate
from HTTP waits. Missing ACK causes reconnect. Fatal close codes stop; invalid
sessions can re-identify. Handshake/request deadlines and reconnect budgets bound
failures. TLS is mandatory except for explicitly configured loopback fixtures.

See [live behavior](LIVE_BOTS.md), [the API](API.md) and [the roadmap](ROADMAP.md).

References: [Asio coroutines](https://think-async.com/Asio/asio-1.30.2/doc/asio/overview/composition/cpp20_coroutines.html),
[libcurl multi](https://curl.se/libcurl/c/libcurl-multi.html),
[Discord Gateway](https://discord.com/developers/docs/events/gateway),
[Discord rate limits](https://discord.com/developers/docs/topics/rate-limits).
