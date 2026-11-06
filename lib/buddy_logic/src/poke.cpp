#include "poke.h"

static const uint32_t HOLD_MS = 30;
static const uint32_t RELEASE_MS = 100;

bool PokeDetector::update(bool pressed, uint32_t now) {
  if (!pressed) {
    if (down_) {
      down_ = false;
      releaseStart_ = now;
    }
    return false;
  }
  if (!down_) {
    down_ = true;
    bool newPress = !everPressed_ || now - releaseStart_ >= RELEASE_MS;
    everPressed_ = true;
    if (newPress) {
      pressStart_ = now;
      fired_ = false;
    }
  }
  if (!fired_ && now - pressStart_ >= HOLD_MS) {
    fired_ = true;
    return true;
  }
  return false;
}

Reaction chooseReaction(Mood m, bool busy, uint32_t rnd) {
  if (busy) return R_NONE;
  if (m == M_SLEEPY) return R_STARTLE;
  if (m == M_IDLE) return (rnd % 2) ? R_HIDE : R_GIGGLE;
  return R_QUICK_GIGGLE;
}
