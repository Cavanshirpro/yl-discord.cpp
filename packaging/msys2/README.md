# MSYS2 packages

GitHub Releases provide project-built MINGW64 and UCRT64 packages. Use the
package matching your active MSYS2 terminal. For MINGW64:

```sh
pacman -U ./mingw-w64-x86_64-yl-discord-cpp-0.1.1-1-any.pkg.tar.zst
```

For UCRT64 use `mingw-w64-ucrt-x86_64-yl-discord-cpp-0.1.1-1-any.pkg.tar.zst`.
Pacman installs declared Asio, curl, JSON and GCC runtime dependencies. The package contains
the library DLL/import library, headers, CMake/pkg-config metadata and licenses.
Use `find_package(ylDiscord 0.1 CONFIG REQUIRED)` and `yl::discord` in CMake.

The recipe ZIP contains a generated PKGBUILD with an exact upstream commit and
SHA512. In an MSYS2 terminal with base-devel and the matching compiler installed,
extract the recipe and run `makepkg-mingw --syncdeps --cleanbuild`, then install
the resulting package with `pacman -U`.

The template `PKGBUILD.in` is not directly buildable. From a source checkout whose
commit is already on GitHub, `python scripts/make_msys2_recipe.py` generates the
recipe under `build/msys2/mingw-w64-yl-discord-cpp`.

These packages are hosted by this project. They are not a central MSYS2 repository
listing. The library supports live bot messages and offline test transports.
