#!/usr/bin/env python3
"""Desk Buddy daemon.

Owns the USB serial port to the board, receives Claude Code hook events from
buddy_hook.py over a local Unix socket, works out one face state across all
running Claude Code sessions, and sends that state name to the board.
Also forwards the 5-hour usage for the board's ring (from the status line relay)
and treats a poke on the board's touch panel as activity, so it wakes the face.

Face states: idle, happy, thinking, sleepy, alert, error.
Needs pyserial (see requirements.txt) unless you run with --no-serial.
"""

import argparse
import fcntl
import glob
import json
import math
import os
import select
import signal
import socket
import sys
import time

SOCK_DIR = os.environ.get("DESK_BUDDY_DIR") or os.path.expanduser("~/.desk-buddy")
SOCK_PATH = os.path.join(SOCK_DIR, "buddy.sock")
LOCK_PATH = os.path.join(SOCK_DIR, "daemon.lock")
SOCK_PATH_LIMIT = 100  # macOS allows 104 bytes for a Unix socket path, Linux 108

PORT_GLOBS = [
    "/dev/cu.usbserial*",
    "/dev/cu.wchusbserial*",
    "/dev/cu.SLAB_USBtoUART*",
    "/dev/cu.usbmodem*",
    "/dev/ttyUSB*",
    "/dev/ttyACM*",
]

IGNORED_NOTIFICATIONS = ("idle_prompt", "auth_success")
BOOT_BANNER = "Desk Buddy"  # the firmware prints this line at every boot


class Aggregator(object):
    """Turns hook events from many sessions into one face state.

    Priority, highest first: alert, error, thinking, happy, sleepy, idle.

    A session counts as working from the prompt until Stop. Tool runs that
    are still open (PreToolUse without a matching PostToolUse) get the longer
    tool_timeout, so a five minute build does not drop the face to idle.
    """

    def __init__(self, work_timeout=180.0, tool_timeout=600.0, wait_timeout=3600.0,
                 happy_hold=4.0, error_hold=8.0, sleep_after=600.0, clock=time.monotonic):
        self.work_timeout = work_timeout
        self.tool_timeout = tool_timeout
        self.wait_timeout = wait_timeout
        self.happy_hold = happy_hold
        self.error_hold = error_hold
        self.sleep_after = sleep_after
        self.clock = clock
        self.sessions = {}  # session id -> {"state": str, "ts": float, "pending": int}
        self.happy_until = 0.0
        self.error_until = 0.0
        self.last_activity = clock()

    @staticmethod
    def classify_notification(ntype, msg):
        """Return 'waiting' if the notification needs the user, else None."""
        ntype = (ntype or "").lower()
        if ntype in IGNORED_NOTIFICATIONS:
            return None
        if ntype:
            return "waiting"
        if "waiting for your input" in (msg or "").lower():
            return None
        return "waiting"

    def _pending(self, sid):
        current = self.sessions.get(sid)
        return current["pending"] if current else 0

    def _set(self, sid, state, now, pending):
        self.sessions[sid] = {"state": state, "ts": now, "pending": pending}

    def _any(self, states):
        for session in self.sessions.values():
            if session["state"] in states:
                return True
        return False

    def handle(self, event, sid, ntype="", msg=""):
        sid = sid or "default"
        if event == "Notification" and self.classify_notification(ntype, msg) is None:
            return
        now = self.clock()
        self.last_activity = now
        if event == "SessionEnd":
            self.sessions.pop(sid, None)
        elif event == "SessionStart":
            self._set(sid, "idle", now, 0)
        elif event == "UserPromptSubmit":
            self._set(sid, "working", now, 0)
        elif event == "PreToolUse":
            self._set(sid, "working", now, self._pending(sid) + 1)
        elif event in ("PostToolUse", "PostToolUseFailure"):
            self._set(sid, "working", now, max(0, self._pending(sid) - 1))
        elif event == "Notification":
            self._set(sid, "waiting", now, self._pending(sid))
        elif event == "Stop":
            self._set(sid, "idle", now, 0)
            if not self._any(("working", "waiting")):
                self.happy_until = now + self.happy_hold
        elif event == "StopFailure":
            self._set(sid, "idle", now, 0)
            self.error_until = now + self.error_hold
            self.happy_until = 0.0

    def poke(self):
        """Someone touched the board: count it as activity, so a sleepy face wakes and stays awake."""
        self.last_activity = self.clock()

    def tick(self):
        now = self.clock()
        for sid in list(self.sessions):
            session = self.sessions[sid]
            age = now - session["ts"]
            if session["state"] == "working":
                limit = self.tool_timeout if session["pending"] > 0 else self.work_timeout
                if age > limit:
                    session["state"] = "idle"
                    session["pending"] = 0
            elif session["state"] == "waiting":
                if age > self.wait_timeout:
                    session["state"] = "idle"
                    session["pending"] = 0
            elif age > 6 * 3600:
                del self.sessions[sid]

    def face(self):
        now = self.clock()
        if self._any(("waiting",)):
            return "alert"
        if now < self.error_until:
            return "error"
        if self._any(("working",)):
            return "thinking"
        if now < self.happy_until:
            return "happy"
        if now - self.last_activity > self.sleep_after:
            return "sleepy"
        return "idle"


class UsageTracker(object):
    """Latest 5-hour usage from the status line, for the board's ring.

    The number covers the whole account, so the most recent report from any
    session wins. None until a valid report arrives (the ring stays hidden).
    """

    def __init__(self, clock=time.time):
        self.clock = clock  # wall clock: resets_at is in epoch seconds
        self.pct = None
        self.resets_at = None

    @staticmethod
    def _number(value):
        if isinstance(value, bool):
            return None
        try:
            number = float(value)
        except (TypeError, ValueError):
            return None
        return number if math.isfinite(number) else None

    def report(self, pct, resets_at):
        number = self._number(pct)
        if number is None:
            return
        self.pct = int(round(min(100.0, max(0.0, number))))
        self.resets_at = self._number(resets_at)

    def value(self):
        if self.pct is None:
            return None
        if self.resets_at is not None and self.clock() >= self.resets_at:
            return 0  # the window rolled over; Claude Code drops it until the next response
        return self.pct


class LineReader(object):
    """Splits what the board prints into lines, whatever size the serial reads come in."""

    MAX_LINE = 200

    def __init__(self):
        self.buf = b""
        self.skipping = False  # inside a line that grew too long

    def feed(self, data):
        self.buf += data
        lines = []
        while b"\n" in self.buf:
            raw, self.buf = self.buf.split(b"\n", 1)
            if self.skipping:
                self.skipping = False
                continue
            if len(raw) > self.MAX_LINE:
                continue
            text = raw.decode("utf-8", "replace").strip()
            if text:
                lines.append(text)
        if len(self.buf) > self.MAX_LINE:
            self.buf = b""
            self.skipping = True
        return lines


class PrintBoard(object):
    """Stand-in for --no-serial: prints each state instead of sending it."""

    ser = True

    def __init__(self, log):
        self.log = log

    def connect(self):
        return True

    def take_fresh(self):
        return False

    def take_reboot(self):
        return False

    def hold(self, seconds):
        pass

    def ready(self):
        return True

    def send(self, text):
        self.log("would send: %s" % text)
        return True

    def drain(self):
        return []

    def close(self):
        pass


class Board(object):
    def __init__(self, port, baud, boot_delay, log):
        self.port = port
        self.baud = baud
        self.boot_delay = boot_delay
        self.log = log
        self.ser = None
        self.ready_at = 0.0
        self.next_try = 0.0
        self.fresh = False
        self.rebooted = False
        self.reader = LineReader()

    def _find_port(self):
        if self.port:
            return self.port if os.path.exists(self.port) else None
        for pattern in PORT_GLOBS:
            hits = sorted(glob.glob(pattern))
            if hits:
                return hits[0]
        return None

    def connect(self):
        if self.ser is not None:
            return True
        now = time.monotonic()
        if now < self.next_try:
            return False
        self.next_try = now + 2.0
        path = self._find_port()
        if not path:
            return False
        try:
            import serial
            ser = serial.Serial()
            ser.port = path
            ser.baudrate = self.baud
            ser.timeout = 0
            ser.write_timeout = 1.0
            ser.dtr = False
            ser.rts = False
            ser.open()
        except Exception as exc:
            self.log("cannot open %s: %s" % (path, exc))
            return False
        self.ser = ser
        self.ready_at = time.monotonic() + self.boot_delay
        self.fresh = True
        self.reader = LineReader()
        self.log("board connected on %s" % path)
        return True

    def take_fresh(self):
        fresh = self.fresh
        self.fresh = False
        return fresh

    def take_reboot(self):
        rebooted = self.rebooted
        self.rebooted = False
        return rebooted

    def hold(self, seconds):
        """Do not send anything for this long (the board is booting)."""
        self.ready_at = max(self.ready_at, time.monotonic() + seconds)

    def ready(self):
        return self.ser is not None and time.monotonic() >= self.ready_at

    def send(self, text):
        try:
            self.ser.write((text + "\n").encode("ascii"))
            return True
        except Exception as exc:
            self.log("serial write failed (%s), will reconnect" % exc)
            self.close()
            return False

    def drain(self):
        """Read what the board printed. Returns its lines, except the boot banner, which sets rebooted."""
        if self.ser is None:
            return []
        try:
            waiting = self.ser.in_waiting
            if not waiting:
                return []
            lines = self.reader.feed(self.ser.read(waiting))
        except Exception as exc:
            self.log("serial read failed (%s), will reconnect" % exc)
            self.close()
            return []
        out = []
        for line in lines:
            if BOOT_BANNER in line:
                self.rebooted = True
            else:
                out.append(line)
        return out

    def close(self):
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None
        self.fresh = False
        self.reader = LineReader()


def make_logger():
    def log(text):
        sys.stdout.write("%s  %s\n" % (time.strftime("%H:%M:%S"), text))
        sys.stdout.flush()
    return log


def handle_datagram(data, agg, usage, log, verbose):
    try:
        msg = json.loads(data.decode("utf-8", "replace"))
    except ValueError:
        return
    if not isinstance(msg, dict):
        return
    event = str(msg.get("e", ""))[:40]
    if event == "StatusLine":
        # Usage for the ring. Not Claude activity: a status line refresh must not keep the face awake.
        if verbose:
            log("usage %s resets_at=%s" % (msg.get("u"), msg.get("r")))
        usage.report(msg.get("u"), msg.get("r"))
        return
    sid = str(msg.get("s", ""))[:80]
    ntype = str(msg.get("t", ""))[:40]
    text = str(msg.get("m", ""))[:100]
    if verbose:
        log("event %s session=%s type=%s" % (event, sid[:8], ntype))
    agg.handle(event, sid, ntype, text)


def main():
    parser = argparse.ArgumentParser(description="Desk Buddy daemon")
    parser.add_argument("--port", default=os.environ.get("DESK_BUDDY_PORT"),
                        help="serial port, for example /dev/cu.usbserial-0001 (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--boot-delay", type=float, default=3.0,
                        help="seconds to wait after the board boots before sending a face")
    parser.add_argument("--keepalive", type=float, default=30.0,
                        help="resend the current face this often so the board does not time out")
    parser.add_argument("--work-timeout", type=float, default=180.0,
                        help="seconds without events before a working session counts as idle")
    parser.add_argument("--tool-timeout", type=float, default=600.0,
                        help="same, but while a tool call is still open (long builds and test runs)")
    parser.add_argument("--wait-timeout", type=float, default=3600.0,
                        help="seconds an alert stays up with no further events")
    parser.add_argument("--happy-hold", type=float, default=4.0, help="seconds the happy face shows")
    parser.add_argument("--error-hold", type=float, default=8.0, help="seconds the error face shows")
    parser.add_argument("--sleep-after", type=float, default=600.0,
                        help="seconds without events before the face falls asleep")
    parser.add_argument("--no-serial", action="store_true",
                        help="print faces instead of sending them (test hooks without a board)")
    parser.add_argument("--verbose", action="store_true", help="log every event")
    args = parser.parse_args()

    log = make_logger()

    if len(os.fsencode(SOCK_PATH)) > SOCK_PATH_LIMIT:
        sys.stderr.write("Socket path is too long (%d bytes, limit %d): %s\n"
                         "Set DESK_BUDDY_DIR to a shorter directory.\n"
                         % (len(os.fsencode(SOCK_PATH)), SOCK_PATH_LIMIT, SOCK_PATH))
        return 2

    if not args.no_serial:
        try:
            import serial  # noqa: F401
        except ImportError:
            sys.stderr.write("pyserial is missing. Run: pip install -r helper/requirements.txt "
                             "(inside the project's .venv)\n")
            return 2

    os.makedirs(SOCK_DIR, exist_ok=True)
    os.chmod(SOCK_DIR, 0o700)
    lock = open(LOCK_PATH, "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except (IOError, OSError):
        log("another daemon is already running, exiting")
        return 0

    if os.path.exists(SOCK_PATH):
        os.unlink(SOCK_PATH)
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    sock.bind(SOCK_PATH)
    os.chmod(SOCK_PATH, 0o600)
    sock.setblocking(False)

    def stop(signum, frame):
        raise SystemExit(0)

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    agg = Aggregator(work_timeout=args.work_timeout, tool_timeout=args.tool_timeout,
                     wait_timeout=args.wait_timeout, happy_hold=args.happy_hold,
                     error_hold=args.error_hold, sleep_after=args.sleep_after)
    usage = UsageTracker()
    board = PrintBoard(log) if args.no_serial else Board(args.port, args.baud, args.boot_delay, log)
    log("daemon started, socket %s" % SOCK_PATH)

    last_face = None
    last_sent_face = None
    last_sent_usage = None
    last_sent_at = 0.0
    try:
        while True:
            readable, _, _ = select.select([sock], [], [], 0.2)
            if readable:
                for _ in range(200):
                    try:
                        data = sock.recv(65536)
                    except BlockingIOError:
                        break
                    handle_datagram(data, agg, usage, log, args.verbose)
            agg.tick()
            board.connect()
            for line in board.drain():
                if line == "poke":
                    if args.verbose:
                        log("poke")
                    agg.poke()
            if board.take_fresh():
                last_sent_face = None
            if board.take_reboot():
                log("board booted, will resend the face")
                last_sent_face = None
                board.hold(args.boot_delay)
            face = agg.face()
            if face != last_face:
                log("face: %s" % face)
                last_face = face
            if board.ready():
                now = time.monotonic()
                if face != last_sent_face or now - last_sent_at > args.keepalive:
                    if board.send(face):
                        last_sent_face = face
                        last_sent_at = now
                        last_sent_usage = None  # the usage line always follows the face
                    else:
                        last_sent_face = None
                pct = usage.value()
                usage_line = "usage none" if pct is None else "usage %d" % pct
                if last_sent_face is not None and usage_line != last_sent_usage:
                    if board.send(usage_line):
                        last_sent_usage = usage_line
                    else:
                        last_sent_face = None
    except SystemExit:
        pass
    finally:
        if board.ready():
            board.send("idle")
        board.close()
        sock.close()
        try:
            os.unlink(SOCK_PATH)
        except OSError:
            pass
        log("daemon stopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
