#!/usr/bin/env python3
"""Stream a raw application binary to the M2 bootstrap over USB serial."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys
import time

import serial

from firmware_utils import crc16_ccitt, read_firmware


def read_line(port: serial.Serial, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline()
        if line:
            decoded = line.decode("utf-8", errors="replace").strip()
            print(f"< {decoded}")
            return decoded
    raise TimeoutError("device response timed out")


def wait_for(port: serial.Serial, prefix: str, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = read_line(port, max(0.1, deadline - time.monotonic()))
        if line.startswith("ERR"):
            raise RuntimeError(line)
        if line.startswith(prefix):
            return line
    raise TimeoutError(f"did not receive {prefix!r}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="USB CDC port, for example /dev/cu.usbmodem101")
    parser.add_argument(
        "firmware",
        type=pathlib.Path,
        help="raw firmware.bin or PlatformIO firmware.zip",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="commit bootloader settings and reboot after verification",
    )
    args = parser.parse_args()

    image = read_firmware(args.firmware)
    crc = crc16_ccitt(image)
    sha256 = hashlib.sha256(image).hexdigest()
    if len(image) > 397_312:
        raise ValueError(f"image is too large: {len(image)} > 397312")
    if crc == 0:
        raise ValueError("CRC16 is zero; this bootloader treats zero as CRC disabled")

    print(
        f"image={args.firmware} size={len(image)} "
        f"crc16={crc:04x} sha256={sha256}"
    )
    with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=10) as port:
        time.sleep(0.5)
        port.reset_input_buffer()
        header = f"WIOOTA {len(image)} {crc:04x} {sha256}\n".encode()
        print(f"> {header.decode().strip()}")
        port.write(header)
        wait_for(port, "READY", 20)

        sent = 0
        while sent < len(image):
            chunk = image[sent : sent + 512]
            port.write(chunk)
            sent += len(chunk)
        port.flush()
        wait_for(port, "VERIFIED", 60)
        wait_for(port, "SEND APPLY", 5)

        if not args.apply:
            print("> ABORT")
            port.write(b"ABORT\n")
            port.flush()
            wait_for(port, "ABORTED", 5)
            print("verified only; update was not committed (use --apply to activate)")
            return 0

        print("> APPLY")
        port.write(b"APPLY\n")
        port.flush()
        wait_for(port, "ACTIVATED", 20)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, TimeoutError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
