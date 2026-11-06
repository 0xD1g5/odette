// Host-side tests for lib/buddy_logic. Run with: pio test -e native

#include <unity.h>

#include "mood.h"
#include "poke.h"
#include "protocol.h"
#include "scheduler.h"

void setUp() {}
void tearDown() {}

// ---- protocol ----

void test_parse_moods() {
  TEST_ASSERT_EQUAL(CMD_MOOD, parseLine("thinking").type);
  TEST_ASSERT_EQUAL(M_THINKING, parseLine("thinking").mood);
  TEST_ASSERT_EQUAL(M_ALERT, parseLine("  ALERT\r").mood);
  TEST_ASSERT_EQUAL(CMD_MOOD, parseLine("  ALERT\r").type);
  TEST_ASSERT_EQUAL(M_IDLE, parseLine("idle").mood);
  TEST_ASSERT_EQUAL(M_ERROR, parseLine("error").mood);
}

void test_parse_next_and_empty() {
  TEST_ASSERT_EQUAL(CMD_NEXT, parseLine("next").type);
  TEST_ASSERT_EQUAL(CMD_NONE, parseLine("").type);
  TEST_ASSERT_EQUAL(CMD_NONE, parseLine("   \r").type);
  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseLine("dance").type);
}

void test_parse_usage() {
  TEST_ASSERT_EQUAL(CMD_USAGE, parseLine("usage 42").type);
  TEST_ASSERT_EQUAL(42, parseLine("usage 42").usage);
  TEST_ASSERT_EQUAL(7, parseLine("USAGE  7\r").usage);
  TEST_ASSERT_EQUAL(-1, parseLine("usage none").usage);
  TEST_ASSERT_EQUAL(CMD_USAGE, parseLine("usage none").type);
  TEST_ASSERT_EQUAL(100, parseLine("usage 150").usage);
  TEST_ASSERT_EQUAL(0, parseLine("usage -3").usage);
}

void test_parse_usage_garbage() {
  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseLine("usage abc").type);
  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseLine("usage 42abc").type);
  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseLine("usage").type);
  TEST_ASSERT_EQUAL(CMD_UNKNOWN, parseLine("usage ").type);
}

void test_mood_names() {
  TEST_ASSERT_EQUAL_STRING("sleepy", MOOD_NAMES[M_SLEEPY]);
  TEST_ASSERT_EQUAL(M_HAPPY, moodFromName("happy"));
  TEST_ASSERT_EQUAL(-1, moodFromName("grumpy"));
}

// ---- scheduler ----

// Steps time forward from `from` until a quirk is returned or `until` passes. Returns the time, or 0 if none.
static uint32_t firstQuirk(QuirkScheduler& s, uint32_t from, uint32_t until, Quirk* out, uint32_t step = 250) {
  for (uint32_t t = from; (int32_t)(until - t) >= 0; t += step) {
    Quirk q = s.update(t);
    if (q != Q_NONE) {
      if (out) *out = q;
      return t;
    }
  }
  return 0;
}

void test_no_quirk_during_grace() {
  for (uint32_t seed = 1; seed <= 50; seed++) {
    QuirkScheduler s(seed);
    s.setMood(M_IDLE, 1000);
    TEST_ASSERT_EQUAL_UINT32(0, firstQuirk(s, 1000, 1000 + 59999, nullptr));
  }
}

void test_quirk_after_grace() {
  for (uint32_t seed = 1; seed <= 50; seed++) {
    QuirkScheduler s(seed);
    s.setMood(M_IDLE, 1000);
    TEST_ASSERT_NOT_EQUAL(0, firstQuirk(s, 1000, 1000 + 90000, nullptr));
  }
}

void test_same_mood_does_not_restart_grace() {
  QuirkScheduler s(7);
  s.setMood(M_IDLE, 1000);
  for (uint32_t t = 1000; t < 1000 + 50000; t += 1000) s.setMood(M_IDLE, t);  // called every frame by main
  TEST_ASSERT_NOT_EQUAL(0, firstQuirk(s, 51000, 1000 + 90000, nullptr));
}

void test_returns_once_until_finished() {
  QuirkScheduler s(3);
  s.setMood(M_IDLE, 1000);
  uint32_t t = firstQuirk(s, 1000, 1000 + 90000, nullptr);
  TEST_ASSERT_NOT_EQUAL(0, t);
  TEST_ASSERT_EQUAL_UINT32(0, firstQuirk(s, t + 250, t + 600000, nullptr, 1000));
}

void test_gap_after_finished() {
  for (uint32_t seed = 1; seed <= 20; seed++) {
    QuirkScheduler s(seed);
    s.setMood(M_IDLE, 1000);
    uint32_t t = firstQuirk(s, 1000, 1000 + 90000, nullptr);
    s.finished(t + 5000);
    uint32_t done = t + 5000;
    TEST_ASSERT_EQUAL_UINT32(0, firstQuirk(s, done, done + 119999, nullptr, 1000));
    TEST_ASSERT_NOT_EQUAL(0, firstQuirk(s, done + 120000, done + 300000, nullptr, 1000));
  }
}

void test_never_repeats() {
  QuirkScheduler s(11);
  s.setMood(M_IDLE, 1000);
  uint32_t t = 1000;
  Quirk prev = Q_NONE;
  for (int i = 0; i < 200; i++) {
    Quirk q = Q_NONE;
    t = firstQuirk(s, t, t + 400000, &q, 1000);
    TEST_ASSERT_NOT_EQUAL(0, t);
    TEST_ASSERT_NOT_EQUAL(prev, q);
    prev = q;
    s.finished(t);
  }
}

void test_weights() {
  QuirkScheduler s(12345);
  s.setMood(M_IDLE, 1000);
  int count[8] = {0};
  uint32_t t = 1000;
  for (int i = 0; i < 3000; i++) {
    Quirk q = Q_NONE;
    t = firstQuirk(s, t, t + 400000, &q, 5000);
    count[q]++;
    s.finished(t);
  }
  TEST_ASSERT_EQUAL(0, count[Q_SNORE]);
  TEST_ASSERT_EQUAL(0, count[Q_NONE]);
  const Quirk common[] = {Q_WHISTLE, Q_COFFEE, Q_YAWN};
  const Quirk rare[] = {Q_SNEEZE, Q_PEEK};
  for (Quirk c : common) TEST_ASSERT_GREATER_THAN(count[Q_SNACK], count[c]);
  for (Quirk r : rare) TEST_ASSERT_LESS_THAN(count[Q_SNACK], count[r]);
  for (Quirk r : rare) TEST_ASSERT_GREATER_THAN(0, count[r]);
}

void test_no_quirks_in_busy_moods() {
  const Mood busy[] = {M_THINKING, M_ALERT, M_ERROR, M_HAPPY};
  for (Mood m : busy) {
    QuirkScheduler s(5);
    s.setMood(m, 1000);
    TEST_ASSERT_EQUAL_UINT32(0, firstQuirk(s, 1000, 1000 + 3600000, nullptr, 1000));
  }
}

void test_sleepy_only_snores() {
  for (uint32_t seed = 1; seed <= 20; seed++) {
    QuirkScheduler s(seed);
    s.setMood(M_SLEEPY, 1000);
    Quirk q = Q_NONE;
    uint32_t t = firstQuirk(s, 1000, 1000 + 130000, &q, 100);
    TEST_ASSERT_EQUAL(Q_SNORE, q);
    TEST_ASSERT_TRUE(t >= 1000 + 60000 && t <= 1000 + 120000);
    s.finished(t + 4000);
    uint32_t done = t + 4000;
    uint32_t t2 = firstQuirk(s, done, done + 130000, &q, 100);
    TEST_ASSERT_EQUAL(Q_SNORE, q);
    TEST_ASSERT_TRUE(t2 >= done + 60000 && t2 <= done + 120000);
  }
}

void test_mood_change_restarts_grace() {
  QuirkScheduler s(9);
  s.setMood(M_IDLE, 1000);
  s.setMood(M_THINKING, 50000);
  s.setMood(M_IDLE, 100000);
  TEST_ASSERT_EQUAL_UINT32(0, firstQuirk(s, 100000, 100000 + 59999, nullptr));
  TEST_ASSERT_NOT_EQUAL(0, firstQuirk(s, 160000, 100000 + 90000, nullptr));
}

void test_mood_change_while_playing_clears_it() {
  QuirkScheduler s(4);
  s.setMood(M_IDLE, 1000);
  uint32_t t = firstQuirk(s, 1000, 1000 + 90000, nullptr);
  s.setMood(M_THINKING, t + 1000);  // quirk is cut off; no finished() call comes
  s.setMood(M_IDLE, t + 2000);
  TEST_ASSERT_NOT_EQUAL(0, firstQuirk(s, t + 2000, t + 2000 + 90000, nullptr));
}

void test_wraps_millis() {
  const uint32_t start = 0xFFFFF000u;  // about 4 s before millis() wraps
  QuirkScheduler s(2);
  s.setMood(M_IDLE, start);
  TEST_ASSERT_EQUAL_UINT32(0, firstQuirk(s, start, start + 59999, nullptr));
  TEST_ASSERT_NOT_EQUAL(0, firstQuirk(s, start + 60000, start + 90000, nullptr));
}

// ---- poke ----

void test_poke_debounce() {
  PokeDetector p;
  TEST_ASSERT_FALSE(p.update(true, 1000));
  TEST_ASSERT_FALSE(p.update(true, 1010));
  TEST_ASSERT_FALSE(p.update(true, 1020));  // held 20 ms
  TEST_ASSERT_TRUE(p.update(true, 1030));   // held 30 ms
  TEST_ASSERT_FALSE(p.update(true, 1040));  // same press
  TEST_ASSERT_FALSE(p.update(true, 1500));
  TEST_ASSERT_FALSE(p.update(false, 1550));
  TEST_ASSERT_FALSE(p.update(false, 1600));  // released 50 ms
  TEST_ASSERT_FALSE(p.update(true, 1600));
  TEST_ASSERT_FALSE(p.update(true, 1640));  // release was too short: still the first press
  TEST_ASSERT_FALSE(p.update(false, 1650));
  TEST_ASSERT_FALSE(p.update(false, 1750));  // released 100 ms
  TEST_ASSERT_FALSE(p.update(true, 1750));
  TEST_ASSERT_TRUE(p.update(true, 1780));
}

void test_reactions() {
  for (int m = 0; m < MOOD_COUNT; m++) TEST_ASSERT_EQUAL(R_NONE, chooseReaction((Mood)m, true, 0));
  TEST_ASSERT_EQUAL(R_STARTLE, chooseReaction(M_SLEEPY, false, 0));
  TEST_ASSERT_EQUAL(R_STARTLE, chooseReaction(M_SLEEPY, false, 1));
  TEST_ASSERT_EQUAL(R_GIGGLE, chooseReaction(M_IDLE, false, 0));
  TEST_ASSERT_EQUAL(R_HIDE, chooseReaction(M_IDLE, false, 1));
  const Mood others[] = {M_THINKING, M_ALERT, M_ERROR, M_HAPPY};
  for (Mood m : others) {
    TEST_ASSERT_EQUAL(R_QUICK_GIGGLE, chooseReaction(m, false, 0));
    TEST_ASSERT_EQUAL(R_QUICK_GIGGLE, chooseReaction(m, false, 1));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_moods);
  RUN_TEST(test_parse_next_and_empty);
  RUN_TEST(test_parse_usage);
  RUN_TEST(test_parse_usage_garbage);
  RUN_TEST(test_mood_names);
  RUN_TEST(test_no_quirk_during_grace);
  RUN_TEST(test_quirk_after_grace);
  RUN_TEST(test_same_mood_does_not_restart_grace);
  RUN_TEST(test_returns_once_until_finished);
  RUN_TEST(test_gap_after_finished);
  RUN_TEST(test_never_repeats);
  RUN_TEST(test_weights);
  RUN_TEST(test_no_quirks_in_busy_moods);
  RUN_TEST(test_sleepy_only_snores);
  RUN_TEST(test_mood_change_restarts_grace);
  RUN_TEST(test_mood_change_while_playing_clears_it);
  RUN_TEST(test_wraps_millis);
  RUN_TEST(test_poke_debounce);
  RUN_TEST(test_reactions);
  return UNITY_END();
}
