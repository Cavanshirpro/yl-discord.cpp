# Examples

Build with the [project instructions](../docs/BUILDING.md).

- [ping_bot.cpp](ping_bot.cpp): **live Discord bot**, DISCORD_BOT_TOKEN login,
  ping/echo prefix commands, class-based Ready hook and Ctrl+C shutdown. Enable
  Message Content Intent. See [live setup](../docs/LIVE_BOTS.md).
- [offline_bot.cpp](offline_bot.cpp): simulated async ping/echo, aliases, quoted
  arguments and a nonblocking timer. Makes no network calls.
- [class_bot.cpp](class_bot.cpp): simulated Bot overrides, a class-state Cog and
  error hook. Makes no network calls.

```sh
./build/default/yl_offline_bot
./build/default/yl_class_bot
# Set DISCORD_BOT_TOKEN before intentionally running:
./build/default/yl_ping_bot
```

On Windows add `.exe`; for the PowerShell preset use `mingw64` instead of
`default` and keep its runtime on PATH. CI compiles all examples, runs only the
two offline examples and uses loopback fixtures for network tests.

The [API](../docs/API.md) explains task ownership and the distinction between
`close()` (network shutdown) and `stop()` (event-loop pause).
