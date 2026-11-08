#include "style.h"

const char* const STYLE_NAMES[STYLE_COUNT] = {"owl", "classic"};

Style styleFromSaved(int32_t raw) { return raw >= 0 && raw < STYLE_COUNT ? (Style)raw : S_OWL; }
