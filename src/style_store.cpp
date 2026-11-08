#include "style_store.h"

#include <Preferences.h>

static const char* const NAMESPACE = "buddy";
static const char* const KEY = "style";

Style loadStyle() {
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, true)) return styleFromSaved(-1);  // namespace not created yet: first boot
  int32_t raw = prefs.getInt(KEY, -1);
  prefs.end();
  return styleFromSaved(raw);
}

void saveStyle(Style s) {
  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false)) return;
  if (prefs.getInt(KEY, -1) != s) prefs.putInt(KEY, s);
  prefs.end();
}
