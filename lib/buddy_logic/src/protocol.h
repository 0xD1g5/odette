// Host -> board serial commands, one per line:
//   <mood name>      show that mood (idle happy thinking sleepy alert error)
//   next             cycle to the next mood (testing)
//   usage <0-100>    5-hour usage for the ring; "usage none" hides it
//   quirk <name>     play one idle quirk now, for checking (whistle snack coffee yawn sneeze peek snore)
#pragma once

#include "mood.h"
#include "scheduler.h"

enum CmdType : uint8_t { CMD_NONE, CMD_MOOD, CMD_NEXT, CMD_USAGE, CMD_QUIRK, CMD_UNKNOWN };

struct Command {
  CmdType type;
  Mood mood;  // CMD_MOOD
  int usage;  // CMD_USAGE: 0-100, or -1 to hide the ring
  Quirk quirk;  // CMD_QUIRK
};

// Trims spaces and CR, ignores case. Out-of-range usage is clamped; anything malformed is CMD_UNKNOWN.
Command parseLine(const char* line);
