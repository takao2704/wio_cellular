from __future__ import annotations

import pathlib
import zipfile


def read_firmware(path: pathlib.Path) -> bytes:
    if path.suffix.lower() != ".zip":
        return path.read_bytes()
    with zipfile.ZipFile(path) as package:
        try:
            return package.read("firmware.bin")
        except KeyError as error:
            raise ValueError(f"{path} does not contain firmware.bin") from error


def crc16_ccitt(data: bytes, crc: int = 0xFFFF) -> int:
    for value in data:
        crc = ((crc >> 8) & 0xFF) | ((crc << 8) & 0xFFFF)
        crc ^= value
        crc ^= (crc & 0xFF) >> 4
        crc ^= (crc << 12) & 0xFFFF
        crc ^= ((crc & 0xFF) << 5) & 0xFFFF
    return crc & 0xFFFF
