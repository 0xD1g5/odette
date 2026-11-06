#include "backlight.h"

#include <Arduino.h>

#include "fade.h"

static const uint8_t CHANNEL = 0;
static const uint8_t STEP = 8;  // per frame: full to dim in about a second
static uint8_t level = 255, target = 255;

void backlightBegin() {
  ledcSetup(CHANNEL, 5000, 8);
  ledcAttachPin(TFT_BL, CHANNEL);
  ledcWrite(CHANNEL, level);
}

void backlightTarget(uint8_t t) { target = t; }

void backlightUpdate() {
  if (level == target) return;
  level = fadeToward(level, target, STEP);
  ledcWrite(CHANNEL, level);
}

void backlightSnap(uint8_t l) {
  level = target = l;
  ledcWrite(CHANNEL, level);
}
