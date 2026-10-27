# Examples

Both programs use `testing::MemoryTransport` and make no Discord connection.
Build them with the [project instructions](../docs/BUILDING.md).

- [offline_bot.cpp](offline_bot.cpp): async ping/echo commands, aliases, quoted
  arguments, and a nonblocking timer. Echo completes while ping is waiting.
- [class_bot.cpp](class_bot.cpp): Bot event overrides, a Cog with class state,
  and an error hook. `on_message()` does not disable registered commands.

Run from the selected build directory:

```sh
./build/default/yl_offline_bot
./build/default/yl_class_bot
```

On Windows the filenames end in `.exe`; for the PowerShell preset replace
`default` with `mingw64`. Keep the matching runtime/DLL directory on PATH.

These examples receive simulated events through `dispatch()` and print recorded
responses. There is no `run(token)` overload or environment-token loader in
v0.1.0. See [the API](../docs/API.md) for extension/ownership rules.
