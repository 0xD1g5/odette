// One full-screen 4-bit palette sprite (320x240, 38,400 bytes). Every frame is drawn here and then pushed,
// so the coloured background never flickers.
#pragma once

#include <TFT_eSPI.h>

#include "mood.h"

struct Rect {
  int16_t x, y, w, h;
};

const Rect FULL_RECT = {0, 0, 320, 240};

Rect unionRect(const Rect& a, const Rect& b);

// Initialises the display and allocates the sprite. False if the allocation fails.
bool canvasBegin();
TFT_eSPI& display();
TFT_eSprite& canvas();

// Swaps in the mood's palette. The next push should be FULL_RECT, since the background changed.
void canvasSetMood(Mood m);

// Pushes the union of `dirty` and the previous push's rect, so whatever moved out of an area is erased.
// A full push takes ~38 ms, a face-sized one ~18-27 ms (measured, SPI 40 MHz).
void canvasPush(const Rect& dirty);
