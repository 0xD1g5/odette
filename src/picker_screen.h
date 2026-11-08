// The boot picker screen (picker.html): two tiles with blinking previews, and a countdown bar.
// Drawn in the idle palette.
#pragma once

#include <TFT_eSPI.h>

#include "picker.h"

void drawPicker(TFT_eSprite& s, const StylePicker& p, uint32_t now);
