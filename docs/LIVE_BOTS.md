# Live bot setup

v0.3.0 supports bot-token login, Gateway API v10, messages and REST replies.
Create a Discord application/bot in the Developer Portal and install it in your
test server with permission to view the channel, send messages and read history
for replies. Enable Message Content Intent on its Bot page for guild prefix
commands, and request that intent in code.

```cpp
using namespace yl::discord;
Bot bot{BotOptions{.intents = Intents::defaults().with(Intent::message_content)}};
bot.command("ping", [](Context ctx) -> task<> {
    co_await ctx.reply("Pong!");
});
bot.run(token_from_env());
```

See [ping_bot.cpp](../examples/ping_bot.cpp) for a complete program with Ctrl+C
shutdown and an echo command. The library also preserves the
[offline examples](../examples/README.md) for credential-free testing.

## Run the example

Build the project, then provide the raw bot token through DISCORD_BOT_TOKEN.
The examples and release tooling do not read a local credential file.

PowerShell:

```powershell
$secret = Read-Host 'Bot token' -AsSecureString
$env:DISCORD_BOT_TOKEN = [Net.NetworkCredential]::new('', $secret).Password
try { ./build/mingw64/yl_ping_bot.exe }
finally { Remove-Item Env:\DISCORD_BOT_TOKEN }
```

Bash/MSYS2:

```bash
read -rs -p 'Bot token: ' DISCORD_BOT_TOKEN; echo
export DISCORD_BOT_TOKEN
./build/default/yl_ping_bot
unset DISCORD_BOT_TOKEN
```

On Windows append `.exe` and keep the matching MSYS2 runtime on PATH. In the
test channel send `!ping` or `!echo "hello world"`. Use Ctrl+C to close. Bot-authored
messages do not invoke commands. Outgoing messages/replies disable automatic
mentions by default. The live example is compiled in CI but never run there.

## Connection behavior

The client fetches `/gateway/bot`, checks the identify budget and starts a JSON
WebSocket session. Heartbeats use the server interval and initial jitter. Missing
ACK, resumable closes and reconnect requests resume with session/sequence state;
invalid sessions can identify again. Fatal close codes, including invalid auth
and disallowed intents, stop. Repeated short-lived failures exhaust a bounded
reconnect budget; healthy minute-long sessions reset it.

HTTP uses TLS verification, deadlines and libcurl multi without blocking the
event-loop thread. A shared pump runs only while transfers are active; idle
Gateway reads use socket readiness and heartbeat timers. REST responses learn
route/shared-bucket/global rate limits; 429 retries wait asynchronously. A 401
stops later requests with the same REST client. Uncertain POST failures are not
automatically retried, avoiding accidental duplicate sends.

`close()` ends network work and interrupts HTTP/rate-limit waits. `stop()` only
pauses the loop. Application-owned timers/signals remain the application's
responsibility; the complete example cancels its signal wait on Gateway failure.
Construct a new Client to restart after close.

## Current limits and validation

No slash commands, components, voice, compression, sharding or broad guild/member
API exists yet. A bot requiring multiple shards is rejected explicitly. The
initial model surface remains User/Message/Ready; raw events and low-level REST
requests provide additional access through the same runtime/rate-limit handling.

HTTP/WebSocket fixture tests cover actual frame/JSON traffic, delayed replies,
heartbeat liveness, reconnect/resume, fragments, fatal closes, timeouts and
shutdown. An authenticated Discord smoke test is a separate owner-initiated
step; no real Discord credential is used by the automated suite.

References: [Discord Gateway](https://discord.com/developers/docs/events/gateway),
[intents](https://discord.com/developers/docs/events/gateway#gateway-intents),
[rate limits](https://discord.com/developers/docs/topics/rate-limits).
