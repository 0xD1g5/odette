#include "protocol.h"

#include <ctype.h>
#include <string.h>

static const int MAX_LINE = 40;

static const struct {
  const char* name;
  Quirk quirk;
} QUIRK_NAMES[] = {
    {"whistle", Q_WHISTLE}, {"snack", Q_SNACK}, {"coffee", Q_COFFEE}, {"yawn", Q_YAWN},
    {"sneeze", Q_SNEEZE},   {"peek", Q_PEEK},   {"snore", Q_SNORE},
};

// Parses an optional '-' and at least one digit, nothing else. Clamps to 0-100.
static bool parsePercent(const char* s, int* out) {
  bool negative = false;
  if (*s == '-') {
    negative = true;
    s++;
  }
  if (!isdigit((unsigned char)*s)) return false;
  int value = 0;
  for (; *s; s++) {
    if (!isdigit((unsigned char)*s)) return false;
    if (value <= 100) value = value * 10 + (*s - '0');
  }
  if (negative) value = 0;
  *out = value > 100 ? 100 : value;
  return true;
}

Command parseLine(const char* line) {
  Command cmd = {CMD_UNKNOWN, M_IDLE, -1, Q_NONE};
  while (*line == ' ' || *line == '\r' || *line == '\t') line++;
  char buf[MAX_LINE];
  int len = 0;
  for (; line[len]; len++) {
    if (len >= MAX_LINE - 1) return cmd;
    buf[len] = (char)tolower((unsigned char)line[len]);
  }
  while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\r' || buf[len - 1] == '\t')) len--;
  buf[len] = '\0';

  if (len == 0) {
    cmd.type = CMD_NONE;
    return cmd;
  }
  int mood = moodFromName(buf);
  if (mood >= 0) {
    cmd.type = CMD_MOOD;
    cmd.mood = (Mood)mood;
    return cmd;
  }
  if (strcmp(buf, "next") == 0) {
    cmd.type = CMD_NEXT;
    return cmd;
  }
  if (strncmp(buf, "quirk ", 6) == 0) {
    const char* arg = buf + 6;
    while (*arg == ' ') arg++;
    for (const auto& q : QUIRK_NAMES) {
      if (strcmp(arg, q.name) == 0) {
        cmd.type = CMD_QUIRK;
        cmd.quirk = q.quirk;
      }
    }
    return cmd;
  }
  if (strncmp(buf, "usage ", 6) == 0) {
    const char* arg = buf + 6;
    while (*arg == ' ') arg++;
    if (strcmp(arg, "none") == 0) {
      cmd.type = CMD_USAGE;
      cmd.usage = -1;
    } else if (parsePercent(arg, &cmd.usage)) {
      cmd.type = CMD_USAGE;
    }
  }
  return cmd;
}
