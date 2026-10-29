# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Exercise publication failure paths without real credentials or remote writes."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from check_release import ROOT, archive_files, repository_files, validate
import publish_release
import create_release_tag


class PublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = repository_files()

    def test_current_metadata_and_links(self):
        self.assertEqual(validate(self.files), self.files["VERSION"].decode().strip())

    def test_mismatched_tag_and_vcpkg_are_rejected(self):
        for name, data in (
            ("release.json", json.dumps({"version": "0.1.0", "tag": "v0.1", "prerelease": True}).encode()),
            ("packaging/vcpkg/vcpkg.json", b'{"version-semver":"9.9.9"}'),
        ):
            with self.subTest(name=name), self.assertRaises(ValueError):
                validate({**self.files, name: data})

    def test_private_paths_and_fake_credentials_are_rejected(self):
        for name, data in (
            (".env", b"private"),
            ("build/output.txt", b"generated"),
            ("test_" + "token", b"private"),
            ("examples/leak.cpp", ("ghp_" + "a" * 36).encode()),
        ):
            with self.subTest(name=name), self.assertRaises(ValueError):
                validate({**self.files, name: data})

    def test_broken_links_and_unfinished_notes_are_rejected(self):
        with self.assertRaises(ValueError):
            validate({**self.files, "README.md": b"[missing](docs/does-not-exist.md)"})
        version = self.files["VERSION"].decode().strip()
        notes = f"# v{version}\nTODO\n".encode()
        with self.assertRaises(ValueError):
            validate({**self.files, "release.md": notes, f"docs/releases/{version}.md": notes})

    def test_archive_paths_and_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "source.zip"
            for bad in ("../outside", "root/../outside", "/absolute", "root/C:/bad", "root\\..\\outside"):
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr(bad, "unsafe")
                with self.subTest(path=bad), self.assertRaises(ValueError):
                    archive_files(path)
            with zipfile.ZipFile(path, "w") as archive:
                for name, data in self.files.items():
                    archive.writestr("source/" + name, data)
            self.assertEqual(archive_files(path), self.files)
            validate(archive_files(path))

    def test_version_preparation_and_downgrade_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            (root / "packaging/vcpkg").mkdir(parents=True)
            (root / "docs/releases").mkdir(parents=True)
            for name in ("bump_version.py", "check_release.py"):
                shutil.copyfile(ROOT / "scripts" / name, root / "scripts" / name)
            (root / "VERSION").write_text("0.1.0\n")
            (root / "packaging/vcpkg/vcpkg.json").write_text('{"version-semver":"0.1.0"}')
            notes = root / "notes.md"
            notes.write_text("# v0.2.0\nA completed release.\n")
            command = [sys.executable, str(root / "scripts/bump_version.py"), "0.2.0", "--notes", str(notes)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((root / "VERSION").read_text().strip(), "0.2.0")
            self.assertEqual(json.loads((root / "release.json").read_text())["tag"], "v0.2.0")
            self.assertEqual((root / "release.md").read_bytes(), (root / "docs/releases/0.2.0.md").read_bytes())
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)

    def publisher_fixture(self, root):
        (root / "build/dist").mkdir(parents=True)
        metadata = {"tag": "v0.1.1", "version": "0.1.1", "prerelease": False}
        (root / "release.json").write_text(json.dumps(metadata))
        (root / "release.md").write_text("# v0.1.1\nCompleted release.\n")
        names = ["yl-discord.cpp-v0.1.1.zip", "yl-discord.cpp-v0.1.1.tar.gz",
                 "yl-discord.cpp-v0.1.1-vcpkg-overlay.zip", "yl-discord.cpp-v0.1.1-msys2-recipe.zip",
                 "mingw-w64-x86_64-yl-discord-cpp-0.1.1-1-any.pkg.tar.zst",
                 "mingw-w64-ucrt-x86_64-yl-discord-cpp-0.1.1-1-any.pkg.tar.zst", "release.md", "SHA256SUMS"]
        for name in names:
            (root / "build/dist" / name).write_bytes(b"fixture")
        return names

    def test_publisher_reaches_public_release_after_upload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.publisher_fixture(root)
            result = subprocess.CompletedProcess([], 1, "", "release not found")
            with patch.object(publish_release, "ROOT", root), patch.dict("os.environ", {"RELEASE_TAG": "v0.1.1"}), \
                    patch.object(publish_release.subprocess, "run", return_value=result) as runner:
                publish_release.main()
                commands = [call.args[0] for call in runner.call_args_list]
                self.assertEqual([command[2] for command in commands], ["view", "create", "upload", "edit"])
                self.assertIn("--draft=false", commands[-1])
                self.assertIn("--prerelease=false", commands[-1])
                self.assertIn("--latest=true", commands[-1])

    def test_publisher_retry_keeps_complete_release_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = self.publisher_fixture(root)
            release = {"isDraft": False, "isPrerelease": False, "body": (root / "release.md").read_text(),
                       "assets": [{"name": name, "size": 1} for name in names]}
            result = subprocess.CompletedProcess([], 0, json.dumps(release), "")
            with patch.object(publish_release, "ROOT", root), patch.dict("os.environ", {"RELEASE_TAG": "v0.1.1"}), \
                    patch.object(publish_release.subprocess, "run", return_value=result) as runner:
                publish_release.main()
                self.assertEqual(runner.call_count, 1)

    def tag_fixture(self, root, existing_commit=None):
        head = "a" * 40
        existing = subprocess.CompletedProcess([], 1 if existing_commit is None else 0, existing_commit or "", "")
        return head, existing, root / "outputs.txt"

    def test_version_workflow_creates_annotated_tag_and_emits_publication_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            head, existing, output = self.tag_fixture(root)
            with patch.object(create_release_tag, "repository_files", return_value={}), \
                    patch.object(create_release_tag, "validate", return_value="0.2.0"), \
                    patch.object(create_release_tag.subprocess, "check_output", return_value=head + "\n"), \
                    patch.object(create_release_tag.subprocess, "run", return_value=existing), \
                    patch.object(create_release_tag, "api", return_value={"sha": "b" * 40}) as api, \
                    patch.dict("os.environ", {"GH_REPO": "fixture/repo", "GITHUB_OUTPUT": str(output)}):
                create_release_tag.main()
                self.assertEqual(api.call_count, 2)
                self.assertEqual(api.call_args_list[0].args[1], {
                    "tag": "v0.2.0", "message": "yl-discord.cpp v0.2.0", "object": head, "type": "commit"})
                self.assertEqual(api.call_args_list[1].args[1], {"ref": "refs/tags/v0.2.0", "sha": "b" * 40})
                self.assertEqual(output.read_text(), "tag=v0.2.0\n")

    def test_matching_existing_tag_is_reused_without_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            head, existing, output = self.tag_fixture(root, "a" * 40)
            with patch.object(create_release_tag, "repository_files", return_value={}), \
                    patch.object(create_release_tag, "validate", return_value="0.2.0"), \
                    patch.object(create_release_tag.subprocess, "check_output", return_value=head), \
                    patch.object(create_release_tag.subprocess, "run", return_value=existing), \
                    patch.object(create_release_tag, "api") as api, \
                    patch.dict("os.environ", {"GH_REPO": "fixture/repo", "GITHUB_OUTPUT": str(output)}):
                create_release_tag.main()
                api.assert_not_called()
                self.assertEqual(output.read_text(), "tag=v0.2.0\n")

    def test_existing_tag_on_another_commit_is_never_moved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            head, existing, output = self.tag_fixture(root, "c" * 40)
            with patch.object(create_release_tag, "repository_files", return_value={}), \
                    patch.object(create_release_tag, "validate", return_value="0.2.0"), \
                    patch.object(create_release_tag.subprocess, "check_output", return_value=head), \
                    patch.object(create_release_tag.subprocess, "run", return_value=existing), \
                    patch.object(create_release_tag, "api") as api, \
                    patch.dict("os.environ", {"GH_REPO": "fixture/repo", "GITHUB_OUTPUT": str(output)}):
                with self.assertRaisesRegex(ValueError, "another commit"):
                    create_release_tag.main()
                api.assert_not_called()
                self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
