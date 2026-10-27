# Publication configuration

## GitHub Releases

Enable GitHub Actions for this repository. The release job requests
`contents: write`; build/package jobs use read-only contents access. The standard
GITHUB_TOKEN is sufficient for release creation/assets; no personal token is
needed in repository secrets. Actions are pinned by commit and Dependabot checks
their references monthly. Fork PR builds receive no publication credentials.

`release-version.yml` runs when VERSION/release metadata changes on main, creates
the annotated tag and calls `publish.yml` directly. It therefore publishes even
when the tag was created by GITHUB_TOKEN. Manual tag pushes also run publication.
`publish.yml` runs on the exact version tag, calls build/package tests, then
publishes a GitHub release. `workflow_dispatch` can retry an existing unpublished
tag. Source/package artifacts belong to the same run and source commit.

## Optional automatic Conan remote upload

No remote or publishing credentials are bundled or preconfigured. Configure:

| Location | Name | Value |
| --- | --- | --- |
| Repository Actions variable | `CONAN_REMOTE_URL` | HTTPS URL of your writable Conan remote |
| `conan-publish` environment secret | `CONAN_LOGIN_USERNAME` | Remote username |
| `conan-publish` environment secret | `CONAN_PASSWORD` | Scoped password/access token |

Repository secrets with those names can also be used; environment secrets are
preferred when environment protection is desired. The URL must be a repository
variable so the parent workflow can decide whether upload is enabled.
For fully unattended publication do not require manual environment reviewers.

When the URL is absent, GitHub releases still publish and the summary explains
that Conan upload is not configured. When the URL is present, the release
workflow calls `publish-conan.yml` with upload enabled. Missing credentials then
fail explicitly rather than claiming a successful upload.

The Conan workflow first verifies a non-draft GitHub release, rebuilds/tests
static and shared packages, authenticates to the configured remote and uploads
only `yl-discord-cpp/<version>`. It does not upload Asio or other dependencies.
It also supports a manual run with `upload: false` for build-only validation.

GitHub Packages does not provide a general Conan registry. Use a compatible
Conan remote; this automation does not create an external account or infer one.
ConanCenter inclusion is a separate reviewed recipe submission.

## vcpkg and MSYS2

The project automatically generates and tests a vcpkg overlay, then attaches it
to GitHub releases. The source commit and SHA512 are fixed during generation.
Default vcpkg registry acceptance is separate from this project-hosted overlay.

MSYS2 jobs build/test/install both MINGW64 and UCRT64 packages and attach them to
the release. Users install them with `pacman -U`. A central repository listing
requires the upstream MSYS2 process; no `pacman -S` availability is claimed.

## Scope

This repository currently publishes source/package assets and Markdown docs in
GitHub. There is no separately hosted documentation website or automatic central
registry submission. The v0.1.1 library includes live bot REST/Gateway transport.
