# Releasing

Versions use [MAJOR.MINOR.PATCH](VERSIONING.md). Published tags/assets are immutable;
v0.1.0 remains the historical offline prerelease, and v0.1.1 completes live messages.

## Prepare and publish automatically

1. Complete changes, tests and documentation. Prepare finished versioned notes.
2. Use `python scripts/bump_version.py <version> --notes <notes.md> --stable` for
   a normal release; omit `--stable` to mark a GitHub prerelease.
3. Update CHANGELOG/README, run `scripts/check_release.py`, core/integration tests
   and package consumers.
4. Commit/push VERSION and release metadata to main.

The `release-version` workflow validates metadata, creates the annotated version
tag and **calls `publish` directly**. It does not stop after creating a tag or
depend on GITHUB_TOKEN-generated push events triggering a second workflow.
The full publish workflow builds/tests the frozen source commit, assembles assets,
uploads them to a draft, then sets `draft=false` and the configured prerelease flag.
Normal releases also become the latest release.

Manual tag pushes and manual publication of an existing tag remain supported:

```sh
git tag -a v0.1.1 -m "yl-discord.cpp v0.1.1"
git push origin v0.1.1
```

Use one path per release. A manual `release-version` run is available when metadata
was already committed. An existing tag must point to the exact checked-out commit.
Never move a published tag to a fix; increment the version instead.

## Checks and assets

`publish` validates tag/version/source identity, calls reusable build/package
workflows, collects artifacts from that run, generates the MSYS2 source recipe
and SHA256SUMS, and validates source contents before publication. Assets include:

- Source ZIP and TAR.GZ.
- Commit-pinned vcpkg overlay and MSYS2 source recipe ZIPs.
- Native MINGW64 and UCRT64 packages.
- `release.md` and `SHA256SUMS`.

An interrupted draft can be retried. A complete published release with matching
notes/classification/assets is kept unchanged on retry. An inconsistent published
release fails validation; it is never silently overwritten. Optional Conan upload
runs after GitHub publication when its remote variable is configured.

## Local source archives

After committing:

```sh
python scripts/package_source.py --ref HEAD
python scripts/check_release.py --archive build/dist/yl-discord.cpp-v0.1.1.tar.gz
```

Archives contain committed files only, excluding local credentials/build outputs
and downloaded dependencies. Overlay/recipe generators additionally need the
commit reachable on GitHub. See [publication configuration](PUBLISHING.md) and
[testing](TESTING.md).
