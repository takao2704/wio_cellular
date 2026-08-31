#!/usr/bin/env python3
"""Regressions for the public example, maintainer tests and documentation."""
from __future__ import annotations

import configparser
import json
from pathlib import Path
import re
import unittest
from urllib.parse import unquote

PROJECT = Path(__file__).resolve().parents[2]


def config(path: str) -> configparser.ConfigParser:
    result = configparser.ConfigParser(interpolation=None)
    result.read(PROJECT / path)
    return result


class ProjectLayoutTest(unittest.TestCase):
    def test_example_and_hardware_environments_are_separate(self):
        example = config("examples/CellularStatusOta/platformio.ini")
        hardware = config("tests/hardware/platformio.ini")
        self.assertEqual({s for s in example if s.startswith("env:")},
                         {"env:initial", "env:update"})
        self.assertEqual({s for s in hardware if s.startswith("env:")},
                         {"env:before_activation", "env:after_activation"})
        self.assertNotIn("HALT", (PROJECT / "examples/CellularStatusOta/platformio.ini").read_text())
        self.assertIn("-DWIO_OTA_SECURE", example["env"]["build_flags"])
        self.assertIn("WioOtaAgent=symlink://../..", example["env"]["lib_deps"])
        for field in ("platform", "platform_packages", "board", "framework", "lib_deps"):
            self.assertEqual(example["env"][field], hardware["env"][field])
        for environment, flag in (
            ("before_activation", "WIO_OTA_TEST_HALT_BEFORE_ACTIVATE"),
            ("after_activation", "WIO_OTA_TEST_HALT_AFTER_ACTIVATE"),
        ):
            self.assertIn(flag, hardware[f"env:{environment}"]["build_flags"])
        sketch = PROJECT / "examples/CellularStatusOta/CellularStatusOta.ino"
        self.assertNotIn("HALT", sketch.read_text())

    def test_single_library_dependencies(self):
        manifest = json.loads((PROJECT / "library.json").read_text())
        self.assertEqual(manifest["name"], "WioOtaAgent")
        self.assertEqual(manifest["dependencies"], {
            "seeedjp/WioCellular": "0.3.15", "bblanchon/ArduinoJson": "7.0.4"})
        self.assertFalse(list((PROJECT / "lib").rglob("library.*")))
        self.assertTrue((PROJECT / manifest["build"]["extraScript"]).is_file())

    def test_initial_diagnostics_are_not_distributed(self):
        for filename in (
            "apps/blinky_v2/main.cpp", "apps/bootstrap/main.cpp",
            "apps/lte_bootstrap/main.cpp", "include/ota_config.h",
            "include/ota_config.local.h.example", "platformio.ini",
            "tools/send_firmware.py", "docs/development-plan.md",
        ):
            self.assertFalse((PROJECT / filename).exists(), filename)
        runner = (PROJECT / "tools/run_native_tests.sh").read_text()
        self.assertNotIn("lte_bootstrap", runner)
        self.assertIn("ARDUINOJSON_INCLUDE_DIR", runner)

    def test_relative_document_links_resolve(self):
        documents = [PROJECT / "README.md", PROJECT / "tests/README.md"]
        documents += list((PROJECT / "docs").rglob("*.md"))
        for document in documents:
            for target in re.findall(r"\[[^\]]+\]\(([^)\s]+)\)", document.read_text()):
                if "://" in target or target.startswith("#"):
                    continue
                path = unquote(target.split("#", 1)[0])
                self.assertTrue((document.parent / path).exists(), f"{document}: {target}")

    def test_user_guides_do_not_require_internal_test_names(self):
        documents = [PROJECT / "README.md"] + list((PROJECT / "docs").glob("*.md"))
        for document in documents:
            text = document.read_text()
            self.assertNotRegex(text, r"\bM[0-6]\b|ota_v[0-9]|lte_target_v[0-9]")
        guide = (PROJECT / "README.md").read_text()
        self.assertIn("metadata.soracom.io", guide)
        self.assertNotIn("100.127.", guide)


if __name__ == "__main__":
    unittest.main()
