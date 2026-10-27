# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Publish verified release assets via gh, refusing to overwrite published releases."""
import json
import os
import subprocess
from check_release import ROOT, require


def main():
    metadata = json.loads((ROOT / "release.json").read_text())
    tag = metadata["tag"]
    require(tag == os.environ["RELEASE_TAG"], "Release tag mismatch")
    assets = sorted((ROOT / "build" / "dist").iterdir())
    require(all(path.is_file() for path in assets), "Unexpected asset directory")
    expected = {f"yl-discord.cpp-{tag}.zip", f"yl-discord.cpp-{tag}.tar.gz",
                f"yl-discord.cpp-{tag}-vcpkg-overlay.zip", f"yl-discord.cpp-{tag}-msys2-recipe.zip",
                "release.md", "SHA256SUMS"}
    require(expected <= {path.name for path in assets}, "Required release assets are missing")
    require(len([path for path in assets if path.name.endswith(".pkg.tar.zst")]) == 2,
            "Expected both MINGW64 and UCRT64 packages")
    existing = subprocess.run(["gh", "release", "view", tag, "--json", "isDraft"], capture_output=True, text=True)
    if existing.returncode == 0:
        require(json.loads(existing.stdout)["isDraft"], "Published release will not be overwritten; publish a new PATCH version")
    else:
        require("not found" in existing.stderr.lower() or "404" in existing.stderr,
                "Unable to determine existing release status")
        subprocess.run(["gh", "release", "create", tag, "--verify-tag", "--draft",
                        "--title", f"yl-discord.cpp {tag}", "--notes-file", str(ROOT / "release.md")], check=True)
    subprocess.run(["gh", "release", "upload", tag, "--clobber", *(str(path) for path in assets)], check=True)
    pre = str(metadata["prerelease"]).lower()
    subprocess.run(["gh", "release", "edit", tag, "--draft=false", f"--prerelease={pre}",
                    f"--latest={str(not metadata['prerelease']).lower()}",
                    "--title", f"yl-discord.cpp {tag}", "--notes-file", str(ROOT / "release.md")], check=True)


if __name__ == "__main__":
    main()
