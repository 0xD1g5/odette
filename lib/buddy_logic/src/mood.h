// Moods the face can show. The names are also the serial commands the Mac helper sends.
#pragma once

#include <stdint.h>

enum Mood : uint8_t { M_IDLE, M_HAPPY, M_THINKING, M_SLEEPY, M_ALERT, M_ERROR, MOOD_COUNT };

extern const char* const MOOD_NAMES[MOOD_COUNT];

// Index of a lowercase mood name, or -1.
int moodFromName(const char* s);

// True once `now` has reached `t`. Safe across the millis() wrap at ~49.7 days, unlike now >= t.
inline bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }
