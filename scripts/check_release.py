# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Validate release metadata, source contents, local documentation links and archives."""
import argparse
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import subprocess
import sys
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SEMVER = r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)"
TOKEN = re.compile(rb"(?:mfa\.[A-Za-z0-9_-]{20,}|[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{5,}\.[A-Za-z0-9_-]{20,}|gh[pousr]_[A-Za-z0-9]{30,})")
FORBIDDEN = {".git", "build", "dist", "out", ".cache", "__pycache__", ".env"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def repository_files():
    result = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=ROOT)
    files = {}
    for name in sorted(set(result.decode().split("\0")) - {""}):
        path = ROOT / name
        require(not path.is_symlink(), f"Source contains a symlink: {name}")
        files[name] = path.read_bytes()
    return files


def archive_files(path):
    entries = {}
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for item in archive.infolist():
                require((item.external_attr >> 16) & 0o170000 != 0o120000, "Archive contains a symlink")
                if not item.is_dir():
                    require(item.filename not in entries, "Duplicate archive entry")
                    entries[item.filename] = archive.read(item)
    else:
        with tarfile.open(path, "r:gz") as archive:
            for item in archive.getmembers():
                require(item.isfile() or item.isdir(), "Archive contains a non-regular entry")
                if item.isfile():
                    require(item.name not in entries, "Duplicate archive entry")
                    entries[item.name] = archive.extractfile(item).read()
    require(entries, "Empty source archive")
    for name in entries:
        parts = PurePosixPath(name).parts
        require(not name.startswith("/") and ".." not in parts and "\\" not in name
                and ":" not in name and len(parts) > 1, "Unsafe source archive path")
    require(len({PurePosixPath(name).parts[0] for name in entries}) == 1, "Expected one source archive root")
    return {"/".join(PurePosixPath(name).parts[1:]): data for name, data in entries.items()}


def validate(files):
    required = {
        "VERSION", "release.json", "release.md", "README.md", "LICENSE", "COPYING", "NOTICE",
        "THIRD_PARTY.md", "CHANGELOG.md", "SECURITY.md", "CONTRIBUTING.md", "CMakeLists.txt",
        "conanfile.py", "docs/API.md", "docs/BUILDING.md", "docs/DISTRIBUTION.md",
        "docs/RELEASING.md", "tests/core.cpp", "test_package/main.cpp",
        "packaging/vcpkg/vcpkg.json", ".github/workflows/ci.yml", ".github/workflows/publish.yml",
        ".github/workflows/release-version.yml", "scripts/create_release_tag.py", "docs/LIVE_BOTS.md",
        "include/yl/discord/network.hpp", "src/gateway.cpp", "src/rest.cpp", "tests/network.cpp", "tests/loopback.py",
        "third_party/licenses/asio.txt", "third_party/licenses/curl.txt", "third_party/licenses/nlohmann-json.txt"
    }
    require(required <= files.keys(), "Missing publication files: " + ", ".join(sorted(required - files.keys())))
    version = files["VERSION"].decode().strip()
    require(re.fullmatch(SEMVER, version), "VERSION must use MAJOR.MINOR.PATCH with no leading zeros")
    metadata = json.loads(files["release.json"])
    require(metadata["version"] == version and metadata["tag"] == "v" + version, "Release version/tag mismatch")
    require(type(metadata["prerelease"]) is bool, "prerelease must be a boolean")
    notes = f"docs/releases/{version}.md"
    require(notes in files and files["release.md"] == files[notes], "release.md must match current versioned notes")
    require(f"v{version}".encode() in files["release.md"], "Release notes have the wrong version")
    require(b"TODO" not in files["release.md"], "Finish release notes before publication")
    require(f"[{version}]".encode() in files["CHANGELOG.md"], "Missing changelog entry")
    require(json.loads(files["packaging/vcpkg/vcpkg.json"])["version-semver"] == version, "vcpkg version mismatch")
    require(b"GNU LESSER GENERAL PUBLIC LICENSE" in files["LICENSE"], "Missing LGPL text")
    require(b"GNU GENERAL PUBLIC LICENSE" in files["COPYING"], "Missing GPL text incorporated by LGPL")
    require(b"LGPL-3.0-or-later" in files["NOTICE"], "Missing license notice")
    for name, data in files.items():
        parts = PurePosixPath(name).parts
        require(not any(part.lower() in FORBIDDEN for part in parts), f"Private/generated path: {name}")
        require(not any("token" in part.lower() or (part.startswith(".env.") and part != ".env.example")
                        for part in parts), f"Credential path: {name}")
        require(not TOKEN.search(data), f"Possible credential in {name}; value withheld")
        if name.endswith((".cpp", ".hpp", ".py", ".ps1")):
            require(b"SPDX-License-Identifier: LGPL-3.0-or-later" in data, f"Missing SPDX notice: {name}")
        if name.endswith(".md"):
            text = data.decode("utf-8")
            prose = re.sub(r"(?ms)^```.*?^```[^\n]*(?:\n|$)", "", text)
            prose = re.sub(r"`[^`\n]*`", "", prose)
            for link in re.findall(r"!?\[[^\]]*\]\(([^\s)]+)(?:\s+[^)]*)?\)", prose):
                if "://" in link or link.startswith(("#", "mailto:")):
                    continue
                target = link.split("#", 1)[0].strip("<>")
                if target:
                    require(posixpath.normpath(posixpath.join(posixpath.dirname(name), target)) in files,
                            f"Broken local documentation link in {name}: {target}")
    return version


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--tag", help="Also require this existing tag to match metadata and HEAD")
    args = parser.parse_args()
    files = archive_files(args.archive) if args.archive else repository_files()
    version = validate(files)
    if args.tag:
        require(args.tag == "v" + version, "Requested tag does not match VERSION")
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).strip()
        tag = subprocess.check_output(["git", "rev-parse", f"refs/tags/{args.tag}^{{commit}}"], cwd=ROOT).strip()
        require(head == tag, "Tag does not point to the checked-out commit")
    print(f"Release {version}: {len(files)} files validated; metadata, links, licenses and credential checks passed")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Release check failed: {error}", file=sys.stderr)
        sys.exit(1)
