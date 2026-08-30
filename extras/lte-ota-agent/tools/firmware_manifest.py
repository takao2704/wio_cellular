#!/usr/bin/env python3
"""Create the transport-independent metadata needed by the OTA writer."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import struct
import sys

from firmware_utils import crc16_ccitt, read_firmware

MAXIMUM_IMAGE_SIZE = 397_312


def _field(value: str) -> bytes:
    encoded = value.encode("utf-8")
    if not encoded or len(encoded) > 0xFFFF:
        raise ValueError("signed manifest text fields must be 1..65535 bytes")
    if any(byte < 0x20 or byte == 0x7F for byte in encoded):
        raise ValueError("signed manifest text fields must not contain controls")
    return struct.pack(">H", len(encoded)) + encoded


def canonical_manifest(manifest: dict[str, object]) -> bytes:
    """Encode the fields covered by the format-2 Ed25519 signature."""
    sha256 = bytes.fromhex(str(manifest["sha256"]))
    if len(sha256) != 32:
        raise ValueError("sha256 must contain 32 bytes")
    return b"".join(
        (
            b"WIO-OTA-MANIFEST-V2",
            struct.pack(">I", int(manifest["format"])),
            _field(str(manifest["hardware"])),
            struct.pack(">I", int(manifest["version"])),
            _field(str(manifest["url"])),
            struct.pack(">I", int(manifest["size"])),
            struct.pack(">H", int(str(manifest["crc16"]), 16)),
            sha256,
            _field(str(manifest["release_id"])),
            struct.pack(">H", int(manifest["rollout"])),
            _field(str(manifest["key_id"])),
        )
    )


def _validate_text(name: str, value: str, maximum_bytes: int) -> None:
    encoded = value.encode("utf-8")
    if not encoded or len(encoded) > maximum_bytes:
        raise ValueError(f"{name} must be 1..{maximum_bytes} UTF-8 bytes")
    if any(byte < 0x20 or byte == 0x7F for byte in encoded):
        raise ValueError(f"{name} must not contain control characters")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=pathlib.Path)
    parser.add_argument("--version", required=True, type=int)
    parser.add_argument("--url", required=True)
    parser.add_argument("--hardware", default="wio-bg770a-v1.0")
    parser.add_argument(
        "--signing-key",
        type=pathlib.Path,
        help="Ed25519 private key in PEM format; never commit this file",
    )
    parser.add_argument("--key-id")
    parser.add_argument("--release-id")
    parser.add_argument(
        "--rollout",
        type=int,
        default=10000,
        help="eligible share in basis points (0..10000)",
    )
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

    if args.version < 0 or args.version > 0xFFFFFFFF:
        parser.error("--version must be in the range 0..4294967295")
    if args.rollout < 0 or args.rollout > 10000:
        parser.error("--rollout must be in the range 0..10000")
    signing = args.signing_key is not None
    if signing and (not args.key_id or not args.release_id):
        parser.error("--signing-key requires --key-id and --release-id")
    if not signing and (args.key_id or args.release_id or args.rollout != 10000):
        parser.error(
            "--key-id, --release-id, and --rollout require --signing-key"
        )

    image = read_firmware(args.firmware)
    if len(image) < 8 or len(image) > MAXIMUM_IMAGE_SIZE:
        parser.error(
            f"firmware size must be 8..{MAXIMUM_IMAGE_SIZE} bytes"
        )
    try:
        _validate_text("hardware", args.hardware, 63)
        _validate_text("url", args.url, 511)
        if signing:
            _validate_text("key-id", args.key_id, 31)
            _validate_text("release-id", args.release_id, 63)
    except ValueError as error:
        parser.error(str(error))
    if args.firmware_output is not None:
        args.firmware_output.parent.mkdir(parents=True, exist_ok=True)
        args.firmware_output.write_bytes(image)
    crc16 = crc16_ccitt(image)
    if crc16 == 0:
        parser.error("firmware CRC16 is zero and cannot be represented")
    manifest = {
        "format": 2 if signing else 1,
        "hardware": args.hardware,
        "version": args.version,
        "url": args.url,
        "size": len(image),
        "crc16": f"{crc16:04x}",
        "sha256": hashlib.sha256(image).hexdigest(),
    }
    if signing:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric.ed25519 import (
            Ed25519PrivateKey,
        )

        private_key = serialization.load_pem_private_key(
            args.signing_key.read_bytes(), password=None
        )
        if not isinstance(private_key, Ed25519PrivateKey):
            parser.error("--signing-key must contain an Ed25519 private key")
        manifest.update(
            {
                "release_id": args.release_id,
                "rollout": args.rollout,
                "key_id": args.key_id,
            }
        )
        manifest["signature"] = private_key.sign(
            canonical_manifest(manifest)
        ).hex()
    encoded = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    if args.output is None:
        sys.stdout.write(encoded)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
