"""Provide one build-time firmware version for every PlatformIO environment."""

import os
import re
import subprocess

Import("env")

SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


def git_short_sha(project_dir):
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=project_dir,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


release_version = os.environ.get("GASMETER_VERSION", "").strip()
if release_version:
    if not SEMVER.fullmatch(release_version):
        raise ValueError(
            "GASMETER_VERSION must be an exact semantic version such as 3.2.0"
        )
    firmware_version = release_version
else:
    firmware_version = f"dev+{git_short_sha(env.subst('$PROJECT_DIR'))}"

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(firmware_version))])
print(f"Firmware version: {firmware_version}")
