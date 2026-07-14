#!/usr/bin/env python3
"""Replay and validate captured BK-G4 M-Bus telegrams from a text file."""

from __future__ import annotations

import argparse
import math
from pathlib import Path


def parse_hex_line(line: str) -> bytes:
    cleaned = line.split("#", 1)[0].strip().replace(",", " ")
    if not cleaned:
        return b""
    return bytes(int(token, 16) for token in cleaned.split())


def decode_volume(frame: bytes) -> float:
    if len(frame) < 9 or frame[0] != 0x68 or frame[3] != 0x68 or frame[1] != frame[2]:
        raise ValueError("unsupported or malformed long frame")
    expected = frame[1] + 6
    if len(frame) != expected:
        raise ValueError(f"length mismatch: expected {expected}, got {len(frame)}")
    if frame[-1] != 0x16:
        raise ValueError("invalid stop byte")
    checksum = sum(frame[4:-2]) & 0xFF
    if checksum != frame[-2]:
        raise ValueError(f"checksum mismatch: expected {checksum:02X}, got {frame[-2]:02X}")

    for index in range(4, len(frame) - 6):
        if frame[index : index + 2] != b"\x0c\x13":
            continue
        value = 0
        factor = 1
        for byte in frame[index + 2 : index + 6]:
            low = byte & 0x0F
            high = byte >> 4
            if low > 9 or high > 9:
                raise ValueError("invalid BCD digit")
            value += low * factor
            factor *= 10
            value += high * factor
            factor *= 10
        return value / 1000.0
    raise ValueError("volume record not found")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path, help="text file with one hex telegram per line")
    parser.add_argument("--expect", type=float, help="expected volume for every telegram")
    args = parser.parse_args()

    frames = [parse_hex_line(line) for line in args.capture.read_text(encoding="utf-8").splitlines()]
    frames = [frame for frame in frames if frame]
    if not frames:
        raise SystemExit("no telegrams found")

    for number, frame in enumerate(frames, start=1):
        volume = decode_volume(frame)
        print(f"frame {number}: {volume:.3f} m3")
        if args.expect is not None and not math.isclose(volume, args.expect, abs_tol=0.0005):
            raise SystemExit(f"frame {number}: expected {args.expect:.3f}, got {volume:.3f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
