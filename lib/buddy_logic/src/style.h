// Face styles: which set of faces the board draws, picked at boot to suit the enclosure.
#pragma once

#include <stdint.h>

enum Style : uint8_t { S_OWL, S_CLASSIC, STYLE_COUNT };

extern const char* const STYLE_NAMES[STYLE_COUNT];

// The style stored in flash. Anything that is not a valid style (nothing saved yet, a corrupt or
// newer value) is the owl.
Style styleFromSaved(int32_t raw);

inline Style otherStyle(Style s) { return s == S_OWL ? S_CLASSIC : S_OWL; }
