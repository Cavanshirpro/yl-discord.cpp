# Files and attachments

FileUpload owns immutable shared byte storage and a per-request filename,
description and content_type. Copies share the bytes while owning separate
metadata, so passing MessageOptions by value does not copy a large file buffer.
There is no automatic filesystem read, URL download, transcoding or content sniffing.

```cpp
MessageOptions options;
options.files.push_back(FileUpload::text("report.txt", "Status: ready\n"));
auto sent = co_await ctx.reply(options);
```

For binary data, construct FileUpload(filename, std::vector<std::uint8_t>) and
move your vector into it. `bytes()` exposes a read-only span and `size()` reads
its byte count. FileUpload::text(filename, string_view) copies the supplied bytes
once and sets text/plain; charset=utf-8. Embedded NULs are preserved; file bytes
are never interpreted as JSON or validated as text. A zero-byte file is allowed.

Client, Context and MessageOptions use the same file API for create/reply.
`Message::attachments` contains response metadata: ID, filename, description,
content type, uint64 size, URL, proxy URL and ephemeral flag. Optional string
metadata is represented as an empty string. Attachment URLs can expire; fetch
the message again for refreshed metadata when needed. No download is triggered
by reading a URL. See [upload_bot.cpp](../examples/upload_bot.cpp) for an offline example.

## Limits and metadata

The typed upload policy caps each file at 20 MiB, combined new file bytes at
24 MiB, and total attachments at 10. FileUpload construction checks its byte cap
before adopting the buffer. Sending/editing validates counts and the combined cap.
The live multipart builder also checks a 25 MiB request budget with 64 KiB reserved
for MIME framing, so unusually large JSON metadata can reduce the available file
budget. Discord may apply a lower or different account/channel limit and returns
HttpError when it refuses a request. These are conservative project limits,
not a guarantee that every server accepts the largest typed payload.

Upload filenames must be distinct among the new files, nonempty UTF-8 basenames
of at most 255 bytes. Slashes, backslashes, drive colons, double quotes, ASCII
controls and standalone dot/dot-dot names are rejected. Descriptions allow up to
1024 conservative UTF-16 units. Content types require a printable ASCII MIME
type of at most 128 bytes; parameters are allowed and the default is
application/octet-stream. Validation errors do not include rejected names/bytes.

For a spoiler attachment, supply the conventional SPOILER_ filename prefix.
To reference uploaded media from an Embed, use attachment://filename in its
image/thumbnail/icon URL. Discord determines supported media types and rendering;
the library does not inspect the binary format. Use simple URL-safe names when
embedding attachment URLs.

## Explicit attachment edits

```cpp
MessageEditOptions edit;
edit.retained_attachment_ids = std::vector<Snowflake>{};
for (const auto& attachment : sent.attachments)
    edit.retained_attachment_ids->push_back(attachment.id);
edit.files.push_back(FileUpload::text("extra.txt", "More details\n"));
auto updated = co_await sent.edit(edit);
```

When new files are present, retained_attachment_ids **must** be explicit. This
prevents an accidental loss of older files. Include every old ID you want to keep;
an engaged empty vector replaces all old files with the new ones. An engaged
empty vector with no new files removes all attachments. Disengaged retention
with no new files preserves existing attachments. Retained IDs must be distinct
and nonzero; the retained and new counts together must not exceed 10.

Attachment-only edits omit content, embeds, components and flags. All normal
mention defaults still apply. Message results are new snapshots. If another actor
changes the message concurrently, Discord remains authoritative; fetch a fresh
snapshot before choosing the retention list when necessary. Changing metadata
of retained attachments is outside this initial typed subset.

## Transport behavior

Live sends and file edits use libcurl MIME parts: payload_json plus uniquely
named files[n] parts. Read/seek callbacks stream directly from the immutable
buffers through the shared curl multi engine, without constructing another full
multipart byte string. Callbacks perform memory copies only; file/network reads
are not performed inside them. MIME state owns its callback data until the
transfer is detached and destroyed.

Only a server 429 triggers an automatic replay. Each retry creates fresh MIME
cursors over the same bytes. Uncertain write failures are not retried by the
library. TLS, route/bucket/global waits, deadlines, User-Agent versioning and
close cancellation use the existing REST machinery. Raw RestClient::request()
remains JSON-only; use the typed message methods for multipart uploads.

Custom transports now receive MessageRequest::files and structured edit
retention/file fields. They must implement those capabilities deliberately.
Older text-edit overrides reject attachment edits through the default fallback.
Rebuild consumers for this minor because public layouts changed.

MemoryTransport generates synthetic attachment metadata, preserves/clears retained
IDs and records upload buffers in sent()/edit action logs. Unknown retained IDs
throw std::out_of_range before changing a stored message. It does not store a
downloadable file service or emulate Discord's media/permission checks.

The loopback tests parse real multipart bytes, including NUL/binary data and UTF-8
filenames, replay a 429, verify retention/append/removal and cancel a pending
8 MiB upload. No real credential or Discord server is used.

References: [Discord uploads](https://docs.discord.com/developers/reference#uploading-files),
[message edits](https://docs.discord.com/developers/resources/message#edit-message),
[libcurl callback MIME data](https://curl.se/libcurl/c/curl_mime_data_cb.html).
