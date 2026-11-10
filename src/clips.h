// Short animations that take over the face for a few seconds: poke reactions now, idle quirks later.
#pragma once

#include <TFT_eSPI.h>

#include "face.h"
#include "mood.h"
#include "scheduler.h"

enum ClipId : uint8_t {
  CLIP_NONE,
  CLIP_STARTLE,
  CLIP_GIGGLE,
  CLIP_HIDE,
  CLIP_QUICK_GIGGLE,
  CLIP_WHISTLE,
  CLIP_SNACK,
  CLIP_COFFEE,
  CLIP_YAWN,
  CLIP_SNEEZE,
  CLIP_PEEK,
  CLIP_SNORE,
};

// Draws the whole face for clip `id`, `t` ms after it started, in mood `m`'s palette. `p` is the mood's own
// motion for this frame and `now` the clock, for parts that keep animating. Returns false once the clip has
// finished; nothing is drawn then, so the caller draws the normal face.
bool drawClip(TFT_eSprite& s, ClipId id, Mood m, uint32_t t, uint32_t now, Pose p);

// The clip that plays a scheduled quirk.
ClipId clipForQuirk(Quirk q);

// Pops a snore bubble at the start of the startle (the poke woke him mid-snore).
void clipSnorePopped(bool popped);

// What clipSnorePopped() last set, so the owl's startle can burst its own bubble.
bool clipSnoreBurst();
