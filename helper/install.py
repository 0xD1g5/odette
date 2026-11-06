#!/usr/bin/env python3
"""Install or remove the Desk Buddy hooks in Claude Code's settings.json.

Default is a dry run: it shows exactly what would change and writes nothing.
Add --apply to write. A timestamped backup is made first.

    python3 install.py                  show the change
    python3 install.py --apply          install the hooks
    python3 install.py --uninstall --apply     remove only Desk Buddy's hooks
    python3 install.py --launchagent --apply   also start the daemon at login (macOS)
    python3 install.py --statusline --apply    also feed the 5-hour usage ring from your status line

The hooks are observational: the relay script prints nothing and always exits 0.
"""

import argparse
import copy
import difflib
import json
import os
import plistlib
import re
import shlex
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HOOK = os.path.join(HERE, "buddy_hook.py")
DAEMON = os.path.join(HERE, "buddy_daemon.py")
MARK = "buddy_hook.py"
EVENTS = [
    "SessionStart",
    "UserPromptSubmit",
    "PreToolUse",
    "PostToolUse",
    "PostToolUseFailure",
    "Notification",
    "Stop",
    "StopFailure",
    "SessionEnd",
]
DEFAULT_SETTINGS = os.path.expanduser("~/.claude/settings.json")
PLIST_PATH = os.path.expanduser("~/Library/LaunchAgents/com.deskbuddy.daemon.plist")
LOG_PATH = os.path.expanduser("~/.desk-buddy/daemon.log")
BIN_DIR = os.path.expanduser("~/.desk-buddy/bin")
LOCAL_HOOK = os.path.join(BIN_DIR, "buddy_hook.py")
LOCAL_DAEMON = os.path.join(BIN_DIR, "buddy_daemon.py")
STATUSLINE_MARK = "# desk-buddy"
STATUSLINE_SHELLS = ("bash", "sh", "zsh", "/bin/bash", "/bin/sh", "/bin/zsh")
STDIN_LINE = re.compile(r"^(\s*)(\w+)=\$\(cat\)\s*$")
PYTHON_CANDIDATES = ["/opt/homebrew/bin/python3", "/usr/local/bin/python3", "/usr/bin/python3"]


def pick_python(override=None):
    """Interpreter for the hook command. Must not live on an external drive."""
    if override:
        return override
    for candidate in PYTHON_CANDIDATES:
        if os.path.exists(candidate):
            return candidate
    return sys.executable


def load(path):
    if not os.path.exists(path):
        return {}
    with open(path, "r") as handle:
        text = handle.read()
    if not text.strip():
        return {}
    data = json.loads(text)
    if not isinstance(data, dict):
        raise ValueError("top level of %s is not a JSON object" % path)
    return data


def is_ours(handler):
    return isinstance(handler, dict) and MARK in str(handler.get("command", ""))


def strip_ours(settings):
    hooks = settings.get("hooks")
    if hooks is None:
        return
    if not isinstance(hooks, dict):
        raise ValueError("'hooks' in settings.json is not an object")
    for event in list(hooks):
        groups = hooks[event]
        if not isinstance(groups, list):
            continue
        kept = []
        for group in groups:
            if isinstance(group, dict) and isinstance(group.get("hooks"), list):
                before = len(group["hooks"])
                group["hooks"] = [h for h in group["hooks"] if not is_ours(h)]
                if before and not group["hooks"]:
                    continue
            kept.append(group)
        if kept:
            hooks[event] = kept
        else:
            del hooks[event]
    if not hooks:
        del settings["hooks"]


def add_ours(settings, python=None, hook=None):
    python = python or sys.executable
    hook = hook or LOCAL_HOOK
    hooks = settings.setdefault("hooks", {})
    if not isinstance(hooks, dict):
        raise ValueError("'hooks' in settings.json is not an object")
    for event in EVENTS:
        # "|| true" keeps a missing file or interpreter from ever blocking Claude Code (exit 2 blocks).
        command = '"%s" "%s" %s || true' % (python, hook, event)
        hooks.setdefault(event, []).append(
            {"hooks": [{"type": "command", "command": command, "timeout": 5}]}
        )


def statusline_command(var, python, hook):
    """The line that hands the status line's JSON to the relay: in the background, silent, never blocking."""
    return 'printf \'%%s\' "$%s" | "%s" "%s" StatusLine >/dev/null 2>&1 & %s' % (var, python, hook, STATUSLINE_MARK)


def statusline_script(settings):
    """Path of the script statusLine.command runs ("bash <path>", "sh <path>" or "<path>"), if it exists."""
    status = settings.get("statusLine")
    if not isinstance(status, dict) or not isinstance(status.get("command"), str):
        return None
    try:
        argv = shlex.split(status["command"])
    except ValueError:
        return None
    if len(argv) == 2 and argv[0] in STATUSLINE_SHELLS:
        path = argv[1]
    elif len(argv) == 1:
        path = argv[0]
    else:
        return None
    path = os.path.realpath(os.path.expanduser(path))  # a dotfiles symlink: edit its target, keep the link
    return path if os.path.isfile(path) else None


def strip_statusline_line(text):
    return "".join(line for line in text.splitlines(True) if not line.rstrip().endswith(STATUSLINE_MARK))


def add_statusline_line(text, python, hook):
    """Insert our line right after the first `<var>=$(cat)`. None if the script does not read stdin that way."""
    lines = strip_statusline_line(text).splitlines(True)
    for i, line in enumerate(lines):
        match = STDIN_LINE.match(line)
        if match:
            indent, var = match.groups()
            lines.insert(i + 1, indent + statusline_command(var, python, hook) + "\n")
            return "".join(lines)
    return None


def statusline_change(settings, python, uninstall):
    """(path, old text, new text) for the status line script, or (None, reason, None) if it is not supported."""
    path = statusline_script(settings)
    if path is None:
        return None, "statusLine.command does not run a script file", None
    with open(path, "r") as handle:
        old = handle.read()
    new = strip_statusline_line(old) if uninstall else add_statusline_line(old, python, LOCAL_HOOK)
    if new is None:
        return None, "%s has no `input=$(cat)` line" % path, None
    return path, old, new


def dump(settings):
    return json.dumps(settings, indent=2, ensure_ascii=False) + "\n"


def write_settings(path, text):
    directory = os.path.dirname(path)
    if directory:
        os.makedirs(directory, exist_ok=True)
    backup = None
    mode = None
    if os.path.exists(path):
        backup = "%s.deskbuddy-backup-%s" % (path, time.strftime("%Y%m%d-%H%M%S"))
        shutil.copy2(path, backup)
        mode = os.stat(path).st_mode & 0o777
    temp = path + ".deskbuddy-tmp"
    with open(temp, "w") as handle:
        handle.write(text)
    if mode is not None:
        os.chmod(temp, mode)
    os.replace(temp, path)
    return backup


def claude_version():
    try:
        out = subprocess.check_output(["claude", "--version"], stderr=subprocess.STDOUT, timeout=10)
        return out.decode("utf-8", "replace").strip()
    except Exception:
        return "not found on PATH"


def copy_hook():
    os.makedirs(BIN_DIR, exist_ok=True)
    shutil.copy2(HOOK, LOCAL_HOOK)
    os.chmod(LOCAL_HOOK, 0o755)


def remove_hook_copy():
    try:
        os.unlink(LOCAL_HOOK)
    except OSError:
        pass


def has_pyserial(python):
    """True if this interpreter can import serial. The daemon exits 2 without it,
    and launchd's KeepAlive would restart it every few seconds forever."""
    try:
        return subprocess.run([python, "-c", "import serial"], stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL, timeout=20).returncode == 0
    except Exception:
        return False


def remove_launchagent():
    if os.path.exists(PLIST_PATH):
        os.unlink(PLIST_PATH)
        # Name the service by label: bootout with a plist path fails once the file is gone.
        print("Removed %s. If it is loaded, run:" % PLIST_PATH)
        print("  launchctl bootout gui/$(id -u)/com.deskbuddy.daemon")
    try:
        os.unlink(LOCAL_DAEMON)
    except OSError:
        pass


def launchagent(apply):
    python = sys.executable
    if python.startswith("/Volumes/"):
        sys.stderr.write("\nWarning: %s is on an external drive. The daemon cannot start at login "
                         "while it is unmounted.\n" % python)
    plist = {
        "Label": "com.deskbuddy.daemon",
        # A copy in ~/.desk-buddy/bin, like the hook, so the script itself never lives on an external drive.
        "ProgramArguments": [python, LOCAL_DAEMON],
        "RunAtLoad": True,
        "KeepAlive": {"SuccessfulExit": False},
        "StandardOutPath": LOG_PATH,
        "StandardErrorPath": LOG_PATH,
        "ProcessType": "Background",
    }
    print("\nLaunch agent: %s" % PLIST_PATH)
    print(plistlib.dumps(plist).decode("utf-8"))
    if not apply:
        print("(dry run, nothing written)")
        return
    os.makedirs(BIN_DIR, exist_ok=True)
    shutil.copy2(DAEMON, LOCAL_DAEMON)
    os.chmod(LOCAL_DAEMON, 0o755)
    os.makedirs(os.path.dirname(PLIST_PATH), exist_ok=True)
    os.makedirs(os.path.dirname(LOG_PATH), exist_ok=True)
    with open(PLIST_PATH, "wb") as handle:
        plistlib.dump(plist, handle)
    print("Written. Start it now with:")
    print("  launchctl bootstrap gui/$(id -u) %s" % PLIST_PATH)
    print("Stop and unload it with:")
    print("  launchctl bootout gui/$(id -u) %s" % PLIST_PATH)


def statusline(settings, python, args):
    """Show, and with --apply write, the status line script change. Uninstall only removes our own line."""
    path, old, new = statusline_change(settings, python, args.uninstall)
    if path is None:
        if args.statusline and not args.uninstall:
            print("\nStatus line: not changed (%s). To show usage on the board, add this line right after" % old)
            print("the line where your status line script reads stdin into a variable (here called input):")
            print("  " + statusline_command("input", python, LOCAL_HOOK))
        return
    if new == old:
        if args.statusline:
            print("\nStatus line: %s already up to date." % path)
        return
    print("\nStatus line script: %s" % path)
    print("\n".join(difflib.unified_diff(old.splitlines(), new.splitlines(), "current", "proposed", lineterm="")))
    if args.apply:
        backup = write_settings(path, new)
        print("Written. Backup: %s" % backup)
    else:
        print("(dry run, nothing written)")


def main():
    parser = argparse.ArgumentParser(description="Install Desk Buddy hooks into Claude Code")
    parser.add_argument("--settings", default=DEFAULT_SETTINGS, help="settings.json to edit")
    parser.add_argument("--apply", action="store_true", help="write the change (default is a dry run)")
    parser.add_argument("--uninstall", action="store_true", help="remove Desk Buddy's hooks")
    parser.add_argument("--launchagent", action="store_true", help="also write a macOS launch agent for the daemon")
    parser.add_argument("--statusline", action="store_true",
                        help="also add one line to your status line script so the board can show 5-hour usage")
    parser.add_argument("--hook-python", default=None,
                        help="interpreter for the hook command (default: a system python3, never one on an external drive)")
    args = parser.parse_args()
    python = pick_python(args.hook_python)
    if python.startswith("/Volumes/"):
        sys.stderr.write("Warning: %s is on an external drive. Hooks will do nothing while it is unmounted.\n" % python)
    if args.launchagent and not args.uninstall and not has_pyserial(sys.executable):
        sys.stderr.write("%s cannot import pyserial, so the daemon would exit at every start and\n"
                         "launchd would restart it forever. Run the installer with the venv interpreter:\n"
                         "  .venv/bin/python helper/install.py --launchagent --apply\n"
                         "Nothing changed.\n" % sys.executable)
        return 1

    path = os.path.abspath(os.path.expanduser(args.settings))
    try:
        before = load(path)
    except (ValueError, OSError) as exc:
        sys.stderr.write("Cannot read %s: %s\nNothing changed.\n" % (path, exc))
        return 1

    after = copy.deepcopy(before)
    try:
        strip_ours(after)
        if not args.uninstall:
            add_ours(after, python)
    except ValueError as exc:
        sys.stderr.write("%s\nNothing changed.\n" % exc)
        return 1

    old_text = dump(before) if before else ""
    new_text = dump(after) if after else ""
    print("Settings file: %s" % path)
    print("Claude Code:   %s" % claude_version())
    print("Hook python:   %s" % python)
    print("Hook script:   %s" % LOCAL_HOOK)
    print("Action:        %s (%s)\n" % ("uninstall" if args.uninstall else "install", "APPLY" if args.apply else "dry run"))
    diff = list(difflib.unified_diff(old_text.splitlines(), new_text.splitlines(),
                                     "current", "proposed", lineterm=""))
    if diff:
        print("\n".join(diff))
    else:
        print("No change needed.")

    if args.apply:
        if args.uninstall:
            remove_hook_copy()
            if args.launchagent:
                remove_launchagent()
        else:
            copy_hook()
    if args.apply and diff:
        backup = write_settings(path, new_text)
        print("\nWritten.%s" % (" Backup: %s" % backup if backup else ""))
        print("Start a new Claude Code session to load the hooks. If it reports a settings error, run:")
        print("  python3 %s --uninstall --apply" % os.path.abspath(__file__))
    elif not args.apply:
        print("\n(dry run, nothing written. Add --apply to write.)")

    if args.statusline or args.uninstall:
        statusline(before, python, args)

    if args.launchagent and args.uninstall and not args.apply:
        print("(dry run) would remove %s and %s" % (PLIST_PATH, LOCAL_DAEMON))
    if args.launchagent and not args.uninstall:
        launchagent(args.apply)
    return 0


if __name__ == "__main__":
    sys.exit(main())
