// The picked face style, kept in flash (NVS) so it survives a power cycle.
#pragma once

#include "style.h"

Style loadStyle();
// Writes only when the value changes, to spare the flash.
void saveStyle(Style s);
