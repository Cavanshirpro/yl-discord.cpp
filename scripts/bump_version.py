# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Prepare a MAJOR.MINOR.PATCH release from reviewed notes; never tag or push."""
import argparse
import json
from pathlib import Path
import re
from check_release import ROOT, SEMVER


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("version", help="New MAJOR.MINOR.PATCH version")
    parser.add_argument("--notes", required=True, type=Path, help="Completed Markdown release notes")
    parser.add_argument("--stable", action="store_true", help="Do not label the GitHub release as a prerelease")
    args = parser.parse_args()
    if not re.fullmatch(SEMVER, args.version):
        parser.error("Expected MAJOR.MINOR.PATCH with no leading zeros")
    old = (ROOT / "VERSION").read_text().strip()
    if tuple(map(int, args.version.split('.'))) <= tuple(map(int, old.split('.'))):
        parser.error("The new version must be greater than VERSION")
    notes = args.notes.read_text(encoding="utf-8")
    if f"v{args.version}" not in notes or "TODO" in notes:
        parser.error("Notes must mention the new vMAJOR.MINOR.PATCH tag and contain no TODO")
    target = ROOT / "docs" / "releases" / f"{args.version}.md"
    if target.exists():
        parser.error("Versioned release notes already exist")
    metadata = {"version": args.version, "tag": "v" + args.version, "prerelease": not args.stable}
    manifest_path = ROOT / "packaging" / "vcpkg" / "vcpkg.json"
    manifest = json.loads(manifest_path.read_text())
    manifest["version-semver"] = args.version
    (ROOT / "VERSION").write_text(args.version + "\n", encoding="utf-8", newline="\n")
    (ROOT / "release.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8", newline="\n")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    for path in (ROOT / "release.md", target):
        path.write_text(notes.rstrip() + "\n", encoding="utf-8", newline="\n")
    print(f"Prepared {metadata['tag']}. Update CHANGELOG.md and README.md, then run scripts/check_release.py.")


if __name__ == "__main__":
    main()
