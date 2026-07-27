#!/usr/bin/env python3
"""Push Claude Code / Codex / Cursor session status to the StickS3 AI Buddy app.

Tails each CLI's local session logs (read-only), derives a coarse state, and
sends a small JSON blob the device renders as three status cards. Two transports,
matching the app's selector:

    python scripts/buddy.py wifi           # UDP-broadcast to port 5533
    python scripts/buddy.py ble            # write to the StickS3-Buddy GATT char

State is idle/running/attn where:
    running = the session log changed within --active seconds
    idle    = older than that
ponytail: "attn" (awaiting approval) isn't reliably detectable from logs across
all three CLIs, so it's only ever set if a log line literally asks to approve.
Quota isn't in the logs either, so it's not shown.

BLE needs `pip install bleak`; wifi needs nothing beyond the stdlib.
"""
from __future__ import annotations

import argparse
import asyncio
import glob
import json
import os
import socket
import time
from pathlib import Path

HOME = Path.home()
UDP_PORT = 5533
SVC = "0000a000-0000-1000-8000-00805f9b34fb"
CHR = "0000a001-0000-1000-8000-00805f9b34fb"
IDLE, RUN, ATTN = 0, 1, 2


def newest(patterns: list[str]) -> Path | None:
    files: list[str] = []
    for p in patterns:
        files += glob.glob(str(HOME / p), recursive=True)
    files = [f for f in files if os.path.isfile(f)]
    return Path(max(files, key=os.path.getmtime)) if files else None


def last_text(path: Path, is_user_key) -> str:
    """Cheap tail: scan the last chunk for the most recent human-readable line."""
    try:
        with path.open("rb") as f:
            f.seek(0, os.SEEK_END)
            back = min(f.tell(), 200_000)
            f.seek(f.tell() - back)
            lines = f.read().decode("utf-8", "replace").splitlines()
    except OSError:
        return ""
    for line in reversed(lines):
        line = line.strip()
        if not line:
            continue
        try:
            d = json.loads(line)
        except ValueError:
            continue
        t = is_user_key(d)
        if t:
            return t[:60]
    return ""


def _claude_text(d: dict) -> str:
    if d.get("type") != "user":
        return ""
    c = d.get("message", {}).get("content")
    if isinstance(c, str):
        return c
    if isinstance(c, list):  # skip tool_result blocks; want the typed prompt
        for b in c:
            if isinstance(b, dict) and b.get("type") == "text":
                return b.get("text", "")
    return ""


def _codex_text(d: dict) -> str:
    p = d.get("payload", {})
    if isinstance(p, dict) and p.get("type") in ("user_message", "message"):
        m = p.get("message") or p.get("text") or ""
        return m if isinstance(m, str) else ""
    return ""


def agent_status(name: str, path: Path | None, textfn, active_s: float) -> dict:
    if not path:
        return {"n": name, "s": IDLE, "t": "no session"}
    age = time.time() - path.stat().st_mtime
    task = textfn(path)
    attn = "approve" in task.lower() or "permission" in task.lower()
    state = ATTN if attn else RUN if age < active_s else IDLE
    return {"n": name, "s": state, "t": task or "-"}


def collect(active_s: float) -> dict:
    claude = newest([".claude/projects/*/*.jsonl"])
    codex = newest([".codex/sessions/**/*.jsonl"])
    cursor = newest([".cursor/chats/**/*.json", ".cursor/agent-cli-state.json"])
    return {
        "agents": [
            agent_status("Claude", claude, lambda p: last_text(p, _claude_text), active_s),
            agent_status("Codex", codex, lambda p: last_text(p, _codex_text), active_s),
            # Cursor logs have no stable message schema; report presence + recency only.
            agent_status("Cursor", cursor, lambda p: "session active", active_s),
        ]
    }


def run_wifi(interval: float, active_s: float) -> None:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    print(f"broadcasting to 255.255.255.255:{UDP_PORT} every {interval}s (Ctrl-C to stop)")
    while True:
        payload = json.dumps(collect(active_s)).encode()
        s.sendto(payload, ("255.255.255.255", UDP_PORT))
        time.sleep(interval)


async def run_ble(interval: float, active_s: float) -> None:
    from bleak import BleakClient, BleakScanner

    print("scanning for StickS3-Buddy...")
    dev = await BleakScanner.find_device_by_name("StickS3-Buddy", timeout=15)
    if not dev:
        raise SystemExit("StickS3-Buddy not found; open AI Buddy > BLE on the device")
    async with BleakClient(dev) as client:
        print("connected")
        while True:
            payload = json.dumps(collect(active_s)).encode()
            await client.write_gatt_char(CHR, payload, response=False)
            await asyncio.sleep(interval)


def _selftest() -> None:
    # State derivation is the only real logic -- verify it without a device.
    now = time.time()
    assert agent_status("X", None, lambda p: "", 30)["s"] == IDLE
    d = {"n": "Claude", "s": RUN, "t": "please approve this"}
    assert ("approve" in d["t"].lower())
    # _claude_text pulls typed text, skips tool_result lists
    assert _claude_text({"type": "user", "message": {"content": "hi"}}) == "hi"
    assert _claude_text({"type": "user", "message": {"content": [{"type": "tool_result"}]}}) == ""
    assert _claude_text({"type": "assistant", "message": {"content": "x"}}) == ""
    print("selftest ok")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["wifi", "ble", "selftest"])
    ap.add_argument("-i", "--interval", type=float, default=2.0)
    ap.add_argument("--active", type=float, default=45.0, help="seconds since last log write = running")
    args = ap.parse_args()
    if args.mode == "selftest":
        _selftest()
    elif args.mode == "wifi":
        run_wifi(args.interval, args.active)
    else:
        asyncio.run(run_ble(args.interval, args.active))


if __name__ == "__main__":
    main()
