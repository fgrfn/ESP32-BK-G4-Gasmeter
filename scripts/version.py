"""Generate a traceable firmware version for every PlatformIO environment.

Official release builds set GASMETER_VERSION. Local builds derive the nearest
version tag and commit information from Git, never silently using dev+unknown.
"""

import os
import re
import subprocess

Import("env")

SEMVER = re.compile(r"^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)$")


def git(project_dir, *args):
    try:
        return subprocess.check_output(
            ["git", *args], cwd=project_dir, text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def development_version(project_dir):
    sha = git(project_dir, "rev-parse", "--short=7", "HEAD")
    if not sha:
        raise RuntimeError(
            "Cannot determine firmware version: Git is unavailable or the project "
            "has no Git history. Build from a Git checkout or set "
            "GASMETER_VERSION explicitly (e.g. 2.1.5)."
        )

    # Only version tags participate; unrelated tags must not become versions.
    tags = git(project_dir, "tag", "--merged", "HEAD", "--list", "v[0-9]*")
    versions = []
    for tag in (tags or "").splitlines():
        value = tag.removeprefix("v")
        if SEMVER.fullmatch(value):
            versions.append((tuple(map(int, value.split("."))), tag))
    if not versions:
        return f"0.0.0-dev+{sha}"

    _, tag = max(versions)
    base = tag[1:]
    distance = git(project_dir, "rev-list", "--count", f"{tag}..HEAD")
    if distance is None:
        raise RuntimeError(f"Cannot determine commit distance from {tag}")
    if distance == "0":
        return base
    return f"{base}-dev.{distance}+{sha}"


release_version = os.environ.get("GASMETER_VERSION", "").strip()
if release_version:
    if not SEMVER.fullmatch(release_version):
        raise ValueError("GASMETER_VERSION must be an exact semantic version such as 3.2.0")
    firmware_version = release_version
else:
    firmware_version = development_version(env.subst("$PROJECT_DIR"))

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(firmware_version))])
print(f"Firmware version: {firmware_version}")
