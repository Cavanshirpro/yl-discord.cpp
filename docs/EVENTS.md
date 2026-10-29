# Events and awaitable waits

Client offers class hooks, ordinary listeners, removable subscriptions and
predicate-based waits. These observe events independently: waiting for a message
does not consume it, disable commands or hide it from another matching waiter.

## Await a future event

```cpp
using namespace std::chrono_literals;
auto answer = co_await bot.wait_for<Message>(30s,
    [author = ctx.message.author.id, channel = ctx.message.channel_id](const Message& message) {
        return !message.author.bot && message.author.id == author
            && message.channel_id == channel && message.content == "yes";
    });
co_await answer.reply("Confirmed.");
```

`wait_for<Event>(timeout = 30s, predicate = {}, stop_token = {})` supports Message,
Ready and RawEvent. An omitted predicate accepts the next event of that type.
Predicates return bool synchronously; keep them short and nonblocking. They run
on the Client's event-loop thread. Take owned captures or keep referenced data
alive for the whole wait. Message results retain their bound message transport.

Like other tasks, the wait is lazy. Registration and the deadline start when the
task is awaited/spawned, not when it is constructed. It sees future dispatches
after registration and has no event history or cache replay. All matching waits
can receive the same event. Ready waits observe dispatches, not cached login state.
The complete [offline confirmation example](../examples/wait_for_bot.cpp) shows
waiting from a command and replying to the matching message.

## Timeout and cancellation

Timeout must be positive and no greater than 24 hours. A deadline uses the
monotonic steady clock; time continues to elapse while Client::stop() pauses the
event loop. Expiry throws EventTimeout. A predicate failure reaches that waiter
with its original exception; other waiters, hooks and listeners keep running.

```cpp
std::stop_source cancellation;
// Pass cancellation.get_token() as the third argument to wait_for().
// Another thread can later call cancellation.request_stop().
```

A pre-requested or subsequently requested stop token throws
`asio::system_error(asio::error::operation_aborted)`. Cross-thread stop requests
are posted onto the wait's executor before changing its state. Native Asio
coroutine cancellation is also supported by the underlying timer. Cancellation,
a match and timeout complete the wait once; the first processed completion wins.
An event processed at or after the deadline cannot satisfy the predicate.

`Client::close()` cancels pending waits as well as live network work. Further
waits on that closed Client are rejected with operation_aborted; construct a new
Client for a fresh lifecycle. Client's existing shutdown handling suppresses
uncaught operation_aborted errors during close. An explicit stop-token
cancellation outside close still reaches the caller/error hook normally.
Use `is_cancelled(error.code())` when inspecting an Asio system_error from a
shared library. Header-only Asio can have separate error-category singleton
addresses in a Windows DLL and its executable; the helper recognizes their
equivalent operation_aborted codes without relying only on pointer identity.

Await on the Client's event loop (an Asio strand over the same loop is accepted).
A foreign I/O context is rejected before registering a wait. Never run one Client
on multiple threads concurrently. Client must remain alive while its tasks run.
Destroying a paused Client discards pending Asio frames and releases predicate
captures; it does not execute application callbacks from its destructor.

## Removable listeners

```cpp
auto subscription = bot.subscribe_message([](Message event) -> task<> {
    // Observe this event independently of command handling.
    co_return;
});
subscription.disconnect();
```

`subscribe_message()`, `subscribe_ready()` and `subscribe_raw()` return a
ListenerConnection. `connected()` reads its state; `disconnect()` is idempotent.
Copies refer to the same registration. Dropping a handle does **not** disconnect
it; keep a handle if you want to remove that registration later. Existing
`listen_*()` methods retain their chainable Client& return and stay registered
until Client destruction.

Disconnecting releases the registry's callback ownership immediately. Callbacks
already snapshotted for an event can still finish, with captured state retained
through suspension. Later events omit disconnected registrations. Snapshots are
taken before wait predicates execute, so predicates can add/remove listeners
without invalidating the current event's delivery. Additions observe future events.
A handle is weak: it is safe to query/disconnect after Client destruction and
cannot keep the Client or its callbacks alive.

Subscription mutation and connected() queries belong on the Client executor or
before/after run(), like command registry changes. ListenerConnection is not a
cross-thread cancellation handle; use a stop token for that purpose.

These facilities do not implement bounded event queues, a persistent event log,
async predicates or automatic Cog background-task management. Callers still own
their application timers/signals and any tasks unrelated to event waits.
