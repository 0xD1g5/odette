// The 5-hour usage ring in the bottom-right corner (ring A in usage-ring-v2.html).
#pragma once

#include <TFT_eSPI.h>

// Track in P_TRACK, filled part in P_INK clockwise from 12 o'clock, the number in the middle.
// pct < 0 draws nothing (no usage known, or the host is gone).
void drawRing(TFT_eSprite& s, int pct);
