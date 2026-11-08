// The boot picker's choice and countdown. The saved style starts after 30 s untouched; each tap flips
// the highlight and leaves 5 s to change your mind.
#pragma once

#include <stdint.h>

#include "mood.h"
#include "style.h"

class StylePicker {
 public:
  static const uint32_t FIRST_MS = 30000, AFTER_TAP_MS = 5000;

  StylePicker(Style initial, uint32_t now) : style_(initial), window_(FIRST_MS), deadline_(now + FIRST_MS) {}

  void tap(uint32_t now) {
    style_ = otherStyle(style_);
    window_ = AFTER_TAP_MS;
    deadline_ = now + AFTER_TAP_MS;
  }

  Style highlighted() const { return style_; }
  bool done(uint32_t now) const { return reached(now, deadline_); }
  uint32_t msLeft(uint32_t now) const { return done(now) ? 0 : deadline_ - now; }
  // Length of the current countdown, for drawing how much of it is left.
  uint32_t windowMs() const { return window_; }

 private:
  Style style_;
  uint32_t window_;
  uint32_t deadline_;
};
