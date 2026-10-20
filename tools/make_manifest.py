#!/usr/bin/env python3
"""Writes the manifest.json a panel reads to find out what's available.

The release workflow runs this after a build and attaches the result to the
GitHub release, next to the firmware.bin it describes. A panel fetches only
this file when it checks for updates (a few hundred bytes, rather than the
release API's several KB), and downloads the firmware only if it decides to.

    tools/make_manifest.py \
        --version 1.2.0 --channel stable --tag v1.2.0 \
        --repo ianbakst/thermostat \
        --firmware panel/.pio/build/esp32c6-touch-lcd/firmware.bin \
        --asset panel-firmware.bin \
        --out panel-manifest.json

Each firmware in this repo gets its own manifest and its own asset name, so a
panel can never be offered the mini-split's image or the other way round.
`--asset` is the name the image is uploaded under, which is what the manifest
points at; it needn't match the local file's name.

The panel uses `version` (compared against its own) and `url` (what to
download). The rest is for people: `sha256` and `size` are there to check a
download by hand, and `commit` says exactly what was built.
"""

import argparse
import hashlib
import json
import pathlib
import subprocess
from datetime import datetime, timezone


def git(*args, default=""):
    try:
        return subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL).decode().strip()
    except (OSError, subprocess.CalledProcessError):
        return default


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help='e.g. "1.2.0" (no leading v)')
    parser.add_argument("--channel", required=True, choices=["stable", "beta"])
    parser.add_argument("--tag", required=True, help="the release tag holding the assets")
    parser.add_argument("--repo", required=True, help='"owner/name"')
    parser.add_argument("--firmware", required=True, type=pathlib.Path)
    parser.add_argument(
        "--asset", required=True, help='the name it is uploaded as, e.g. "panel-firmware.bin"'
    )
    parser.add_argument("--out", required=True, type=pathlib.Path)
    args = parser.parse_args()

    image = args.firmware.read_bytes()
    manifest = {
        "version": args.version,
        "channel": args.channel,
        # Absolute, and naming the tag rather than "latest": whichever release
        # a panel found this manifest in, it downloads the firmware that was
        # built alongside it, even if a newer one appears in between.
        "url": f"https://github.com/{args.repo}/releases/download/{args.tag}/{args.asset}",
        "size": len(image),
        "sha256": hashlib.sha256(image).hexdigest(),
        "commit": git("rev-parse", "HEAD", default="unknown"),
        "released": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }

    args.out.write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
