#!/usr/bin/env python3
"""Claude Code hook relay for Desk Buddy.

Usage in a hook command:  python3 buddy_hook.py <EventName>
From the status line:     python3 buddy_hook.py StatusLine   (forwards the 5-hour usage for the ring)

Reads the hook's JSON from stdin, forwards a few fields to the daemon over a
local Unix socket, and exits 0. It prints nothing and never blocks, so it
cannot change what Claude Code does. If the daemon is not running, it does
nothing. Uses only the standard library.
"""

import json
import os
import re
import select
import socket
import sys
import time

SOCK_DIR = os.environ.get("DESK_BUDDY_DIR") or os.path.expanduser("~/.desk-buddy")
SOCK_PATH = os.path.join(SOCK_DIR, "buddy.sock")

MAX_STDIN = 32 * 1024 * 1024   # tool output can be large, so read it all up to a cap
READ_DEADLINE = 2.0            # seconds, so a writer that never closes stdin cannot hang us
HEAD_BYTES = 64 * 1024         # session fields sit at the start of the payload
FIELDS = ("session_id", "hook_event_name", "notification_type")


def read_stdin():
    """Return all of stdin as bytes, bounded by a size cap and a deadline."""
    try:
        if sys.stdin is None or sys.stdin.isatty():
            return b""
        fd = sys.stdin.fileno()
    except Exception:
        return b""
    chunks = []
    total = 0
    end = time.monotonic() + READ_DEADLINE
    while total < MAX_STDIN:
        remaining = end - time.monotonic()
        if remaining <= 0:
            break
        ready, _, _ = select.select([fd], [], [], remaining)
        if not ready:
            break
        data = os.read(fd, 65536)
        if not data:
            break
        chunks.append(data)
        total += len(data)
    return b"".join(chunks)


def scan_head(raw):
    """Pull the few fields we need out of the start of a payload without parsing all of it."""
    head = raw[:HEAD_BYTES].decode("utf-8", "replace")
    found = {}
    for key in FIELDS:
        match = re.search(r'"%s"\s*:\s*"([^"\\]*)"' % key, head)
        if match:
            found[key] = match.group(1)
    return found


def parse(raw):
    if not raw.strip():
        return {}
    if len(raw) > HEAD_BYTES:
        # Big payloads are tool output. Skip the full JSON parse, we only need the header fields.
        return scan_head(raw)
    try:
        parsed = json.loads(raw.decode("utf-8", "replace"))
        if isinstance(parsed, dict):
            return parsed
    except ValueError:
        pass
    return scan_head(raw)


def _number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return value


def usage_fields(payload):
    """(used_percentage, resets_at) of the status line's 5-hour window, or None for each part missing."""
    limits = payload.get("rate_limits")
    window = limits.get("five_hour") if isinstance(limits, dict) else None
    if not isinstance(window, dict):
        return None, None
    return _number(window.get("used_percentage")), _number(window.get("resets_at"))


def main():
    event = sys.argv[1] if len(sys.argv) > 1 else ""
    try:
        payload = parse(read_stdin())
    except Exception:
        payload = {}

    msg = {
        "e": event or str(payload.get("hook_event_name") or ""),
        "s": str(payload.get("session_id") or os.environ.get("DESK_BUDDY_SESSION") or "default"),
        "t": str(payload.get("notification_type") or ""),
        "m": str(payload.get("message") or "")[:100],
    }
    if event == "StatusLine":
        msg["u"], msg["r"] = usage_fields(payload)
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        sock.settimeout(0.2)
        sock.sendto(json.dumps(msg).encode("utf-8"), SOCK_PATH)
        sock.close()
    except Exception:
        pass


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
