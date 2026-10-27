# Versioning

Package versions use **MAJOR.MINOR.PATCH** with decimal components and no leading
zeros. The first release is **0.1.0**, Git tag **v0.1.0**. `v0.1` is a shorthand
in discussion, not a release tag accepted by the tooling.

- **PATCH:** backward-compatible fixes, documentation and packaging corrections.
- **MINOR:** new functionality; before 1.0 this may also include documented API breaks.
- **MAJOR:** incompatible public API changes after 1.0, or the declared 1.0 milestone.

Before 1.0 there is no cross-minor API/ABI compatibility promise. The installed
CMake package uses SameMinorVersion compatibility; patch updates within a minor
line are intended to remain compatible. Shared library SOVERSION is MAJOR.MINOR.
No cross-compiler/standard-library ABI compatibility is promised.

`VERSION` is the canonical package version, read by CMake and Conan. CMake generates
public version constants. `release.json` adds the exact tag and a GitHub
`prerelease` boolean; the vcpkg manifest and release notes must agree.
The boolean is GitHub release metadata, not an extra semantic-version component.
v0.1.0 is marked as a prerelease because it was an offline foundation. v0.1.1 completes the initial live-message milestone
and is published as a normal GitHub release.

For a later release, prepare completed Markdown notes mentioning the new tag:

```sh
python scripts/bump_version.py 0.1.2 --notes /path/to/completed-notes.md
```

Use `--stable` to set GitHub's prerelease flag to false. The helper refuses an
older/equal version, writes VERSION/release metadata/vcpkg version, and copies
the notes into both `release.md` and `docs/releases/<version>.md`. It does not
commit, tag, push or invent changelog entries. Update README/CHANGELOG, validate
and follow [the release procedure](RELEASING.md). Never move a published tag.
