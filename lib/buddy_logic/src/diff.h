// Finds what changed between two 4-bit frames, so only that is pushed to the display.
#pragma once

#include <stdint.h>

struct Span {
  uint16_t x, y, w, h;  // pixels
};

// Frames are 4 bits per pixel, two pixels per byte, rows of width/2 bytes (width must be even).
// Splits the frame into bands of `bandRows` rows; for each band with a change, writes one span covering the
// changed columns across the whole band. Copies the changed bytes into `prev`. Returns the span count.
int diffFrames(uint8_t* prev, const uint8_t* cur, int width, int height, int bandRows, Span* out, int maxOut);
