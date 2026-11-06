#include "palette.h"

// Colours from the approved mockups (look-so-far.html), as 0xRRGGBB.
static const struct {
  uint32_t bg, ink;
} MOOD_COLOURS[MOOD_COUNT] = {
    {0xa8e6cf, 0x1f3a33},  // idle: mint
    {0xffd34d, 0x2b1d0e},  // happy: yellow
    {0xc3b5f5, 0x251a45},  // thinking: lavender
    {0x2c3557, 0x7d89b8},  // sleepy: navy, dimmed ink
    {0xff8f3d, 0x2b1406},  // alert (needs you): orange
    {0x8fb8e8, 0x1c2a44},  // error (playful sad): blue
};

static const uint32_t WHITE = 0xffffff;
static const uint32_t BLUSH = 0xff9e9e;      // drawn at 70% like the mockup's opacity .7
static const uint32_t CHEEK = 0xff7a59;
static const uint32_t TONGUE = 0xff6b8a;
static const uint32_t ALERT_MARK = 0xff6a00;
static const uint32_t SLEEPY_Z = 0x4f9e86;
// Quirk props, from idle-vignettes.html.
static const uint32_t COOKIE = 0xd9a35b, CHIP = 0x5a3415, COFFEE = 0x6b3e1f;
static const uint32_t SNORE_FILL = 0x9fc3ff, SNORE_EDGE = 0xb9d3ff;  // fill drawn at 35% like the mockup

static uint16_t rgb565(uint32_t c) {
  return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

// `fg` over `bg` at `alpha` (0-1), per channel.
static uint32_t blend(uint32_t fg, uint32_t bg, float alpha) {
  uint32_t out = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    float f = (fg >> shift) & 0xff, b = (bg >> shift) & 0xff;
    out |= (uint32_t)(b + (f - b) * alpha + 0.5f) << shift;
  }
  return out;
}

// One palette from a background and ink; `sleepy` picks the snore props and the green Z.
static void build(uint32_t bg, uint32_t ink, bool sleepy, uint16_t* p) {
  for (int j = 0; j < PAL_COUNT; j++) p[j] = rgb565(ink);
  p[P_BG] = rgb565(bg);
  p[P_INK] = rgb565(ink);
  p[P_WHITE] = rgb565(WHITE);
  p[P_BLUSH] = rgb565(blend(BLUSH, bg, 0.7f));
  p[P_CHEEK] = rgb565(CHEEK);
  p[P_TONGUE] = rgb565(TONGUE);
  p[P_ACCENT] = rgb565(sleepy ? SLEEPY_Z : ALERT_MARK);
  p[P_TRACK] = rgb565(blend(ink, bg, 0.18f));
  p[P_BUBBLE] = rgb565(blend(WHITE, bg, 0.85f));
  p[P_DIM] = rgb565(blend(ink, blend(WHITE, bg, 0.85f), 0.3f));
  if (sleepy) {
    p[P_SNORE_FILL] = rgb565(blend(SNORE_FILL, bg, 0.35f));
    p[P_SNORE_EDGE] = rgb565(SNORE_EDGE);
  } else {
    p[P_COOKIE] = rgb565(COOKIE);
    p[P_CHIP] = rgb565(CHIP);
    p[P_COFFEE] = rgb565(COFFEE);
  }
}

const uint16_t* paletteFor(Mood m) {
  static uint16_t palettes[MOOD_COUNT][PAL_COUNT];
  static bool built = false;
  if (!built) {
    for (int i = 0; i < MOOD_COUNT; i++) build(MOOD_COLOURS[i].bg, MOOD_COLOURS[i].ink, i == M_SLEEPY, palettes[i]);
    built = true;
  }
  return palettes[m];
}

const uint16_t* startlePalette() {
  static uint16_t p[PAL_COUNT];
  static bool built = false;
  if (!built) {
    build(0xffb3c7, 0x3d1426, false, p);  // pastel pink, plum ink: a jolt that stays in the family
    p[P_BLUSH] = rgb565(0xe8607f);  // the usual blush vanishes into pink, so a deeper rose
    built = true;
  }
  return p;
}
