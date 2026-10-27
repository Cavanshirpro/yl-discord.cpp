# Releasing

Versions and tags use [MAJOR.MINOR.PATCH](VERSIONING.md). Releases are produced
from one exact commit. Source archives use committed files only; uncommitted
working-tree edits are intentionally not packaged.

## Prepare

1. Complete the code/docs and run core and installed-consumer tests.
2. Update VERSION, release.json, the vcpkg manifest, CHANGELOG and README. The
   [version helper](VERSIONING.md) updates machine-readable versions and notes.
3. Keep `release.md` byte-identical to `docs/releases/<version>.md`.
4. Run `python scripts/check_release.py` and the [publication tests](TESTING.md).
5. Commit/push to main; wait for the build and packages workflows to pass.

For the initial release the prepared version/tag are `0.1.0` / `v0.1.0`.
After checking the exact commit, create and push an annotated tag:

```sh
git tag -a v0.1.0 -m "yl-discord.cpp v0.1.0"
git push origin v0.1.0
```

## Automatic publication

The `publish` workflow runs on `v*` tags or manually for an existing tag. It:

1. Validates the exact tag against VERSION, metadata and the checked-out commit.
2. Freezes that commit for all downstream checkouts.
3. Runs the reusable build/test and package workflows.
4. Collects source, vcpkg and native MSYS2 artifacts from that run.
5. Generates the commit-pinned MSYS2 source recipe and final SHA256SUMS.
6. Validates source contents and creates/uploads a draft release.
7. Publishes the draft only after uploads succeed.
8. Calls the Conan upload workflow if `CONAN_REMOTE_URL` is configured.

Expected assets:

- `yl-discord.cpp-v<version>.zip` and `.tar.gz`
- `yl-discord.cpp-v<version>-vcpkg-overlay.zip`
- `yl-discord.cpp-v<version>-msys2-recipe.zip`
- MINGW64 and UCRT64 native `.pkg.tar.zst` packages
- `release.md` and `SHA256SUMS`

The release script can recover an unfinished draft but refuses to overwrite an
already-published release. A published fix needs a new PATCH version. If a job
fails before publication, fix the source on a new commit and use a new tag if
the original tag has already been published. Do not disable checks to publish.

The publication workflow calls reusable workflows directly. It does not depend
on a release event starting another workflow, because events created with the
repository GITHUB_TOKEN generally do not trigger additional workflow runs.

## Validate assets locally

After committing:

```sh
python scripts/package_source.py --ref HEAD
python scripts/check_release.py --archive build/dist/yl-discord.cpp-v0.1.0.tar.gz
```

Overlay/MSYS2 generators additionally require the commit archive to be reachable
on GitHub. Package builds run installation consumers, not only compilation.
See [publication configuration](PUBLISHING.md) for permissions and credentials.
