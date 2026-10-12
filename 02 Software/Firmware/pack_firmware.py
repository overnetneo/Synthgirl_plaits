#!/usr/bin/env python3
"""Packt ein Synthgirl-App-Binary in das SD-Update-Format des Bootloaders.

Entspricht Synthgirl-Firmware-Builder.cpp:
  24 Byte  "RW_SYNTHGIRL_FIRMWARE   "
   2 Byte  Version Major (LE), 2 Byte Version Minor (LE)
   4 Byte  Groesse des Binaries (LE)
   4 Byte  CRC32 (zlib) des Binaries (LE)
   danach  das Binary

Zusaetzlich wird das Binary auf ein Vielfaches von 32 Byte (ein STM32H7-Flashwort)
mit 0xFF aufgefuellt, weil der Bootloader in 32-Byte-Schritten schreibt.

Aufruf: pack_firmware.py <app.bin> <major> <minor> [ausgabeordner]
"""
import struct
import sys
import zlib
from pathlib import Path

HEADER = b"RW_SYNTHGIRL_FIRMWARE   "
MAX_SIZE = 917504  # 896 KB App-Bereich, Grenze aus dem Bootloader


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    app = Path(sys.argv[1]).read_bytes()
    major, minor = int(sys.argv[2]), int(sys.argv[3])
    out_dir = Path(sys.argv[4]) if len(sys.argv) > 4 else Path(".")
    if not (major < 10 and minor < 100):
        sys.exit("Version ausserhalb des vom Bootloader akzeptierten Bereichs (Major<10, Minor<100)")
    if len(app) % 32:
        app += b"\xff" * (32 - len(app) % 32)
    if len(app) >= MAX_SIZE:
        sys.exit(f"Binary zu gross: {len(app)} >= {MAX_SIZE}")
    crc = zlib.crc32(app) & 0xFFFFFFFF
    data = HEADER + struct.pack("<HHII", major, minor, len(app), crc) + app
    name = out_dir / f"Synthgirl_Fw_{major:02d}_{minor:02d}.bin"
    name.write_bytes(data)
    print(f"{name}: {len(app)} Byte, CRC32 0x{crc:08x}")


if __name__ == "__main__":
    main()
