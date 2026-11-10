// Per-mood 16-colour palettes for the 4-bit canvas. Drawing code uses these indexes, never RGB values,
// so a mood change only swaps the palette.
#pragma once

#include <stdint.h>

#include "mood.h"

enum PalIndex : uint8_t {
  P_BG,      // full-screen mood colour
  P_INK,     // the face
  P_WHITE,   // eye highlights, tear, speech bubble
  P_BLUSH,   // soft cheeks on calm moods
  P_CHEEK,   // strong happy cheeks
  P_TONGUE,  // happy tongue
  P_ACCENT,  // alert "!", sleepy Z
  P_TRACK,   // usage ring track: ink at 18% over the background
  P_BUBBLE,  // thinking bubble: white at 85% over the background
  P_DIM,     // resting thinking dots: ink at 30% over the bubble
  P_PROP_A,  // quirk props: each mood's palette fills these for the quirks it plays
  P_PROP_B,
  P_PROP_C,
  P_PROP_D,
  PAL_COUNT = 16,

  // Idle quirks (mug and steam use P_WHITE, the mug stripe P_CHEEK).
  P_COOKIE = P_PROP_A,
  P_CHIP = P_PROP_B,
  P_COFFEE = P_PROP_C,
  P_EMPTY = P_PROP_D,  // the owl's emptied mug: white at 75% over coffee
  // Sleepy's snore bubble.
  P_SNORE_FILL = P_PROP_A,
  P_SNORE_EDGE = P_PROP_B,
};

// 16 RGB565 entries for this mood.
const uint16_t* paletteFor(Mood m);

// The startle's own colours, shown only while it plays.
const uint16_t* startlePalette();
