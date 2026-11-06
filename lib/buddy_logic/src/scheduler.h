// Decides when idle quirks and the sleepy snore play. Pure logic: the caller draws them.
#pragma once

#include "mood.h"

enum Quirk : uint8_t { Q_NONE, Q_WHISTLE, Q_SNACK, Q_COFFEE, Q_YAWN, Q_SNEEZE, Q_PEEK, Q_SNORE };

class QuirkScheduler {
 public:
  explicit QuirkScheduler(uint32_t seed);

  // Call on every mood (calling again with the same mood changes nothing). A change cuts off any playing quirk.
  void setMood(Mood m, uint32_t now);

  // Returns a quirk to start now, once; then Q_NONE until finished() is called.
  Quirk update(uint32_t now);

  // The quirk returned by update() has ended. Schedules the next one.
  void finished(uint32_t now);

 private:
  uint32_t random();
  uint32_t between(uint32_t lo, uint32_t hi);  // inclusive
  Quirk pick();
  void schedule(uint32_t now, uint32_t lo, uint32_t hi);

  uint32_t state_;
  Mood mood_ = M_IDLE;
  bool hasMood_ = false;
  bool armed_ = false;
  bool playing_ = false;
  uint32_t due_ = 0;
  Quirk last_ = Q_NONE;
};
