#include "fade.h"

uint8_t fadeToward(uint8_t current, uint8_t target, uint8_t step) {
  if (current < target) return (target - current <= step) ? target : current + step;
  if (current > target) return (current - target <= step) ? target : current - step;
  return current;
}
