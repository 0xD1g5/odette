// Desk Buddy, stage 1
// Round-eye face on the 2.8" ESP32-32E display (240x320 panel, used in landscape).
// States: idle, happy, thinking, sleepy, alert, error.
// Control: press the BOOT button to cycle states, or send a state name over USB serial (115200).

#include <Arduino.h>
#include <TFT_eSPI.h>

#ifndef ROTATION
#define ROTATION 1
#endif

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite eyes = TFT_eSprite(&tft);

enum State { S_IDLE, S_HAPPY, S_THINKING, S_SLEEPY, S_ALERT, S_ERROR, STATE_COUNT };
const char* STATE_NAMES[STATE_COUNT] = {"idle", "happy", "thinking", "sleepy", "alert", "error"};

// Eye sprite covers the eye area; sprite coordinates are screen coordinates minus (SPR_X, SPR_Y).
const int SPR_X = 40, SPR_Y = 50, SPR_W = 240, SPR_H = 120;
const int EYE_R = 42;
const int EYE_LX = 60, EYE_RX = 180, EYE_CY = 60;
const uint32_t FRAME_MS = 33;

uint16_t C_BG, C_EYE, C_PUPIL, C_CHEEK, C_ERR, C_AMBER, C_ACCENT, C_DOT_ON, C_DOT_OFF, C_MOUTH;

State state = S_IDLE;
uint32_t lastFrame = 0;
uint32_t nextBlink = 3000, blinkStart = 0;
uint32_t nextGlance = 0;
float gx = 0, gy = 0, tgx = 0, tgy = 0;
int lastDot = -1;

const uint32_t BLINK_MS = 180;

// True once now has reached t. Safe across the millis() wrap at ~49.7 days, unlike now >= t.
bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

void colorTest() {
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

void mouthSmile(int cx, int cy, int r, int th, uint16_t col) {
  tft.fillCircle(cx, cy, r, col);
  tft.fillCircle(cx, cy, r - th, C_BG);
  tft.fillRect(cx - r - 1, cy - r - 1, 2 * r + 3, r + 1, C_BG);
}

void mouthO(int cx, int cy, int r, int th, uint16_t col) {
  tft.fillCircle(cx, cy, r, col);
  tft.fillCircle(cx, cy, r - th, C_BG);
}

void mouthWavy(int cx, int cy, uint16_t col) {
  for (int i = -30; i <= 30; i += 3) {
    int y = cy + (int)(4.0f * sinf(i * 0.35f));
    tft.fillCircle(cx + i, y, 2, col);
  }
}

// Everything that does not animate every frame is drawn once per state change.
void drawStatic() {
  tft.fillScreen(C_BG);
  switch (state) {
    case S_IDLE:
      mouthSmile(160, 186, 16, 4, C_MOUTH);
      break;
    case S_HAPPY:
      mouthSmile(160, 176, 30, 5, C_MOUTH);
      break;
    case S_THINKING:
      break;  // dots are animated in renderAux
    case S_SLEEPY:
      mouthO(160, 198, 7, 3, C_MOUTH);
      tft.setTextColor(C_ACCENT, C_BG);
      tft.setTextSize(1);
      tft.drawString("Z", 252, 20, 4);
      tft.drawString("z", 232, 38, 2);
      break;
    case S_ALERT:
      mouthO(160, 200, 12, 4, C_MOUTH);
      tft.setTextColor(C_AMBER, C_BG);
      tft.setTextSize(2);
      tft.drawString("!", 268, 2, 4);
      tft.setTextSize(1);
      break;
    case S_ERROR:
      mouthWavy(160, 200, C_ERR);
      break;
    default:
      break;
  }
}

void setState(State s) {
  state = s;
  lastDot = -1;
  blinkStart = 0;
  nextBlink = millis() + 1500;
  if (s != S_IDLE && s != S_THINKING) { gx = gy = tgx = tgy = 0; }
  drawStatic();
  Serial.printf("state: %s\n", STATE_NAMES[s]);
}

float blinkOpen(uint32_t now) {
  if (blinkStart == 0) {
    if (!reached(now, nextBlink)) return 1.0f;
    blinkStart = now;
  }
  uint32_t t = now - blinkStart;
  if (t >= BLINK_MS) {
    blinkStart = 0;
    nextBlink = now + random(2200, 6000);
    return 1.0f;
  }
  float p = (float)t / BLINK_MS;
  return p < 0.45f ? 1.0f - (p / 0.45f) * 0.92f : 0.08f + ((p - 0.45f) / 0.55f) * 0.92f;
}

void updateGaze(uint32_t now, bool thinking) {
  if (thinking) {
    tgx = 0.75f;
    tgy = -0.7f;
  } else if (reached(now, nextGlance)) {
    tgx = random(-80, 81) / 100.0f;
    tgy = random(-40, 41) / 100.0f;
    if (random(4) == 0) { tgx = 0; tgy = 0; }
    nextGlance = now + random(1200, 3800);
  }
  gx += (tgx - gx) * 0.25f;
  gy += (tgy - gy) * 0.25f;
}

void drawRoundEye(int cx, int cy, float open, float px_, float py_, int pr) {
  int ry = max(2, (int)(EYE_R * open));
  eyes.fillEllipse(cx, cy, EYE_R, ry, C_EYE);
  int pry = min(pr, ry - 1);
  if (pry < 2) return;
  int px = cx + (int)(px_ * (EYE_R - pr - 4));
  int py = cy + (int)(py_ * max(0, ry - pry - 3));
  eyes.fillEllipse(px, py, pr, pry, C_PUPIL);
  if (open > 0.6f) eyes.fillCircle(px - pr / 3, py - pr / 3, max(2, pr / 4), TFT_WHITE);
}

void drawHappyEye(int cx, int cy, bool leftSide) {
  eyes.fillCircle(cx, cy + 14, EYE_R, C_EYE);
  eyes.fillCircle(cx, cy + 14, EYE_R - 10, C_BG);
  eyes.fillRect(cx - EYE_R - 2, cy + 14, 2 * EYE_R + 4, EYE_R + 4, C_BG);
  eyes.fillCircle(cx + (leftSide ? -34 : 34), cy + 40, 9, C_CHEEK);
}

void drawXEye(int cx, int cy) {
  const int d = 28;
  eyes.drawWideLine(cx - d, cy - d, cx + d, cy + d, 9, C_ERR, C_BG);
  eyes.drawWideLine(cx + d, cy - d, cx - d, cy + d, 9, C_ERR, C_BG);
}

void renderEyes(uint32_t now) {
  eyes.fillSprite(C_BG);
  float bob = sinf(now / 650.0f) * 2.0f;
  switch (state) {
    case S_IDLE:
    case S_THINKING: {
      updateGaze(now, state == S_THINKING);
      float open = blinkOpen(now);
      int cy = EYE_CY + (int)bob;
      drawRoundEye(EYE_LX, cy, open, gx, gy, 18);
      drawRoundEye(EYE_RX, cy, open, gx, gy, 18);
      break;
    }
    case S_HAPPY: {
      int cy = EYE_CY + (int)(sinf(now / 260.0f) * 3.0f);
      drawHappyEye(EYE_LX, cy, true);
      drawHappyEye(EYE_RX, cy, false);
      break;
    }
    case S_SLEEPY: {
      float open = 0.10f + 0.04f * (sinf(now / 1100.0f) + 1.0f);
      int cy = EYE_CY + 10 + (int)bob;
      drawRoundEye(EYE_LX, cy, open, 0, 0, 0);
      drawRoundEye(EYE_RX, cy, open, 0, 0, 0);
      break;
    }
    case S_ALERT: {
      int jx = (int)((now / 90) % 3) - 1;
      drawRoundEye(EYE_LX + jx, EYE_CY, 1.12f, 0, 0, 10);
      drawRoundEye(EYE_RX + jx, EYE_CY, 1.12f, 0, 0, 10);
      break;
    }
    case S_ERROR:
      drawXEye(EYE_LX, EYE_CY);
      drawXEye(EYE_RX, EYE_CY);
      break;
    default:
      break;
  }
  eyes.pushSprite(SPR_X, SPR_Y);
}

void renderAux(uint32_t now) {
  if (state != S_THINKING) return;
  int active = (int)((now / 350) % 3);
  if (active == lastDot) return;
  lastDot = active;
  for (int i = 0; i < 3; i++) {
    tft.fillCircle(140 + i * 20, 200, 6, i == active ? C_DOT_ON : C_DOT_OFF);
  }
}

// A state set over serial counts as host driven. The Mac helper resends its state every
// 30 seconds, so if nothing arrives for HOST_TIMEOUT_MS the helper is gone and we fall back to idle.
bool hostDriven = false;
uint32_t lastHostMs = 0;
const uint32_t HOST_TIMEOUT_MS = 120000;

void applyCommand(const String& cmd) {
  for (int i = 0; i < STATE_COUNT; i++) {
    if (cmd == STATE_NAMES[i]) {
      hostDriven = true;
      lastHostMs = millis();
      if (state != (State)i) setState((State)i);
      return;
    }
  }
  if (cmd == "next") { hostDriven = false; setState((State)((state + 1) % STATE_COUNT)); return; }
  Serial.println("commands: idle happy thinking sleepy alert error next");
}

void handleSerial() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      line.toLowerCase();
      if (line.length()) applyCommand(line);
      line = "";
    } else if (line.length() < 32) {
      line += c;
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
    setState((State)((state + 1) % STATE_COUNT));
  }
  last = cur;
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);
  delay(200);
  Serial.println("\nDesk Buddy stage 1");

  tft.init();
  tft.setRotation(ROTATION);

  C_BG = TFT_BLACK;
  C_EYE = tft.color565(241, 239, 232);
  C_PUPIL = tft.color565(24, 52, 60);
  C_CHEEK = tft.color565(237, 147, 177);
  C_ERR = tft.color565(226, 75, 74);
  C_AMBER = tft.color565(239, 159, 39);
  C_ACCENT = tft.color565(93, 202, 165);
  C_DOT_ON = tft.color565(241, 239, 232);
  C_DOT_OFF = tft.color565(70, 72, 78);
  C_MOUTH = tft.color565(241, 239, 232);

#ifdef COLOR_TEST
  Serial.println("Color test: expect red, green, blue, white");
  colorTest();
#endif

  eyes.setColorDepth(16);
  if (!eyes.createSprite(SPR_W, SPR_H)) {
    Serial.println("Sprite allocation failed");
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("sprite alloc failed", 10, 100, 4);
    while (true) delay(1000);
  }

  randomSeed(esp_random());
  nextGlance = millis() + 1500;
  setState(S_IDLE);
  Serial.println("BOOT button or serial: idle happy thinking sleepy alert error next");
}

void loop() {
  uint32_t now = millis();
  handleButton(now);
  handleSerial();
  if (hostDriven && state != S_IDLE && now - lastHostMs > HOST_TIMEOUT_MS) {
    hostDriven = false;
    setState(S_IDLE);
  }
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  renderEyes(now);
  renderAux(now);
}
