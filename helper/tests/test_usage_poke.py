import json
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from buddy_daemon import Aggregator, LineReader, UsageTracker, handle_datagram  # noqa: E402


class FakeClock(object):
    def __init__(self, t=1000.0):
        self.t = t

    def __call__(self):
        return self.t

    def advance(self, seconds):
        self.t += seconds


def quiet(_text):
    pass


class UsageTrackerTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock(1_800_000_000.0)  # wall clock, epoch seconds
        self.usage = UsageTracker(clock=self.clock)
        self.later = self.clock.t + 3600

    def test_usage_hidden_before_report(self):
        self.assertIsNone(self.usage.value())

    def test_usage_reports_and_rounds(self):
        self.usage.report(42.7, self.later)
        self.assertEqual(self.usage.value(), 43)

    def test_usage_clamps(self):
        self.usage.report(150, self.later)
        self.assertEqual(self.usage.value(), 100)
        self.usage.report(-5, self.later)
        self.assertEqual(self.usage.value(), 0)

    def test_usage_ignores_garbage(self):
        self.usage.report(30, self.later)
        self.usage.report("abc", self.later)
        self.usage.report(None, self.later)
        self.usage.report(True, self.later)
        self.usage.report(float("nan"), self.later)
        self.assertEqual(self.usage.value(), 30)

    def test_usage_accepts_numeric_strings(self):
        self.usage.report("55", self.later)
        self.assertEqual(self.usage.value(), 55)

    def test_usage_resets_after_window(self):
        self.usage.report(80, self.clock.t + 10)
        self.clock.advance(11)
        self.assertEqual(self.usage.value(), 0)

    def test_usage_reset_time_in_the_past_reads_zero(self):
        self.usage.report(80, self.clock.t - 5)
        self.assertEqual(self.usage.value(), 0)

    def test_usage_missing_reset_time(self):
        self.usage.report(55, None)
        self.clock.advance(86400)
        self.assertEqual(self.usage.value(), 55)

    def test_usage_garbage_reset_time_is_ignored(self):
        self.usage.report(55, "soon")
        self.assertEqual(self.usage.value(), 55)

    def test_usage_latest_wins(self):
        self.usage.report(20, self.later)
        self.usage.report(70, self.later)
        self.assertEqual(self.usage.value(), 70)


class PokeTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.agg = Aggregator(clock=self.clock)

    def test_poke_wakes_and_stays_awake(self):
        self.agg.handle("Stop", "a")
        self.clock.advance(601)
        self.agg.tick()
        self.assertEqual(self.agg.face(), "sleepy")
        self.agg.poke()
        self.assertEqual(self.agg.face(), "idle")
        self.clock.advance(30)  # the daemon's keepalive resends the face now
        self.agg.tick()
        self.assertEqual(self.agg.face(), "idle")

    def test_poke_does_not_clear_other_faces(self):
        self.agg.handle("UserPromptSubmit", "a")
        self.agg.poke()
        self.assertEqual(self.agg.face(), "thinking")


class DatagramTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.agg = Aggregator(clock=self.clock)
        self.wall = FakeClock(1_800_000_000.0)
        self.usage = UsageTracker(clock=self.wall)

    def send(self, msg):
        handle_datagram(json.dumps(msg).encode(), self.agg, self.usage, quiet, False)

    def test_statusline_is_not_activity(self):
        self.clock.advance(601)
        self.send({"e": "StatusLine", "s": "a", "u": 50, "r": self.wall.t + 3600})
        self.agg.tick()
        self.assertEqual(self.agg.face(), "sleepy")
        self.assertEqual(self.usage.value(), 50)

    def test_statusline_without_usage_keeps_last_value(self):
        self.send({"e": "StatusLine", "s": "a", "u": 50, "r": None})
        self.send({"e": "StatusLine", "s": "a", "u": None, "r": None})
        self.assertEqual(self.usage.value(), 50)

    def test_hook_events_still_drive_the_face(self):
        self.send({"e": "UserPromptSubmit", "s": "a", "t": "", "m": ""})
        self.assertEqual(self.agg.face(), "thinking")
        self.assertIsNone(self.usage.value())


class LineReaderTest(unittest.TestCase):
    def test_line_reader_split_chunks(self):
        r = LineReader()
        self.assertEqual(r.feed(b"Desk Bu"), [])
        self.assertEqual(r.feed(b"ddy stage 2\r\npo"), ["Desk Buddy stage 2"])
        self.assertEqual(r.feed(b"ke\n"), ["poke"])

    def test_line_reader_many_in_one(self):
        r = LineReader()
        self.assertEqual(r.feed(b"state: idle\npoke\n\npoke\n"), ["state: idle", "poke", "poke"])

    def test_line_reader_drops_overlong(self):
        r = LineReader()
        self.assertEqual(r.feed(b"x" * 300 + b"\npoke\n"), ["poke"])

    def test_line_reader_drops_overlong_across_chunks(self):
        r = LineReader()
        self.assertEqual(r.feed(b"x" * 150), [])
        self.assertEqual(r.feed(b"x" * 150), [])
        self.assertEqual(r.feed(b"tail\npoke\n"), ["poke"])

    def test_line_reader_survives_bad_bytes(self):
        r = LineReader()
        self.assertEqual(r.feed(b"\xff\xfe boot noise\npoke\n"), ["�� boot noise", "poke"])


if __name__ == "__main__":
    unittest.main()
