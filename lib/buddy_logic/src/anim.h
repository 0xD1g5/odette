// Timing helpers for clips (poke reactions and idle quirks), shared by the Classic and Owl styles. All take
// the clip's own clock `t`, in ms since it started.
#pragma once

#include <math.h>
#include <stdint.h>

const float PI_F = 3.14159265f;

inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
inline float easeInOut(float x) { return 0.5f - 0.5f * cosf(PI_F * clamp01(x)); }

// Progress through [start, end) of the clip, eased.
inline float segment(uint32_t t, uint32_t start, uint32_t end) {
  return easeInOut((float)((int32_t)t - (int32_t)start) / (end - start));
}

// A dip from 1 down to `low` and back over [start, end): a slow blink.
inline float dip(uint32_t t, uint32_t start, uint32_t end, float low) {
  if (t < start || t >= end) return 1;
  return 1 - (1 - low) * sinf(PI_F * (t - start) / (end - start));
}

// A keyframe track like the mockups' CSS @keyframes: value `v` at `at` ms, eased in between.
struct Key {
  uint32_t at;
  float v;
};

template <int N>
float track(uint32_t t, const Key (&k)[N]) {
  if (t <= k[0].at) return k[0].v;
  for (int i = 1; i < N; i++) {
    if (t < k[i].at) return k[i - 1].v + (k[i].v - k[i - 1].v) * segment(t, k[i - 1].at, k[i].at);
  }
  return k[N - 1].v;
}

// 0..1..0 over `period` ms.
inline float wave(uint32_t t, uint32_t period) { return 0.5f - 0.5f * cosf(2 * PI_F * (t % period) / period); }

// Where a repeating effect (note, steam, crumb) is in its own cycle, 0..1.
inline float cycle(uint32_t t, uint32_t period, uint32_t offset) {
  return ((t + period - offset) % period) / (float)period;
}
