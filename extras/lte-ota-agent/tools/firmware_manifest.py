#!/usr/bin/env python3
"""Create the transport-independent metadata needed by the OTA writer."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys

from firmware_utils import crc16_ccitt, read_firmware


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=pathlib.Path)
    parser.add_argument("--version", required=True, type=int)
    parser.add_argument("--url", required=True)
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        help="write JSON to this file instead of stdout",
    )
    parser.add_argument(
        "--firmware-output",
        type=pathlib.Path,
        help="also write the extracted raw firmware image to this path",
    )
    args = parser.parse_args()

    image = read_firmware(args.firmware)
    if args.firmware_output is not None:
        args.firmware_output.parent.mkdir(parents=True, exist_ok=True)
        args.firmware_output.write_bytes(image)
    manifest = {
        "format": 1,
        "hardware": "wio-bg770a-v1.0",
        "version": args.version,
        "url": args.url,
        "size": len(image),
        "crc16": f"{crc16_ccitt(image):04x}",
        "sha256": hashlib.sha256(image).hexdigest(),
    }
    encoded = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    if args.output is None:
        sys.stdout.write(encoded)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
