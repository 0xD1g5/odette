#include "scheduler.h"

// Timing from the design spec, in ms.
static const uint32_t GRACE_MIN = 60000, GRACE_MAX = 90000;     // after entering idle
static const uint32_t GAP_MIN = 120000, GAP_MAX = 300000;       // between idle quirks
static const uint32_t SNORE_MIN = 60000, SNORE_MAX = 120000;    // between snores

// Whistle, coffee and yawn are common, snack occasional, sneeze and peek-a-boo rare.
static const struct {
  Quirk quirk;
  uint8_t weight;
} WEIGHTS[] = {
    {Q_WHISTLE, 3}, {Q_COFFEE, 3}, {Q_YAWN, 3}, {Q_SNACK, 2}, {Q_SNEEZE, 1}, {Q_PEEK, 1},
};

QuirkScheduler::QuirkScheduler(uint32_t seed) : state_(seed ? seed : 0x9E3779B9u) {}

uint32_t QuirkScheduler::random() {
  // xorshift32
  state_ ^= state_ << 13;
  state_ ^= state_ >> 17;
  state_ ^= state_ << 5;
  return state_;
}

uint32_t QuirkScheduler::between(uint32_t lo, uint32_t hi) { return lo + random() % (hi - lo + 1); }

void QuirkScheduler::schedule(uint32_t now, uint32_t lo, uint32_t hi) {
  due_ = now + between(lo, hi);
  armed_ = true;
}

Quirk QuirkScheduler::pick() {
  uint32_t total = 0;
  for (const auto& w : WEIGHTS) {
    if (w.quirk != last_) total += w.weight;
  }
  uint32_t r = random() % total;
  for (const auto& w : WEIGHTS) {
    if (w.quirk == last_) continue;
    if (r < w.weight) return w.quirk;
    r -= w.weight;
  }
  return WEIGHTS[0].quirk;  // not reached
}

void QuirkScheduler::setMood(Mood m, uint32_t now) {
  if (hasMood_ && m == mood_) return;
  hasMood_ = true;
  mood_ = m;
  playing_ = false;
  armed_ = false;
  if (m == M_IDLE) schedule(now, GRACE_MIN, GRACE_MAX);
  if (m == M_SLEEPY) schedule(now, SNORE_MIN, SNORE_MAX);
}

Quirk QuirkScheduler::update(uint32_t now) {
  if (!armed_ || playing_ || !reached(now, due_)) return Q_NONE;
  armed_ = false;
  playing_ = true;
  if (mood_ == M_SLEEPY) return Q_SNORE;
  last_ = pick();
  return last_;
}

void QuirkScheduler::finished(uint32_t now) {
  if (!playing_) return;
  playing_ = false;
  if (mood_ == M_IDLE) schedule(now, GAP_MIN, GAP_MAX);
  if (mood_ == M_SLEEPY) schedule(now, SNORE_MIN, SNORE_MAX);
}
