# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Write SHA256SUMS for all files in a completed release assets directory."""
import argparse
import hashlib
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    paths = sorted(path for path in args.directory.iterdir() if path.is_file() and path.name != "SHA256SUMS")
    if not paths:
        parser.error("No release assets found")
    rows = [f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}" for path in paths]
    (args.directory / "SHA256SUMS").write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    print(f"Checksummed {len(paths)} release assets")


if __name__ == "__main__":
    main()
