// Turning raw touch readings into pokes, and choosing how the face reacts.
#pragma once

#include "mood.h"

// Debounces the resistive panel. A poke is a press held at least 30 ms; a release shorter
// than 100 ms counts as a bounce, so it does not start a new press.
class PokeDetector {
 public:
  // Call every frame with the raw pressed state. True once per poke.
  bool update(bool pressed, uint32_t now);

 private:
  bool down_ = false;
  bool everPressed_ = false;
  bool fired_ = false;
  uint32_t pressStart_ = 0;
  uint32_t releaseStart_ = 0;
};

enum Reaction : uint8_t { R_NONE, R_STARTLE, R_GIGGLE, R_HIDE, R_QUICK_GIGGLE };

// busy: a reaction is already playing (extra pokes are ignored). rnd: any random number.
// Hiding only happens in idle, so it never covers a cue.
Reaction chooseReaction(Mood m, bool busy, uint32_t rnd);
