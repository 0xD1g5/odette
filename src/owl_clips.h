// The owl's clips (owl-clips.html): the same quirks and poke reactions as the Classic ones, with no mouth.
// What the mouth did moves to the eyes, brows and cheeks, or to the bottom centre, just above the printed beak.
#pragma once

#include <TFT_eSPI.h>

#include "clips.h"

// Same contract as drawClip(): draws the whole face for clip `id`, `t` ms in, and returns false once it is over.
bool drawOwlClip(TFT_eSprite& s, ClipId id, Mood m, uint32_t t, uint32_t now, Pose p);
