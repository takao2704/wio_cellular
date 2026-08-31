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
from unittest import mock

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT / "tools"))

from firmware_manifest import (  # noqa: E402
    _write_outputs_atomically,
    canonical_manifest,
    validate_firmware_url,
)


class ManifestSigningTest(unittest.TestCase):
    def run_manifest_tool(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(PROJECT_ROOT / "tools" / "firmware_manifest.py"),
                *arguments,
            ],
            text=True,
            capture_output=True,
        )

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
            firmware_output = root / "published-firmware.bin"
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
                    "--firmware-output",
                    str(firmware_output),
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
            self.assertEqual(firmware_output.read_bytes(), firmware.read_bytes())
            self.assertIn(
                "kManifestPublicKey[32]",
                public_header.read_text(encoding="utf-8"),
            )

    def test_url_validation_matches_device_buffer_limits(self) -> None:
        validate_firmware_url("http://" + "h" * 127 + "/" + "p" * 254)
        for invalid_url in (
            "https://harvest-files.soracom.io/firmware.bin",
            "http://harvest-files.soracom.io",
            "http://" + "h" * 128 + "/firmware.bin",
            "http://host/" + "p" * 255,
            "http://host:0/firmware.bin",
            "http://host:65536/firmware.bin",
            "http://host:not-a-port/firmware.bin",
        ):
            with self.subTest(url=invalid_url):
                with self.assertRaises(ValueError):
                    validate_firmware_url(invalid_url)

    def test_unsigned_cli_writes_format_one_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            firmware = root / "firmware.bin"
            output = root / "manifest.json"
            firmware.write_bytes(bytes(range(64)))
            completed = self.run_manifest_tool(
                str(firmware),
                "--version", "1",
                "--url", "http://harvest-files.soracom.io/v1/firmware.bin",
                "--output", str(output),
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            manifest = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(manifest["format"], 1)
            self.assertNotIn("signature", manifest)

    def test_invalid_signing_key_leaves_no_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            firmware = root / "input.bin"
            signing_key = root / "invalid.pem"
            manifest_output = root / "manifest.json"
            firmware_output = root / "firmware.bin"
            firmware.write_bytes(bytes(range(64)))
            signing_key.write_text("not a private key", encoding="utf-8")
            completed = self.run_manifest_tool(
                str(firmware),
                "--version", "2",
                "--url", "http://harvest-files.soracom.io/v2/firmware.bin",
                "--signing-key", str(signing_key),
                "--key-id", "test-key",
                "--release-id", "release-2",
                "--output", str(manifest_output),
                "--firmware-output", str(firmware_output),
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertFalse(manifest_output.exists())
            self.assertFalse(firmware_output.exists())

    def test_output_transaction_restores_both_files_on_replace_failure(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            firmware_output = root / "firmware.bin"
            manifest_output = root / "manifest.json"
            firmware_output.write_bytes(b"old firmware")
            manifest_output.write_bytes(b"old manifest")
            real_replace = os.replace
            replace_calls = 0

            def fail_second_install(source: object, destination: object) -> None:
                nonlocal replace_calls
                replace_calls += 1
                if replace_calls == 4:
                    raise OSError("simulated manifest replace failure")
                real_replace(source, destination)

            with mock.patch("firmware_manifest.os.replace", fail_second_install):
                with self.assertRaises(OSError):
                    _write_outputs_atomically(
                        [
                            (firmware_output, b"new firmware"),
                            (manifest_output, b"new manifest"),
                        ]
                    )
            self.assertEqual(firmware_output.read_bytes(), b"old firmware")
            self.assertEqual(manifest_output.read_bytes(), b"old manifest")

    def test_output_transaction_preserves_backup_when_restore_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            firmware_output = root / "firmware.bin"
            manifest_output = root / "manifest.json"
            firmware_output.write_bytes(b"old firmware")
            manifest_output.write_bytes(b"old manifest")
            real_replace = os.replace
            replace_calls = 0

            def fail_install_and_first_restore(
                source: object, destination: object
            ) -> None:
                nonlocal replace_calls
                replace_calls += 1
                if replace_calls in (4, 5):
                    raise OSError("simulated replacement failure")
                real_replace(source, destination)

            with mock.patch(
                "firmware_manifest.os.replace", fail_install_and_first_restore
            ):
                with self.assertRaisesRegex(RuntimeError, "preserved backups"):
                    _write_outputs_atomically(
                        [
                            (firmware_output, b"new firmware"),
                            (manifest_output, b"new manifest"),
                        ]
                    )
            preserved = list(root.glob(".firmware.bin.backup.*"))
            self.assertEqual(len(preserved), 1)
            self.assertEqual(preserved[0].read_bytes(), b"old firmware")
            self.assertEqual(manifest_output.read_bytes(), b"old manifest")


if __name__ == "__main__":
    unittest.main()
