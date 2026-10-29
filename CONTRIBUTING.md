# Contributing

Check the [current scope](docs/ROADMAP.md) before adding features. v0.3.0 includes a
live message transport; small, tested changes to async behavior and API clarity are useful.

Use C++20 and the existing namespace/style. Keep simple bot code concise while
documenting ownership and advanced access. Take owned event/Context values in
coroutines; do not retain references to temporary callback arguments. Never
block the event-loop thread for network I/O. Keep command and Cog lifetimes safe
across suspension and registry changes.

Build/test with the [build guide](docs/BUILDING.md), then run:

```sh
python scripts/check_release.py
python -m unittest discover -s tests -p test_release.py -v
```

For package-facing changes also install and run `tests/consumer`, and use
`conan create` where appropriate. Change docs/examples with API changes. Tests
should cover behavior or meaningful failure cases rather than duplicate code.
Do not add token-dependent CI tests, generated build files or credentials.

Use the issue/PR templates to describe the concrete problem, final behavior and
validation. Keep release versions unchanged in ordinary feature PRs; the
maintainer updates versions using [the release process](docs/RELEASING.md).
Project-owned contributions use LGPL-3.0-or-later, with the existing copyright
and SPDX notices. Third-party files retain their original licenses.
