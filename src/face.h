// The six mood faces, drawn from the approved mockups (look-so-far.html, SVG at 320x240 = screen coordinates).
#pragma once

#include <TFT_eSPI.h>

#include "mood.h"

// Where the face is and how it looks this frame. Moods fill it in moodMotion(); clips (quirks, pokes) adjust it.
struct Pose {
  int16_t dx = 0, dy = 0;  // whole-face offset in pixels
  float squash = 1;        // vertical stretch (>1 taller)
  float eyeOpen = 1;       // 1 open, ~0.08 mid-blink
  float gazeX = 0, gazeY = 0;  // -1..1, moves eyes and brows (idle glance)
  float browLift = 0;      // pixels the brows rise
};

// Fills `p` with the mood's own motion at `now` (blink, glance, bob, bounce, jitter, breathing).
void moodMotion(Mood m, uint32_t now, Pose& p);

void drawFace(TFT_eSprite& s, Mood m, const Pose& p, uint32_t now);
