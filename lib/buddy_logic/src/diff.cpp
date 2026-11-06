#include "diff.h"

#include <string.h>

int diffFrames(uint8_t* prev, const uint8_t* cur, int width, int height, int bandRows, Span* out, int maxOut) {
  const int rowBytes = width / 2;
  int count = 0;
  for (int y0 = 0; y0 < height; y0 += bandRows) {
    int rows = height - y0 < bandRows ? height - y0 : bandRows;
    int lo = rowBytes, hi = -1;  // changed byte columns in this band
    for (int y = y0; y < y0 + rows; y++) {
      const uint8_t* c = cur + y * rowBytes;
      uint8_t* p = prev + y * rowBytes;
      if (memcmp(c, p, rowBytes) == 0) continue;
      int a = 0, b = rowBytes - 1;
      while (c[a] == p[a]) a++;
      while (c[b] == p[b]) b--;
      if (a < lo) lo = a;
      if (b > hi) hi = b;
      memcpy(p + a, c + a, b - a + 1);
    }
    if (hi < 0) continue;
    if (count < maxOut) {
      out[count] = {(uint16_t)(lo * 2), (uint16_t)y0, (uint16_t)((hi - lo + 1) * 2), (uint16_t)rows};
    }
    count++;
  }
  return count < maxOut ? count : maxOut;
}
