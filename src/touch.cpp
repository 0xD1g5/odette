#include "touch.h"

#include <Arduino.h>
#include <SPI.h>

static const int PIN_CLK = 25, PIN_MISO = 39, PIN_MOSI = 32, PIN_CS = 33, PIN_IRQ = 36;

// Measured on the board: pressure reads 3-15 untouched and 1,600-3,400 pressed.
static const int PRESSED_Z = 400;

static SPIClass touchSpi(VSPI);

void touchBegin() {
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_IRQ, INPUT);  // input-only pin; the controller pulls it low while pressed
  touchSpi.begin(PIN_CLK, PIN_MISO, PIN_MOSI, PIN_CS);
}

// Pressure from the two Z channels. Ends in power-down with the pen interrupt enabled, so IRQ keeps working.
static int readZ() {
  touchSpi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_CS, LOW);
  touchSpi.transfer(0xB1);
  int z1 = touchSpi.transfer16(0xC1) >> 3;
  int z2 = touchSpi.transfer16(0xD0) >> 3;
  touchSpi.transfer16(0);
  digitalWrite(PIN_CS, HIGH);
  touchSpi.endTransaction();
  return z1 + 4095 - z2;
}

bool touchPressed() {
  if (digitalRead(PIN_IRQ) == HIGH) return false;  // cheap check; it matched pressure on every sample
  return readZ() > PRESSED_Z;
}
