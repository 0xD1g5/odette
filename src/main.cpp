// Desk Buddy, stage 2
// A quirky, cheerful face on the 2.8" ESP32-32E display (320x240 landscape).
// Moods: idle, happy, thinking, sleepy, alert (needs you), error (playful sad).
// Control: press BOOT to cycle moods, or send lines over USB serial at 115200 (see lib/buddy_logic/src/protocol.h).

#include <Arduino.h>

#include "backlight.h"
#include "canvas.h"
#include "clips.h"
#include "face.h"
#include "host.h"
#include "palette.h"
#include "picker.h"
#include "picker_screen.h"
#include "poke.h"
#include "protocol.h"
#include "ring.h"
#include "style_store.h"
#include "touch.h"

const uint32_t FRAME_MS = 33;
const char* const HELP =
    "commands: idle happy thinking sleepy alert error next | usage <0-100> | usage none | quirk <whistle snack coffee "
    "yawn sneeze peek snore>";

Mood mood = M_IDLE;
int usagePct = -1;     // 5-hour usage for the ring; -1 hides it
bool fullPush = true;  // next frame must push the whole screen (mood colour or ring changed)
uint32_t lastFrame = 0;
uint32_t faceStart = 0;  // when the picker handed over to the face
Style style = S_OWL;  // picked at boot, fixed until the next boot

// Sleepy dims the backlight: navy at full brightness glows and washes out the features.
const uint8_t BACKLIGHT_FULL = 255, BACKLIGHT_SLEEPY = 70;

// A clip (poke reaction or quirk) takes over the face for a few seconds. Any mood change cuts it off,
// so a cue from the helper always shows straight away.
ClipId clip = CLIP_NONE;
uint32_t clipStart = 0;
bool clipIsReaction = false;
PokeDetector pokes;
QuirkScheduler quirks(1);  // reseeded in setup()

// The Mac helper resends its state every 30 seconds, so after 2 minutes of silence it is gone and we fall
// back to idle. Pressing BOOT or "next" hands control back to the board until the helper speaks again.
HostWatch host(120000);

void colorTest() {
  TFT_eSPI& tft = display();
  const uint16_t cols[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
  const char* names[] = {"RED", "GREEN", "BLUE", "WHITE"};
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < 4; i++) {
    tft.fillScreen(cols[i]);
    tft.setTextColor(TFT_BLACK, cols[i]);
    tft.drawString(names[i], tft.width() / 2, tft.height() / 2, 4);
    delay(700);
  }
  tft.setTextDatum(TL_DATUM);
}

void endClip() {
  if (clip != CLIP_NONE && !clipIsReaction) quirks.finished(millis());  // schedule the next quirk
  if (clip == CLIP_STARTLE) {  // back from the startle's pink to the mood's colours
    canvasSetMood(mood);
    fullPush = true;
  }
  clip = CLIP_NONE;
}

void startClip(ClipId id, bool reaction) {
  endClip();  // a poke cutting off a quirk must still let the scheduler know it ended
  clip = id;
  clipStart = millis();
  clipIsReaction = reaction;
}

void setMood(Mood m) {
  endClip();
  mood = m;
  quirks.setMood(m, millis());
  canvasSetMood(m);
  fullPush = true;
  backlightTarget(m == M_SLEEPY ? BACKLIGHT_SLEEPY : BACKLIGHT_FULL);
  Serial.printf("state: %s\n", MOOD_NAMES[m]);
}

void applyCommand(const char* line) {
  Command cmd = parseLine(line);
  switch (cmd.type) {
    case CMD_NONE:
      return;
    case CMD_MOOD:
      host.heard(millis());
      if (mood != cmd.mood) setMood(cmd.mood);
      return;
    case CMD_NEXT:
      host = HostWatch(120000);
      setMood((Mood)((mood + 1) % MOOD_COUNT));
      return;
    case CMD_QUIRK:
      startClip(clipForQuirk(cmd.quirk), false);
      return;
    case CMD_USAGE:
      host.heard(millis());
      if (usagePct != cmd.usage) {
        usagePct = cmd.usage;
        fullPush = true;
      }
      return;
    default:
      Serial.println(HELP);
  }
}

void handleSerial() {
  static char line[48];
  static int len = 0;
  static bool overflow = false;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[len] = '\0';
      if (!overflow) applyCommand(line);
      len = 0;
      overflow = false;
    } else if (len < (int)sizeof(line) - 1) {
      line[len++] = c;
    } else {
      overflow = true;  // drop the whole line rather than act on a truncated one
    }
  }
}

// A poke is any press. The helper hears "poke" and counts it as activity, so a woken face stays awake.
void handleTouch(uint32_t now) {
  static uint32_t lastPoll = 0;
  if (now - lastPoll < 10) return;
  lastPoll = now;
  if (!pokes.update(touchPressed(), now)) return;
  Serial.println("poke");
  switch (chooseReaction(mood, clip != CLIP_NONE && clipIsReaction, esp_random())) {
    case R_STARTLE:
      clipSnorePopped(clip == CLIP_SNORE);
      setMood(M_IDLE);  // wake now (backlight up); the helper confirms idle once it sees the poke
      backlightSnap(BACKLIGHT_FULL);  // full brightness at once, so the jolt is seen rather than faded into
      startClip(CLIP_STARTLE, true);
      canvasSetPalette(startlePalette());  // setMood already asked for a full push
      break;
    case R_GIGGLE:
      startClip(CLIP_GIGGLE, true);
      break;
    case R_HIDE:
      startClip(CLIP_HIDE, true);
      break;
    case R_QUICK_GIGGLE:
      startClip(CLIP_QUICK_GIGGLE, true);
      break;
    case R_NONE:
      break;  // a reaction is already playing: extra pokes are ignored
  }
}

// True once per press of BOOT (falling edge, 250 ms debounce).
bool bootPressed(uint32_t now) {
  static bool last = true;
  static uint32_t lastPress = 0;
  bool cur = digitalRead(0);
  bool pressed = last && !cur && now - lastPress > 250;
  if (pressed) lastPress = now;
  last = cur;
  return pressed;
}

void handleButton(uint32_t now) {
  if (!bootPressed(now)) return;
  host = HostWatch(120000);
  setMood((Mood)((mood + 1) % MOOD_COUNT));
}

// The boot picker: the saved style starts after 30 s, a tap or BOOT flips the choice. Anything the helper
// sends meanwhile is thrown away; it resends every 30 s, so the face catches up soon after.
Style runPicker() {
  StylePicker picker(loadStyle(), millis());
  PokeDetector taps;
  canvasSetMood(M_IDLE);
  bool first = true;
  uint32_t lastDraw = 0;
  while (true) {
    uint32_t now = millis();
    if (picker.done(now)) break;
    if (taps.update(touchPressed(), now) || bootPressed(now)) picker.tap(now);
    while (Serial.available()) Serial.read();
    if (first || now - lastDraw >= FRAME_MS) {
      lastDraw = now;
      TFT_eSprite& s = canvas();
      s.fillSprite(P_BG);
      drawPicker(s, picker, now);
      canvasPush(first);
      first = false;
    }
    delay(1);
  }
  saveStyle(picker.highlighted());
  Serial.printf("style: %s\n", STYLE_NAMES[picker.highlighted()]);
  return picker.highlighted();
}

// For the first 30 s after the face starts, report how long frames take, to check the ~30 fps budget.
void reportFrameTime(uint32_t now, uint32_t drawUs, uint32_t pushUs) {
  static uint32_t draw = 0, push = 0, frames = 0, lastReport = 0;
  if (now - faceStart > 30000) return;
  draw += drawUs;
  push += pushUs;
  frames++;
  if (now - lastReport >= 5000) {
    Serial.printf("frame %.1f ms avg (draw %.1f, push %.1f) over %u frames\n", (draw + push) / 1000.0f / frames,
                  draw / 1000.0f / frames, push / 1000.0f / frames, frames);
    draw = push = frames = 0;
    lastReport = now;
  }
}

void renderFrame(uint32_t now) {
  uint32_t t0 = micros();
  Pose pose;
  moodMotion(mood, now, pose);
  TFT_eSprite& s = canvas();
  s.fillSprite(P_BG);
  bool clipDrawn = clip != CLIP_NONE && drawClip(s, clip, mood, now - clipStart, now, pose);
  if (clip != CLIP_NONE && !clipDrawn) endClip();
  if (!clipDrawn) drawFace(s, mood, pose, now);
  drawRing(s, usagePct);
  uint32_t t1 = micros();
  canvasPush(fullPush);
  fullPush = false;
  reportFrameTime(now, t1 - t0, micros() - t1);
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  delay(200);
  Serial.println("\nDesk Buddy stage 2");

  if (!canvasBegin()) {
    TFT_eSPI& tft = display();
    Serial.println("Sprite allocation failed");
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("sprite alloc failed", 10, 100, 4);
    while (true) delay(1000);
  }
  backlightBegin();
  touchBegin();

#ifdef COLOR_TEST
  Serial.println("Color test: expect red, green, blue, white");
  colorTest();
#endif

  style = runPicker();
  randomSeed(esp_random());
  quirks = QuirkScheduler(esp_random());
  setMood(M_IDLE);
  Serial.println(HELP);
  faceStart = millis();
}

void loop() {
  uint32_t now = millis();
  handleButton(now);
  handleTouch(now);
  handleSerial();
  if (host.expired(now)) {  // helper gone: idle, and hide the ring since its number is now stale
    usagePct = -1;
    fullPush = true;
    if (mood != M_IDLE) setMood(M_IDLE);
  }
  if (clip == CLIP_NONE) {
    Quirk q = quirks.update(now);
    if (q != Q_NONE) {
      Serial.printf("quirk %d\n", q);  // for checking timing; the helper ignores it
      startClip(clipForQuirk(q), false);
    }
  }
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  renderFrame(now);
  backlightUpdate();
}
