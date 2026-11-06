// Desk Buddy, stage 2
// A quirky, cheerful face on the 2.8" ESP32-32E display (320x240 landscape).
// Moods: idle, happy, thinking, sleepy, alert (needs you), error (playful sad).
// Control: press BOOT to cycle moods, or send lines over USB serial at 115200 (see lib/buddy_logic/src/protocol.h).

#include <Arduino.h>

#include "canvas.h"
#include "face.h"
#include "palette.h"
#include "protocol.h"

const uint32_t FRAME_MS = 33;
const char* const HELP = "commands: idle happy thinking sleepy alert error next | usage <0-100> | usage none";

Mood mood = M_IDLE;
bool fullPush = true;  // next frame must push the whole screen (mood colour changed)
uint32_t lastFrame = 0;

// A mood set over serial counts as host driven. The Mac helper resends its state every 30 seconds,
// so if nothing arrives for HOST_TIMEOUT_MS the helper is gone and we fall back to idle.
bool hostDriven = false;
uint32_t lastHostMs = 0;
const uint32_t HOST_TIMEOUT_MS = 120000;

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

void setMood(Mood m) {
  mood = m;
  canvasSetMood(m);
  fullPush = true;
  Serial.printf("state: %s\n", MOOD_NAMES[m]);
}

void applyCommand(const char* line) {
  Command cmd = parseLine(line);
  switch (cmd.type) {
    case CMD_NONE:
      return;
    case CMD_MOOD:
      hostDriven = true;
      lastHostMs = millis();
      if (mood != cmd.mood) setMood(cmd.mood);
      return;
    case CMD_NEXT:
      hostDriven = false;
      setMood((Mood)((mood + 1) % MOOD_COUNT));
      return;
    case CMD_USAGE:
      lastHostMs = millis();
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

void handleButton(uint32_t now) {
  static bool last = true;
  static uint32_t lastPress = 0;
  bool cur = digitalRead(0);
  if (last && !cur && now - lastPress > 250) {
    lastPress = now;
    hostDriven = false;
    setMood((Mood)((mood + 1) % MOOD_COUNT));
  }
  last = cur;
}

// For the first 30 s after boot, report how long frames take, to check the ~30 fps budget.
void reportFrameTime(uint32_t now, uint32_t workMs) {
  static uint32_t total = 0, frames = 0, lastReport = 0;
  if (now > 30000) return;
  total += workMs;
  frames++;
  if (now - lastReport >= 5000) {
    Serial.printf("frame %.1f ms avg over %u frames\n", (float)total / frames, frames);
    total = frames = 0;
    lastReport = now;
  }
}

void renderFrame(uint32_t now) {
  Pose pose;
  moodMotion(mood, now, pose);
  TFT_eSprite& s = canvas();
  s.fillSprite(P_BG);
  drawFace(s, mood, pose, now);
  canvasPush(fullPush ? FULL_RECT : faceBounds(mood, pose));
  fullPush = false;
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
  handleSerial();
  if (hostDriven && mood != M_IDLE && now - lastHostMs > HOST_TIMEOUT_MS) {
    hostDriven = false;
    setMood(M_IDLE);
  }
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  renderFrame(now);
  reportFrameTime(now, millis() - now);
}
