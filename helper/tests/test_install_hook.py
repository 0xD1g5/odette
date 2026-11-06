import copy
import json
import os
import socket
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
HELPER = os.path.dirname(HERE)
sys.path.insert(0, HELPER)
import install  # noqa: E402

HOOK = os.path.join(HELPER, "buddy_hook.py")


class InstallerPure(unittest.TestCase):
    def installed(self, base):
        s = copy.deepcopy(base)
        install.strip_ours(s)
        install.add_ours(s, "/usr/bin/python3", "/h/buddy_hook.py")
        return s

    def test_idempotent(self):
        once = self.installed({})
        twice = self.installed(once)
        self.assertEqual(once, twice)

    def test_command_is_safe(self):
        s = self.installed({})
        cmd = s["hooks"]["Stop"][0]["hooks"][0]["command"]
        self.assertTrue(cmd.endswith("Stop || true"))
        self.assertIn("PostToolUseFailure", s["hooks"])

    def test_uninstall_restores_and_keeps_others(self):
        base = {"model": "x", "hooks": {"Stop": [{"hooks": [{"type": "command", "command": "echo mine"}]}]}}
        s = self.installed(base)
        install.strip_ours(s)
        self.assertEqual(s, base)

    def test_empty_groups_dropped(self):
        s = self.installed({})
        install.strip_ours(s)
        self.assertNotIn("hooks", s)

    def test_rejects_non_object_hooks(self):
        for fn in (install.strip_ours, lambda x: install.add_ours(x, "p", "h")):
            with self.assertRaises(ValueError):
                fn({"hooks": ["oops"]})


class Relay(unittest.TestCase):
    def send(self, payload):
        d = tempfile.mkdtemp(prefix="db")
        path = os.path.join(d, "buddy.sock")
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        srv.bind(path)
        srv.settimeout(5)
        env = dict(os.environ, DESK_BUDDY_DIR=d)
        r = subprocess.run([sys.executable, HOOK, "PostToolUse"], input=payload, env=env,
                           capture_output=True, timeout=20)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout, b"")
        return json.loads(srv.recv(4096))

    def test_small(self):
        m = self.send(json.dumps({"session_id": "real"}).encode())
        self.assertEqual(m["s"], "real")

    def test_large_payload_keeps_session(self):
        big = json.dumps({"session_id": "real", "tool_response": "x" * 200000}).encode()
        self.assertEqual(self.send(big)["s"], "real")

    def test_truncated_json_keeps_session(self):
        self.assertEqual(self.send(b'{"session_id": "real", "tool_response": "abc')["s"], "real")

    def test_no_daemon_exits_zero(self):
        env = dict(os.environ, DESK_BUDDY_DIR=tempfile.mkdtemp())
        r = subprocess.run([sys.executable, HOOK, "Stop"], input=b"{}", env=env, capture_output=True, timeout=10)
        self.assertEqual(r.returncode, 0)


if __name__ == "__main__":
    unittest.main()
