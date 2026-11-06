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
#include "poke.h"
#include "protocol.h"
#include "ring.h"
#include "touch.h"

const uint32_t FRAME_MS = 33;
const char* const HELP = "commands: idle happy thinking sleepy alert error next | usage <0-100> | usage none";

Mood mood = M_IDLE;
int usagePct = -1;     // 5-hour usage for the ring; -1 hides it
bool fullPush = true;  // next frame must push the whole screen (mood colour or ring changed)
uint32_t lastFrame = 0;

// Sleepy dims the backlight: navy at full brightness glows and washes out the features.
const uint8_t BACKLIGHT_FULL = 255, BACKLIGHT_SLEEPY = 70;

// A clip (poke reaction or quirk) takes over the face for a few seconds. Any mood change cuts it off,
// so a cue from the helper always shows straight away.
ClipId clip = CLIP_NONE;
uint32_t clipStart = 0;
bool clipIsReaction = false;
PokeDetector pokes;

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

void startClip(ClipId id, bool reaction) {
  clip = id;
  clipStart = millis();
  clipIsReaction = reaction;
}

void endClip() { clip = CLIP_NONE; }

void setMood(Mood m) {
  endClip();
  mood = m;
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
      startClip(CLIP_STARTLE, true);
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

void handleButton(uint32_t now) {
  static bool last = true;
  static uint32_t lastPress = 0;
  bool cur = digitalRead(0);
  if (last && !cur && now - lastPress > 250) {
    lastPress = now;
    host = HostWatch(120000);
    setMood((Mood)((mood + 1) % MOOD_COUNT));
  }
  last = cur;
}

// For the first 30 s after boot, report how long frames take, to check the ~30 fps budget.
void reportFrameTime(uint32_t now, uint32_t drawUs, uint32_t pushUs) {
  static uint32_t draw = 0, push = 0, frames = 0, lastReport = 0;
  if (now > 30000) return;
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

  randomSeed(esp_random());
  setMood(M_IDLE);
  Serial.println(HELP);
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
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  renderFrame(now);
  backlightUpdate();
}
