# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Create a tag for this verified commit; the parent workflow also calls publication."""
import json
import os
from pathlib import Path
import subprocess
from check_release import ROOT, require, validate, repository_files


def api(endpoint, payload=None):
    command = ["gh", "api", endpoint]
    if payload is not None:
        command += ["--method", "POST", "--input", "-"]
    result = subprocess.run(command, input=json.dumps(payload) if payload is not None else None,
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError("GitHub tag request failed: " + result.stderr)
    return json.loads(result.stdout)


def main():
    version = validate(repository_files())
    tag = "v" + version
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    repository = os.environ["GH_REPO"]
    existing = subprocess.run(["git", "rev-parse", "--verify", f"refs/tags/{tag}^{{commit}}"],
                              cwd=ROOT, capture_output=True, text=True)
    if existing.returncode == 0:
        require(existing.stdout.strip() == commit, "Existing release tag points to another commit; increment VERSION")
    else:
        obj = api(f"repos/{repository}/git/tags", {
            "tag": tag, "message": f"yl-discord.cpp {tag}", "object": commit, "type": "commit"})
        api(f"repos/{repository}/git/refs", {"ref": "refs/tags/" + tag, "sha": obj["sha"]})
    if output := os.environ.get("GITHUB_OUTPUT"):
        with Path(output).open("a", encoding="utf-8") as stream:
            stream.write(f"tag={tag}\n")
    print(f"Tag {tag} is ready; the publish workflow will build assets and publish the release")


if __name__ == "__main__":
    main()
