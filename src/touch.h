// The XPT2046 resistive touch controller, on its own SPI pins (CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36).
// Only "is it pressed" is needed: a poke is any press anywhere, so there is no calibration.
#pragma once

void touchBegin();
bool touchPressed();
