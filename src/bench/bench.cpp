// Desk Buddy on-device check: frame timing for a full-screen 4-bit palette sprite, and the touch panel.
// Build and flash with: pio run -e bench -t upload. Results print over serial at 115200.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

// Touch (XPT2046) has its own SPI pins: CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36.
SPIClass touchSpi(VSPI);
const int TOUCH_CS = 33, TOUCH_IRQ = 36;

// Pressure from the two Z channels; near 0 when nothing presses. Ends in power-down with PENIRQ enabled.
int readTouchZ() {
  touchSpi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TOUCH_CS, LOW);
  touchSpi.transfer(0xB1);
  int z1 = touchSpi.transfer16(0xC1) >> 3;
  int z2 = touchSpi.transfer16(0xD0) >> 3;
  touchSpi.transfer16(0);
  digitalWrite(TOUCH_CS, HIGH);
  touchSpi.endTransaction();
  return z1 + 4095 - z2;
}

uint16_t pal[16];

// Rough idle face, enough shapes to cost what the real face will.
void drawFace(int dx, int dy) {
  spr.fillSprite(0);
  spr.fillRoundRect(98 + dx, 78 + dy, 36, 66, 18, 1);
  spr.fillRoundRect(186 + dx, 78 + dy, 36, 66, 18, 1);
  spr.fillCircle(110 + dx, 94 + dy, 6, 2);
  spr.fillCircle(198 + dx, 94 + dy, 6, 2);
  spr.fillRoundRect(92 + dx, 58 + dy, 46, 9, 4, 1);
  spr.fillRoundRect(182 + dx, 58 + dy, 46, 9, 4, 1);
  spr.fillEllipse(84 + dx, 160 + dy, 16, 9, 3);
  spr.fillEllipse(236 + dx, 160 + dy, 16, 9, 3);
  spr.fillCircle(160 + dx, 158 + dy, 16, 1);
  spr.fillCircle(160 + dx, 158 + dy, 11, 0);
  spr.fillRect(140 + dx, 140 + dy, 41, 18, 0);
}

enum Mode { FULL, BOX, BOUNCE, RENDER_ONLY };

float timeFrames(Mode mode, const char* name) {
  const int frames = 90;
  uint32_t start = millis();
  for (int i = 0; i < frames; i++) {
    int phase = i % 20;
    int dx = (mode == BOUNCE) ? 0 : (phase < 10 ? phase : 20 - phase) * 4 - 20;
    int dy = (mode == BOUNCE) ? (phase < 10 ? phase : 20 - phase) * 2 - 10 : 0;
    drawFace(dx, dy);
    if (mode == FULL || mode == BOUNCE) spr.pushSprite(0, 0);
    if (mode == BOX) spr.pushSprite(50, 40, 50, 40, 220, 150);
  }
  float ms = (millis() - start) / (float)frames;
  Serial.printf("bench %s %.1f ms/frame (%.1f fps)\n", name, ms, 1000.0f / ms);
  return ms;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nDesk Buddy bench");
  tft.init();
  tft.setRotation(1);

  pal[0] = tft.color565(0xa8, 0xe6, 0xcf);  // idle background
  pal[1] = tft.color565(0x1f, 0x3a, 0x33);  // idle ink
  pal[2] = TFT_WHITE;
  pal[3] = tft.color565(0xff, 0x9e, 0x9e);  // blush
  for (int i = 4; i < 16; i++) pal[i] = TFT_BLACK;

  spr.setColorDepth(4);
  if (!spr.createSprite(320, 240)) {
    Serial.printf("bench alloc FAILED max_alloc=%u\n", ESP.getMaxAllocHeap());
    while (true) delay(1000);
  }
  spr.createPalette(pal, 16);
  Serial.printf("bench alloc ok free=%u max_alloc=%u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  timeFrames(RENDER_ONLY, "render_only");
  timeFrames(FULL, "full");
  timeFrames(BOX, "box");
  timeFrames(BOUNCE, "bounce");

  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  touchSpi.begin(25, 39, 32, TOUCH_CS);
  pinMode(TOUCH_IRQ, INPUT);
  Serial.println("bench touch: press the screen now");
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 100) return;
  last = millis();
  int irq = digitalRead(TOUCH_IRQ);  // read before SPI, which disturbs PENIRQ while converting
  Serial.printf("touch z=%d irq=%d\n", readTouchZ(), irq);
  // Keep the last face on screen so the owner can see the bench ran.
}
