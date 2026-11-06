// Backlight brightness by PWM on TFT_BL, so sleepy can dim the whole screen instead of glowing.
#pragma once

#include <stdint.h>

void backlightBegin();                 // call after the display is initialised (it drives TFT_BL high)
void backlightTarget(uint8_t level);  // 0-255; reached gradually by backlightUpdate()
void backlightUpdate();               // once per frame
void backlightSnap(uint8_t level);    // jump straight to `level`, no fade (a startled wake-up)
