// One full-screen 4-bit palette sprite (320x240, 38,400 bytes). Every frame is drawn here and then pushed,
// so the coloured background never flickers.
#pragma once

#include <TFT_eSPI.h>

#include "mood.h"

// Initialises the display and allocates the sprite plus a copy of the last pushed frame. False if that fails.
bool canvasBegin();
TFT_eSPI& display();
TFT_eSprite& canvas();

// Swaps in the mood's palette. Every pixel changes colour, so the next push is the whole screen.
void canvasSetMood(Mood m);
// The same for any 16-entry palette.
void canvasSetPalette(const uint16_t* palette);

// Sends what changed since the last push, band by band (or everything when `full`). A full push takes
// ~38 ms; a blink or glance only a few ms (measured, SPI 40 MHz).
void canvasPush(bool full);
