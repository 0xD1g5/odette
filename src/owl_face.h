// The owl faces (owl-b-moods.html, owl-b-corners-v2.html): big round eyes, brows and cheeks, no mouth,
// since the printed beak plays that part. Same moods, colours and motion as the Classic faces.
#pragma once

#include <TFT_eSPI.h>

#include "face.h"
#include "mood.h"

void drawOwlFace(TFT_eSprite& s, Mood m, const Pose& p, uint32_t now);
