// The 5-hour usage ring in a bottom corner (ring A in usage-ring-v2.html): bottom-right for Classic,
// bottom-left for Owl.
#pragma once

#include <TFT_eSPI.h>

// Track in P_TRACK, filled part in P_INK clockwise from 12 o'clock, the number in the middle.
// pct < 0 draws nothing (no usage known, or the host is gone).
const int RING_X_CLASSIC = 288, RING_X_OWL = 32;

void drawRing(TFT_eSprite& s, int pct, int cx);
