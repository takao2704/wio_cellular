#!/usr/bin/env python3
"""Host regressions for Arduino packaging, flags, and DFU export names."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest
import zipfile

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / "tools"))
from firmware_utils import read_firmware
from package_arduino_libraries import LIBRARIES, SKETCH_FILES, package_libraries


class ArduinoSupportTest(unittest.TestCase):
    def test_ed25519_build_matrix(self):
        cases = [
            ([], 0),
            (["NRF52840_XXAA", "ARDUINO=10607"], 1),
            (["NRF52840_XXAA", "ARDUINO=10607", "PLATFORMIO=60118"], 0),
            (["NRF52840_XXAA", "PLATFORMIO=60118", "WIO_OTA_SECURE"], 1),
            (["NRF52840_XXAA", "WIO_OTA_ENABLE_ED25519"], 1),
            (["ARDUINO=10607", "WIO_OTA_SECURE"], 0),
        ]
        for defines, expected in cases:
            with self.subTest(defines=defines):
                source = '#include "WioOtaBuildConfig.h"\n'
                source += f'#if WIO_OTA_HAS_ED25519 != {expected}\n#error wrong configuration\n#endif\n'
                subprocess.run(
                    shlex.split(os.environ.get("CXX", "c++")) +
                    ["-E", "-P", "-x", "c++", "-I", str(PROJECT / "lib/WioOtaAgent/src")] +
                    [f"-D{value}" for value in defines] + ["-"],
                    input=source, text=True, capture_output=True, check=True,
                )

    def test_library_zips_and_shared_sketch(self):
        with tempfile.TemporaryDirectory() as directory:
            for name, path in zip(LIBRARIES, package_libraries(PROJECT, Path(directory))):
                with zipfile.ZipFile(path) as package:
                    names = package.namelist()
                    self.assertIn(f"{name}/library.properties", names)
                    self.assertTrue(all(entry.startswith(f"{name}/") for entry in names))
                    self.assertFalse(any(".pio/" in entry or entry.endswith(".pem") or
                                         entry.endswith("/ota_manifest_public_key.h") for entry in names))
                    properties = package.read(f"{name}/library.properties").decode()
                    version = json.loads((PROJECT / "lib" / name / "library.json").read_text())["version"]
                    self.assertIn(f"version={version}\n", properties)
                    if name == "WioOtaAgent":
                        for filename in SKETCH_FILES:
                            self.assertEqual(package.read(f"{name}/examples/CellularStatusOta/{filename}"),
                                             (PROJECT / "examples/cellular-status-ota/CellularStatusOta" / filename).read_bytes())

    def test_dfu_names_and_rejections(self):
        image = bytes(range(64))
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "application.zip"
            for filename in ("firmware.bin", "CellularStatusOta.ino.bin"):
                with self.subTest(filename=filename):
                    with zipfile.ZipFile(archive, "w") as package:
                        package.writestr(filename, image)
                        package.writestr("manifest.json", json.dumps({"manifest": {
                            "application": {"bin_file": filename}}}))
                    self.assertEqual(read_firmware(archive), image)
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("firmware.bin", image)
            self.assertEqual(read_firmware(archive), image)
            for manifest in (
                {"bootloader": {}, "application": {"bin_file": "firmware.bin"}},
                {"softdevice": {}},
                {"softdevice_bootloader": {}},
                {"application": {"bin_file": "../firmware.bin"}},
                {"application": {"bin_file": "/firmware.bin"}},
                {"application": {"bin_file": "missing.bin"}},
            ):
                with self.subTest(manifest=manifest):
                    with zipfile.ZipFile(archive, "w") as package:
                        package.writestr("firmware.bin", image)
                        package.writestr("manifest.json", json.dumps({"manifest": manifest}))
                    with self.assertRaises(ValueError):
                        read_firmware(archive)
            for document in ([], None, {"manifest": []}):
                with self.subTest(document=document):
                    with zipfile.ZipFile(archive, "w") as package:
                        package.writestr("manifest.json", json.dumps(document))
                    with self.assertRaises(ValueError):
                        read_firmware(archive)
            with self.assertRaises(ValueError):
                read_firmware(Path(directory) / "application.hex")


if __name__ == "__main__":
    unittest.main()
