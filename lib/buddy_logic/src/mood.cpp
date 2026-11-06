#include "mood.h"

#include <string.h>

const char* const MOOD_NAMES[MOOD_COUNT] = {"idle", "happy", "thinking", "sleepy", "alert", "error"};

int moodFromName(const char* s) {
  for (int i = 0; i < MOOD_COUNT; i++) {
    if (strcmp(s, MOOD_NAMES[i]) == 0) return i;
  }
  return -1;
}
