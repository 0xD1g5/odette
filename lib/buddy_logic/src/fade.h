// Easing for the backlight: one step per frame towards a target brightness.
#pragma once

#include <stdint.h>

uint8_t fadeToward(uint8_t current, uint8_t target, uint8_t step);
