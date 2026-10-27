# Security policy

The 0.1.x line is experimental. v0.1.1 includes bot-token REST/Gateway networking with TLS verification, deadlines
and bounded retries. The broader Discord API remains under development.

Report suspected vulnerabilities through GitHub's private vulnerability report
feature when available:
[report privately](https://github.com/Cavanshirpro/yl-discord.cpp/security/advisories/new).
If private reporting is unavailable, open an issue requesting a private channel
without exploit details or secrets. No private email address or response-time
guarantee is advertised.

Never post Discord tokens, CI credentials or message contents from private
servers. If a credential is exposed, revoke/rotate it at the issuing service;
deleting a commit alone does not revoke it. Build/release tooling excludes local
credentials and scans source assets, but callers must still manage their secrets.

Use the latest patch available for the supported minor line. Report the version,
compiler/platform, reproducible behavior and impact with a redacted example.
