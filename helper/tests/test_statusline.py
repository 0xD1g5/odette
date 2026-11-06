import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
HELPER = os.path.dirname(HERE)
sys.path.insert(0, HELPER)
import install  # noqa: E402

HOOK = os.path.join(HELPER, "buddy_hook.py")
INSTALL = os.path.join(HELPER, "install.py")

SAMPLE = """#!/usr/bin/env bash
# a status line like the owner's
input=$(cat)

RESET=$'\\x1b[0m'
echo "chars: ${#input}${RESET}"
"""


def relay(payload):
    """Runs the relay in StatusLine mode against a throwaway socket; returns (datagram, process)."""
    d = tempfile.mkdtemp(prefix="db")
    path = os.path.join(d, "buddy.sock")
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    srv.bind(path)
    srv.settimeout(5)
    env = dict(os.environ, DESK_BUDDY_DIR=d)
    proc = subprocess.run([sys.executable, HOOK, "StatusLine"], input=payload, env=env,
                          capture_output=True, timeout=20)
    try:
        return json.loads(srv.recv(4096)), proc
    finally:
        srv.close()


class RelayStatusLine(unittest.TestCase):
    def test_relay_forwards_usage(self):
        payload = {"session_id": "s1", "model": {"display_name": "Opus"},
                   "rate_limits": {"five_hour": {"used_percentage": 37.5, "resets_at": 1793999999}}}
        msg, proc = relay(json.dumps(payload).encode())
        self.assertEqual(msg["e"], "StatusLine")
        self.assertEqual(msg["u"], 37.5)
        self.assertEqual(msg["r"], 1793999999)
        self.assertEqual(proc.returncode, 0)
        self.assertEqual(proc.stdout, b"")

    def test_relay_usage_missing(self):
        msg, proc = relay(json.dumps({"session_id": "s1"}).encode())
        self.assertIsNone(msg["u"])
        self.assertIsNone(msg["r"])
        self.assertEqual(proc.returncode, 0)
        self.assertEqual(proc.stdout, b"")

    def test_relay_usage_wrong_types(self):
        for limits in (None, {"five_hour": "x"}, {"five_hour": {"used_percentage": "37", "resets_at": True}}):
            msg, proc = relay(json.dumps({"session_id": "s1", "rate_limits": limits}).encode())
            self.assertIsNone(msg["u"], limits)
            self.assertIsNone(msg["r"], limits)
            self.assertEqual(proc.returncode, 0)


class StatusLineEdit(unittest.TestCase):
    def test_add_line_after_input_cat(self):
        out = install.add_statusline_line(SAMPLE, "/usr/bin/python3", "/h/buddy_hook.py")
        lines = out.splitlines()
        i = lines.index("input=$(cat)")
        self.assertEqual(lines[i + 1], 'printf \'%s\' "$input" | "/usr/bin/python3" "/h/buddy_hook.py" StatusLine'
                                       ' >/dev/null 2>&1 & # desk-buddy')
        self.assertEqual(len(lines), len(SAMPLE.splitlines()) + 1)

    def test_add_line_other_var_name(self):
        out = install.add_statusline_line("payload=$(cat)\necho hi\n", "p", "h")
        self.assertIn('printf \'%s\' "$payload" |', out.splitlines()[1])

    def test_add_line_keeps_indentation(self):
        out = install.add_statusline_line("main() {\n  input=$(cat)\n}\n", "p", "h")
        self.assertTrue(out.splitlines()[2].startswith("  printf"))

    def test_add_line_idempotent(self):
        once = install.add_statusline_line(SAMPLE, "p", "h")
        self.assertEqual(install.add_statusline_line(once, "p", "h"), once)

    def test_add_line_unsupported(self):
        self.assertIsNone(install.add_statusline_line("#!/bin/sh\necho static\n", "p", "h"))

    def test_strip_restores_original(self):
        self.assertEqual(install.strip_statusline_line(install.add_statusline_line(SAMPLE, "p", "h")), SAMPLE)

    def test_statusline_script_forms(self):
        home = os.path.realpath(tempfile.mkdtemp())  # macOS temp dirs sit behind the /var symlink
        script = os.path.join(home, "x.sh")
        with open(script, "w") as handle:
            handle.write(SAMPLE)
        old_home = os.environ.get("HOME")
        os.environ["HOME"] = home
        try:
            for command in ("bash ~/x.sh", "sh %s" % script, script, "~/x.sh"):
                settings = {"statusLine": {"type": "command", "command": command}}
                self.assertEqual(install.statusline_script(settings), script, command)
            for settings in ({"statusLine": {"type": "command", "command": "echo hi"}},
                             {"statusLine": {"type": "command", "command": "bash ~/missing.sh"}},
                             {"statusLine": "bash ~/x.sh"}, {}):
                self.assertIsNone(install.statusline_script(settings), settings)
        finally:
            if old_home is None:
                del os.environ["HOME"]
            else:
                os.environ["HOME"] = old_home

    def test_status_output_unchanged_and_fast(self):
        d = tempfile.mkdtemp()
        slow = os.path.join(d, "slow_relay.py")
        with open(slow, "w") as handle:
            handle.write("import sys, time\nsys.stdin.read()\ntime.sleep(3)\nprint('relay must stay silent')\n")
        plain = os.path.join(d, "plain.sh")
        hooked = os.path.join(d, "hooked.sh")
        with open(plain, "w") as handle:
            handle.write(SAMPLE)
        with open(hooked, "w") as handle:
            handle.write(install.add_statusline_line(SAMPLE, sys.executable, slow))
        stdin = json.dumps({"model": {"display_name": "Opus"}}).encode()
        expected = subprocess.run(["bash", plain], input=stdin, capture_output=True, timeout=10)
        start = time.monotonic()
        got = subprocess.run(["bash", hooked], input=stdin, capture_output=True, timeout=10)
        self.assertLess(time.monotonic() - start, 1.0)
        self.assertEqual(got.stdout, expected.stdout)
        self.assertEqual(got.returncode, 0)


class StatusLineCli(unittest.TestCase):
    """install.py end to end, in a throwaway HOME, so the real settings and script are never touched."""

    def setUp(self):
        self.home = tempfile.mkdtemp()
        self.script = os.path.join(self.home, "sl.sh")
        with open(self.script, "w") as handle:
            handle.write(SAMPLE)
        self.settings = os.path.join(self.home, "settings.json")
        with open(self.settings, "w") as handle:
            json.dump({"statusLine": {"type": "command", "command": "bash ~/sl.sh"}}, handle)

    def run_install(self, *flags):
        env = dict(os.environ, HOME=self.home, PATH="/usr/bin:/bin")
        return subprocess.run([sys.executable, INSTALL, "--settings", self.settings] + list(flags),
                              env=env, capture_output=True, text=True, timeout=60)

    def read_script(self):
        with open(self.script) as handle:
            return handle.read()

    def backups(self):
        return [n for n in os.listdir(self.home) if n.startswith("sl.sh.deskbuddy-backup-")]

    def test_dry_run_shows_diff_and_writes_nothing(self):
        r = self.run_install("--statusline")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("+printf '%s' \"$input\"", r.stdout)
        self.assertEqual(self.read_script(), SAMPLE)

    def test_apply_then_uninstall(self):
        r = self.run_install("--statusline", "--apply")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("# desk-buddy", self.read_script())
        self.assertEqual(len(self.backups()), 1)
        r = self.run_install("--uninstall", "--apply")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(self.read_script(), SAMPLE)

    def test_apply_keeps_symlinked_script_a_symlink(self):
        dotfiles = os.path.join(self.home, "dotfiles")
        os.mkdir(dotfiles)
        real = os.path.join(dotfiles, "sl.sh")
        os.replace(self.script, real)
        os.symlink(real, self.script)
        r = self.run_install("--statusline", "--apply")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(os.path.islink(self.script))
        with open(real) as handle:
            self.assertIn("# desk-buddy", handle.read())
        r = self.run_install("--uninstall", "--apply")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(os.path.islink(self.script))
        self.assertEqual(self.read_script(), SAMPLE)

    def test_plain_install_leaves_script_alone(self):
        r = self.run_install("--apply")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(self.read_script(), SAMPLE)

    def test_unsupported_setup_prints_manual_line(self):
        with open(self.settings, "w") as handle:
            json.dump({"statusLine": {"type": "command", "command": "echo static"}}, handle)
        r = self.run_install("--statusline", "--apply")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("StatusLine >/dev/null 2>&1 &", r.stdout)
        self.assertEqual(self.read_script(), SAMPLE)


if __name__ == "__main__":
    unittest.main()
