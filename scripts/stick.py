#!/usr/bin/env python3
"""Drive the StickS3 and see what it drew, over USB serial.

The firmware runs a tiny serial bridge: it dumps its framebuffer on `shot` and
injects button presses on `key`. So a host (or an agent) can screenshot any
screen and work the UI without a finger on the device.

    python scripts/stick.py shot                 # -> shot.png
    python scripts/stick.py key b b a            # BtnB, BtnB, BtnA
    python scripts/stick.py key home shot        # keys then a screenshot
    python scripts/stick.py logs -s 20           # stream the log for 20s

Keys are a / b / pwr / home -- the two buttons plus a synthetic long-press.
Port autodetects /dev/cu.usbmodem*; override with --port.
"""
from __future__ import annotations

import argparse
import glob
import struct
import sys
import time
import zlib
from pathlib import Path

import serial

W = H = None  # from the SHOT header
KEYS = ("a", "b", "pwr", "home")


def find_port() -> str:
    for p in sorted(glob.glob("/dev/cu.usbmodem*")):
        return p
    sys.exit("no /dev/cu.usbmodem* found; pass --port")


def write_png(path: Path, rgb: bytes, w: int, h: int) -> None:
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 6))
        + chunk(b"IEND", b"")
    )


def rgb565_to_rgb888(buf: bytes, swap: bool) -> bytes:
    out = bytearray(len(buf) // 2 * 3)
    for i in range(0, len(buf), 2):
        lo, hi = buf[i], buf[i + 1]
        v = (hi << 8) | lo if swap else (lo << 8) | hi
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        j = i // 2 * 3
        out[j] = (r << 3) | (r >> 2)
        out[j + 1] = (g << 2) | (g >> 4)
        out[j + 2] = (b << 3) | (b >> 2)
    return bytes(out)


def read_until(s: serial.Serial, marker: bytes, timeout: float) -> bytes:
    end = time.time() + timeout
    buf = bytearray()
    while time.time() < end:
        chunk = s.read(256)
        if chunk:
            buf += chunk
            idx = buf.find(marker)
            if idx >= 0:
                return bytes(buf[idx:])
    raise TimeoutError(f"marker {marker!r} not seen")


def read_exact(s: serial.Serial, n: int, timeout: float) -> bytes:
    end = time.time() + timeout
    buf = bytearray()
    while len(buf) < n and time.time() < end:
        chunk = s.read(n - len(buf))
        if chunk:
            buf += chunk
    if len(buf) != n:
        raise TimeoutError(f"got {len(buf)}/{n} pixel bytes")
    return bytes(buf)


def do_shot(s: serial.Serial, out: Path, swap: bool) -> None:
    s.reset_input_buffer()
    s.write(b"shot\n")
    head = read_until(s, b"<<SHOT ", 5.0)
    # header line: "<<SHOT W H>>\n"; the rest of head after the newline is pixels
    nl = head.find(b">>\n")
    w, h = (int(x) for x in head[len(b"<<SHOT "):nl].split())
    tail = head[nl + 3:]
    need = w * h * 2
    body = tail + read_exact(s, need - len(tail), 5.0) if len(tail) < need else tail[:need]
    write_png(out, rgb565_to_rgb888(body, swap), w, h)
    print(f"wrote {out} ({w}x{h})")


def do_keys(s: serial.Serial, keys: list[str]) -> None:
    for k in keys:
        if k not in KEYS:
            sys.exit(f"bad key {k!r}; use {'/'.join(KEYS)}")
        s.write(f"key {k}\n".encode())
        s.flush()
        time.sleep(0.15)


def do_logs(s: serial.Serial, secs: float) -> None:
    end = time.time() + secs
    while time.time() < end:
        line = s.readline()
        if line:
            sys.stdout.write(line.decode(errors="replace"))
            sys.stdout.flush()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--swap", action="store_true", help="byte-swap RGB565 if colors look wrong")
    ap.add_argument("-o", "--out", default="shot.png")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("shot")
    kp = sub.add_parser("key")
    kp.add_argument("keys", nargs="+")
    lp = sub.add_parser("logs")
    lp.add_argument("-s", "--secs", type=float, default=10)
    args = ap.parse_args()

    port = args.port or find_port()
    with serial.Serial(port, 115200, timeout=0.3) as s:
        time.sleep(0.2)
        if args.cmd == "key":
            do_keys(s, args.keys)
        elif args.cmd == "logs":
            do_logs(s, args.secs)
        elif args.cmd == "shot":
            do_shot(s, Path(args.out), args.swap)


if __name__ == "__main__":
    main()
