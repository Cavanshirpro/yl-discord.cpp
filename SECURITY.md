# Security policy

The 0.4.x line is experimental. v0.4.0 includes bot-token REST/Gateway networking with TLS verification, deadlines
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

Command checks are application authorization logic; hidden help entries do not
restrict command access. Cooldown state has bounded keys and never evicts an
active bucket to admit another caller. Discord permissions still govern all live
message operations. Keep dependencies patched and rebuild consumers for this minor.

Typed rich messages suppress mentions by default; enabling parse flags or ID
whitelists is an explicit application choice. Typed validation rejects malformed
UTF-8 and bounds message/embed fields before HTTP. Low-level raw REST remains
caller-validated. Event predicates run on the event loop and must not block it.

Uploads use caller-supplied immutable buffers. Filename/header checks, per-file/
combined byte caps and a multipart request budget run before transfer. No file is
read from disk or downloaded automatically. Retaining old attachment IDs is an
explicit edit decision; new uploads never silently choose which old files to delete.
