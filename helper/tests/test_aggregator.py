import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from buddy_daemon import Aggregator  # noqa: E402


class FakeClock(object):
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t

    def advance(self, seconds):
        self.t += seconds


class AggregatorTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.agg = Aggregator(clock=self.clock)

    def test_starts_idle(self):
        self.assertEqual(self.agg.face(), "idle")

    def test_prompt_makes_thinking(self):
        self.agg.handle("UserPromptSubmit", "a")
        self.assertEqual(self.agg.face(), "thinking")

    def test_stop_gives_happy_then_idle(self):
        self.agg.handle("UserPromptSubmit", "a")
        self.agg.handle("Stop", "a")
        self.assertEqual(self.agg.face(), "happy")
        self.clock.advance(4.5)
        self.assertEqual(self.agg.face(), "idle")

    def test_no_happy_while_another_session_works(self):
        self.agg.handle("UserPromptSubmit", "a")
        self.agg.handle("UserPromptSubmit", "b")
        self.agg.handle("Stop", "a")
        self.assertEqual(self.agg.face(), "thinking")
        self.agg.handle("Stop", "b")
        self.assertEqual(self.agg.face(), "happy")

    def test_permission_notification_alerts_until_next_event(self):
        self.agg.handle("PreToolUse", "a")
        self.agg.handle("Notification", "a", "permission_prompt", "needs permission")
        self.assertEqual(self.agg.face(), "alert")
        self.agg.handle("PostToolUse", "a")
        self.assertEqual(self.agg.face(), "thinking")

    def test_idle_prompt_notification_is_ignored(self):
        self.agg.handle("Stop", "a")
        self.clock.advance(5)
        self.agg.handle("Notification", "a", "idle_prompt", "Claude is waiting for your input")
        self.assertEqual(self.agg.face(), "idle")

    def test_untyped_notification_uses_message(self):
        self.agg.handle("Notification", "a", "", "Claude is waiting for your input")
        self.assertEqual(self.agg.face(), "idle")
        self.agg.handle("Notification", "a", "", "Claude needs your permission to use Bash")
        self.assertEqual(self.agg.face(), "alert")

    def test_stop_failure_shows_error_then_clears(self):
        self.agg.handle("UserPromptSubmit", "a")
        self.agg.handle("StopFailure", "a")
        self.assertEqual(self.agg.face(), "error")
        self.clock.advance(8.5)
        self.assertEqual(self.agg.face(), "idle")

    def test_alert_beats_error(self):
        self.agg.handle("Notification", "a", "permission_prompt")
        self.agg.handle("StopFailure", "b")
        self.assertEqual(self.agg.face(), "alert")

    def test_working_session_times_out(self):
        self.agg.handle("UserPromptSubmit", "a")
        self.clock.advance(181)
        self.agg.tick()
        self.assertEqual(self.agg.face(), "idle")

    def test_sleepy_after_long_quiet(self):
        self.agg.handle("Stop", "a")
        self.clock.advance(601)
        self.agg.tick()
        self.assertEqual(self.agg.face(), "sleepy")
        self.agg.handle("UserPromptSubmit", "a")
        self.assertEqual(self.agg.face(), "thinking")

    def test_session_end_clears_state(self):
        self.agg.handle("Notification", "a", "permission_prompt")
        self.agg.handle("SessionEnd", "a")
        self.assertEqual(self.agg.face(), "idle")


class LongToolRuns(unittest.TestCase):
    def test_open_tool_outlives_work_timeout(self):
        t = [0.0]
        a = Aggregator(work_timeout=180, tool_timeout=600, clock=lambda: t[0])
        a.handle("UserPromptSubmit", "s")
        a.handle("PreToolUse", "s")
        t[0] = 300
        a.tick()
        self.assertEqual(a.face(), "thinking")
        t[0] = 700
        a.tick()
        self.assertNotEqual(a.face(), "thinking")

    def test_closed_tool_uses_short_timeout(self):
        t = [0.0]
        a = Aggregator(work_timeout=180, tool_timeout=600, clock=lambda: t[0])
        a.handle("PreToolUse", "s")
        a.handle("PostToolUseFailure", "s")
        t[0] = 200
        a.tick()
        self.assertNotEqual(a.face(), "thinking")


class RebootDetect(unittest.TestCase):
    def test_banner_split_across_reads(self):
        from buddy_daemon import Board

        class Fake:
            def __init__(self, chunks):
                self.chunks = list(chunks)
                self.in_waiting = 1

            def read(self, n):
                c = self.chunks.pop(0)
                self.in_waiting = 1 if self.chunks else 0
                return c

        b = Board(None, 115200, 3, lambda m: None)
        b.ser = Fake([b"junk Ode", b"tte stage 1\n"])
        b.drain()
        self.assertFalse(b.take_reboot())
        b.drain()
        self.assertTrue(b.take_reboot())


if __name__ == "__main__":
    unittest.main()
