#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import sys
import tempfile
import unittest

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "tools"))

from firmware_manifest import canonical_manifest  # noqa: E402


class ManifestSigningTest(unittest.TestCase):
    def test_public_header_preserves_utf8_key_id_bytes(self) -> None:
        private_key = Ed25519PrivateKey.generate()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            public_key = root / "public.pem"
            header = root / "key.h"
            public_key.write_bytes(private_key.public_key().public_bytes(
                serialization.Encoding.PEM,
                serialization.PublicFormat.SubjectPublicKeyInfo,
            ))
            for key_id in ('test-key', 'key-🚀', '鍵-"\\9', 'x' * 31):
                with self.subTest(key_id=key_id):
                    subprocess.run([
                        sys.executable,
                        str(PROJECT_ROOT / "tools/export_manifest_public_key.py"),
                        str(public_key), "--key-id", key_id, "--output", str(header),
                    ], check=True, capture_output=True)
                    expected = ",".join(str(byte) for byte in key_id.encode("utf-8"))
                    source = (
                        '#include "key.h"\n'
                        f'constexpr unsigned char expected[] = {{{expected},0}};\n'
                        'static_assert(sizeof(wio_ota_keys::kManifestKeyId) == sizeof(expected));\n'
                        'constexpr bool matches() {\n'
                        '  for (unsigned i = 0; i < sizeof(expected); ++i)\n'
                        '    if (static_cast<unsigned char>(wio_ota_keys::kManifestKeyId[i]) != expected[i]) return false;\n'
                        '  return true;\n'
                        '}\n'
                        'static_assert(matches(), "UTF-8 key ID changed");\n'
                    )
                    subprocess.run(
                        shlex.split(os.environ.get("CXX", "c++")) +
                        ["-std=c++17", "-x", "c++", "-fsyntax-only", "-I", str(root), "-"],
                        input=source, text=True, capture_output=True, check=True,
                    )

    def manifest(self) -> dict[str, object]:
        return {
            "format": 2,
            "hardware": "wio-bg770a-v1.0",
            "version": 2,
            "url": "http://harvest-files.soracom.io/firmware.bin",
            "size": 129084,
            "crc16": "2f4f",
            "sha256": (
                "25968e40476c60548cdc9ddab54c905c"
                "cf946005d3b286c778b9cd4fff2b2b74"
            ),
            "release_id": "release-2",
            "rollout": 2500,
            "key_id": "test-2026",
        }

    def test_canonical_encoding_matches_device_vector(self) -> None:
        encoded = canonical_manifest(self.manifest())
        self.assertEqual(len(encoded), 152)
        self.assertEqual(
            hashlib.sha256(encoded).hexdigest(),
            "d6411940e2921015e6ce45043bf476202bb6ed363b369a0950e1cffd794b5af1",
        )

    def test_ed25519_signature_detects_manifest_tampering(self) -> None:
        private_key = Ed25519PrivateKey.generate()
        manifest = self.manifest()
        signature = private_key.sign(canonical_manifest(manifest))
        private_key.public_key().verify(signature, canonical_manifest(manifest))

        manifest["rollout"] = 10000
        with self.assertRaises(InvalidSignature):
            private_key.public_key().verify(
                signature, canonical_manifest(manifest)
            )

    def test_cli_creates_verifiable_manifest_and_public_header(self) -> None:
        private_key = Ed25519PrivateKey.generate()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            firmware = root / "firmware.bin"
            signing_key = root / "signing.pem"
            manifest_path = root / "manifest.json"
            public_header = root / "manifest_public_key.h"
            firmware.write_bytes(bytes(range(64)))
            signing_key.write_bytes(
                private_key.private_bytes(
                    encoding=serialization.Encoding.PEM,
                    format=serialization.PrivateFormat.PKCS8,
                    encryption_algorithm=serialization.NoEncryption(),
                )
            )
            subprocess.run(
                [
                    sys.executable,
                    str(PROJECT_ROOT / "tools" / "firmware_manifest.py"),
                    str(firmware),
                    "--version",
                    "5",
                    "--url",
                    "http://harvest-files.soracom.io/v5/firmware.bin",
                    "--signing-key",
                    str(signing_key),
                    "--key-id",
                    "test-key",
                    "--release-id",
                    "release-5",
                    "--rollout",
                    "2500",
                    "--output",
                    str(manifest_path),
                ],
                check=True,
            )
            subprocess.run(
                [
                    sys.executable,
                    str(
                        PROJECT_ROOT
                        / "tools"
                        / "export_manifest_public_key.py"
                    ),
                    str(signing_key),
                    "--key-id",
                    "test-key",
                    "--output",
                    str(public_header),
                ],
                check=True,
            )
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            private_key.public_key().verify(
                bytes.fromhex(str(manifest["signature"])),
                canonical_manifest(manifest),
            )
            self.assertEqual(manifest["format"], 2)
            self.assertEqual(manifest["rollout"], 2500)
            self.assertIn(
                "kManifestPublicKey[32]",
                public_header.read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
