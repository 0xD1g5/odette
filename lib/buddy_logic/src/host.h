// Notices when the Mac helper has gone quiet, so the board can fall back to idle.
#pragma once

#include <stdint.h>

class HostWatch {
 public:
  explicit HostWatch(uint32_t timeoutMs) : timeout_(timeoutMs) {}

  void heard(uint32_t now) {
    last_ = now;
    active_ = true;
  }

  // True once, when more than the timeout has passed since the last heard(). A heard() stamped after
  // `now` (a command read later in the same loop pass) is not silence.
  bool expired(uint32_t now) {
    if (!active_ || (int32_t)(now - last_) <= (int32_t)timeout_) return false;
    active_ = false;
    return true;
  }

  bool active() const { return active_; }

 private:
  uint32_t timeout_;
  uint32_t last_ = 0;
  bool active_ = false;
};
