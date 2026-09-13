"""Stamps the build with a version, for the updater to compare against.

PlatformIO runs this before every build (`extra_scripts` in platformio.ini).
It defines FIRMWARE_VERSION, which include/config.h falls back to "dev" for
when this script can't work out anything better.

Where the version comes from, in order:

1.  $FIRMWARE_VERSION, which the release workflow sets from the tag it's
    building (or from the beta version it made up). Nothing here has to know
    about GitHub.
2.  `git describe`, so a local build says which tag it's ahead of and whether
    the tree was dirty (e.g. "1.2.0-3-gb209436-dirty"). That version will
    never match a manifest, which is what we want: a hand-built panel isn't
    told it's out of date, and the panel shows exactly what's on it.
3.  "dev", when there's no git and no environment variable.

A leading "v" is dropped, so the tag v1.2.0 and the version 1.2.0 are the
same thing everywhere else.
"""

import os
import subprocess

Import("env")  # noqa: F821  (PlatformIO injects this)


def from_git():
    try:
        described = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
            stderr=subprocess.DEVNULL,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return described.decode().strip() or None


version = os.environ.get("FIRMWARE_VERSION") or from_git() or "dev"
version = version.removeprefix("v")

print(f"Firmware version: {version}")
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
