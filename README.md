# Odette

A quirky, cheerful animated face for the 2.8" ESP32-32E display (240x320 panel, landscape).
A Mac helper drives the face from Claude Code activity.

Each mood fills the screen with its own colour, so you can read it at a glance from across the desk:

| Mood (serial name) | Look |
|---|---|
| idle | Mint. Blinks, glances around, gentle bob |
| thinking | Lavender. Looks up and to the right, one brow raised, "..." bubble top right |
| happy | Yellow. `^ ^` eyes, big smile, bounces |
| needs you (`alert`) | Orange. Wide eyes, jitters, white "!" bubble top right |
| playful sad (`error`) | Blue. Worried brows, puppy eyes, wobbly pout, a falling tear |
| sleepy | Navy, dimmed. Closed eyes, slow breathing, drifting Z z |

Firmware logic tests run on the Mac: `pio test -e native`.

## 1. Flash the firmware

    brew install platformio        # or: pipx install platformio
    ls /dev/cu.*                 # run unplugged and plugged in, the new entry is your port
    pio run -t upload

The default env is `st7789_noinvert`, which is the one that worked on this board.
To replay the boot color test (red, green, blue, white), add `-DCOLOR_TEST` to the env's build_flags.
If upload hangs on "Connecting...", hold BOOT, tap RESET, release BOOT once "Writing at" appears.
You need a USB cable that carries data. Charge-only cables never show up as a port.

Controls without the helper: press BOOT to cycle faces, or type a face name in `pio device monitor`
(`idle happy thinking sleepy alert error next`, `usage 42`, `quirk sneeze`).

Poke him: press anywhere on the screen with a firm fingertip or fingernail (the panel is resistive).

| Mood when poked | Reaction |
|---|---|
| sleepy | Startled awake: a bright pink flash, eyes pop open, "!", a little jump, groggy blinks, a look around, then idle |
| idle | A giggle (squeezed eyes, blush, wiggle) or a quick peek-a-boo off the right edge |
| any other | A one-second giggle, then straight back. He never hides then, so the cue stays visible |

Pokes during a reaction are ignored. The board prints `poke`, and the helper counts it as activity, so a woken face stays awake.

During quiet stretches he does little things on his own, only while idle:

| Quirk | How often |
|---|---|
| whistling, coffee break, yawn & stretch | common |
| snacking (a cookie, bite by bite) | occasional |
| sneeze, peek-a-boo off the edge | rare |

The first one waits 60-90 seconds after he goes idle, then 2-5 minutes between quirks, never the same one twice in a row. While sleepy he blows a snore bubble every 1-2 minutes. Any mood change ends a quirk at once, and a poke interrupts one. To see one now, send `quirk whistle` (or `snack coffee yawn sneeze peek snore`) in `pio device monitor`.

If the display looks wrong, see the table at the bottom.

## 2. Mac helper

The helper has three parts, all in `helper/`:

| File | Job |
|---|---|
| `buddy_daemon.py` | Owns the USB port, merges events from all Claude Code sessions into one face |
| `buddy_hook.py` | Tiny relay that Claude Code hooks call. Prints nothing, always exits 0 |
| `install.py` | Adds or removes the hooks in `~/.claude/settings.json` (dry run by default) |

### Setup

1. Create a venv and install the one dependency:

       python3 -m venv .venv
       .venv/bin/pip install -r helper/requirements.txt

   The commands below use `.venv/bin/python`. A plain `python3` usually lacks pyserial.
2. Close `pio device monitor` if it is open. Only one program can hold the serial port.
3. Test the logic without hooks or a board. In one terminal:

       .venv/bin/python helper/buddy_daemon.py --no-serial --verbose

   In a second terminal:

       echo '{"session_id":"t"}' | python3 helper/buddy_hook.py UserPromptSubmit
       echo '{"session_id":"t"}' | python3 helper/buddy_hook.py Stop

   The first terminal should print `face: thinking`, then `face: happy`.
4. Run the daemon against the board: `.venv/bin/python helper/buddy_daemon.py --verbose`
   Opening the port can reset the board once, so you may see the color test replay. The daemon waits 3 seconds before sending a face.
5. Preview the hook change, then apply it:

       .venv/bin/python helper/install.py
       .venv/bin/python helper/install.py --apply

   The preview prints a diff and writes nothing. `--apply` makes a timestamped backup first.
6. Start a new Claude Code session. The face should switch to thinking when you send a prompt.

The installer copies `buddy_hook.py` to `~/.odette/bin/` and points the hooks at a system python3 plus that copy, with `|| true`, so Claude Code is never affected when an external drive is unmounted. Re-run `--apply` after editing `buddy_hook.py` to refresh the copy. Override the interpreter with `--hook-python`.

Optional, start the daemon at login: `.venv/bin/python helper/install.py --launchagent --apply`, then run the `launchctl bootstrap` line it prints. The installer refuses if that interpreter cannot import pyserial, since the daemon would then exit at every start and launchd would keep restarting it. It copies `buddy_daemon.py` to `~/.odette/bin/` like the hook, so re-run it after editing the daemon. The plist uses the interpreter that runs the installer. If the project sits on an external drive, give the daemon its own venv in your home folder, or it cannot start at login while the drive is unmounted, and macOS privacy protection can block a background process from reading the drive even when it is mounted:

    python3 -m venv ~/.odette/venv
    ~/.odette/venv/bin/pip install -r helper/requirements.txt
    ~/.odette/venv/bin/python helper/install.py --launchagent --apply

### Upgrading

The hooks and the launch agent run copies in `~/.odette/bin/`, not the files in this folder, so after pulling or editing the helper refresh them, and stop any daemon that is still running the old code:

    .venv/bin/python helper/install.py --apply                  # refreshes the hook copy
    .venv/bin/python helper/install.py --launchagent --apply    # if you use it, then run the bootout and bootstrap lines it prints

Restart a daemon you started by hand. Flash new firmware together with the new helper: an old daemon ignores the board's `poke` lines, so a poke would wake the face only for it to fall asleep again at the next keepalive.

### Usage ring (optional)

The small ring in the bottom-right corner shows how much of Claude's 5-hour usage limit is used, as a percentage. Claude Code only hands that number to the status line (claude.ai Pro, Max or Ultra plans, after the first response in a session), so the ring needs one line in your status line script:

    .venv/bin/python helper/install.py --statusline            # preview
    .venv/bin/python helper/install.py --statusline --apply    # write, with a timestamped backup

It adds one line right after the line that reads stdin into a variable (`input=$(cat)`), marked `# odette`. The line runs the relay in the background with its output discarded, so the status line looks and runs exactly as before, even when the daemon is down. Other setups (no status line, an inline command, a script that reads stdin differently) are left alone, and the installer prints the line to add by hand.

- The ring is hidden until a number arrives, and when the board stops hearing from the daemon.
- The number is for the whole account, so the latest report from any session wins. It drops to 0 when the 5-hour window resets.
- The status line only runs when Claude Code updates it (new messages, session start), so during quiet stretches the ring shows the last known value.

Remove everything: `.venv/bin/python helper/install.py --uninstall --apply` (this also removes the status line line). Add `--launchagent` to also delete the launch agent plist.

### What drives each face

| Face | Trigger |
|---|---|
| thinking | A prompt is submitted, or Claude is running tools |
| alert | Claude asks for permission or input (Notification hook). Stays until that session's next event |
| happy | A turn finishes and no other session is still working. Holds 4 seconds |
| error | A turn ends on an API failure (StopFailure hook). Holds 8 seconds |
| sleepy | No events for 10 minutes |
| idle | Everything else |

With several sessions open, priority is alert, error, thinking, happy, sleepy, idle.

### Known limits

- The hook payload field names (`session_id`, `notification_type`) were not confirmed against the docs. The relay falls back safely, but check the daemon log with `--verbose` on the first real run.
- Interrupting a turn with Esc may not fire a Stop event. A working session drops back to idle after 180 seconds (`--work-timeout`), or 600 seconds while a tool call is still open (`--tool-timeout`).
- After you approve a permission prompt, the alert face stays until the next hook event (usually when the tool finishes). No hook event for approval is known.
- The firmware falls back to idle if it hears nothing from the helper for 2 minutes, so a sleeping Mac does not leave it stuck on thinking.
- This covers Claude Code sessions with hooks. It does not see Claude Desktop chats.
- The idle-prompt notification ("Claude is waiting for your input") is ignored on purpose, so the face does not nag after every finished turn.

### Tests

    cd helper && python3 -m unittest discover -s tests -v

## 3. If the display looks wrong

| Symptom | Try |
|---|---|
| Backlight on but screen white or blank | `pio run -e ili9341 -t upload` |
| White looks black, colors look inverted | `-e st7789_invert`, then `-e st7789_noinvert` |
| Red and blue swapped (orange shows blue) | Toggle `-DTFT_RGB_ORDER=TFT_BGR` in `[esp32]` build_flags (this board needs it) |
| Face upside down | Add `-DROTATION=3` to build_flags |
| Garbled or shifting image | Lower `-DSPI_FREQUENCY=40000000` to `27000000` |

## Pins (from the LCDWiki table)

Display: MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, backlight 21, reset shared with EN.
Touch (XPT2046, on its own SPI bus, read directly with no library): CS 33, IRQ 36, CLK 25, MOSI 32, MISO 39.
Pressure reads 3-15 untouched and 1,600-3,400 pressed; a poke is above 400.

## Next

- Mood transitions (a quick squash or blink between moods) were left out of the redesign as optional polish.

## License

MIT, see `LICENSE`.
