#!/usr/bin/env python3
"""Create Arduino IDE library ZIPs without build outputs or local sketch keys."""

from __future__ import annotations

import argparse
from pathlib import Path
import zipfile

PROJECT_ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = ("WioOta", "WioBg770aHttp", "WioOtaAgent")
SKETCH_FILES = ("CellularStatusOta.ino", "ota_sketch_config.h")


def package_libraries(project: Path, output: Path) -> list[Path]:
    output.mkdir(parents=True, exist_ok=True)
    archives = []
    for name in LIBRARIES:
        library = project / "lib" / name
        files = [library / "library.properties"]
        files += sorted(
            path for path in (library / "src").rglob("*")
            if path.is_file() and path.suffix in (".h", ".cpp", ".c", ".S")
        )
        archive = output / f"{name}.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
            for path in files:
                if path.is_symlink():
                    raise ValueError(f"refusing to package symlink: {path}")
                package.write(path, f"{name}/{path.relative_to(library).as_posix()}")
            if name == "WioOtaAgent":
                sketch = project / "examples/cellular-status-ota/CellularStatusOta"
                # Explicit allowlist: never distribute the locally generated key
                # header or compiled/DFU outputs from the sketch directory.
                for filename in SKETCH_FILES:
                    path = sketch / filename
                    if path.is_symlink():
                        raise ValueError(f"refusing to package symlink: {path}")
                    package.write(path, f"{name}/examples/CellularStatusOta/{filename}")
        archives.append(archive)
    return archives


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=PROJECT_ROOT / "dist/arduino")
    args = parser.parse_args()
    for archive in package_libraries(PROJECT_ROOT, args.output):
        print(archive)


if __name__ == "__main__":
    main()
