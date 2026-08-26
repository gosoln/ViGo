#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
INSTALLED_APP="$HOME/Library/Input Methods/NAKL.app"
INSTALLED_EXECUTABLE="$INSTALLED_APP/Contents/MacOS/NAKL"
ORIGINAL_SOURCE=$("$SCRIPT_DIR/input-source-tool.swift" current)
FIXTURE_PID=""

cleanup() {
    if [ -n "$FIXTURE_PID" ] && kill -0 "$FIXTURE_PID" 2>/dev/null; then
        kill "$FIXTURE_PID"
        wait "$FIXTURE_PID" 2>/dev/null || true
    fi
    if ! "$SCRIPT_DIR/input-source-tool.swift" select "$ORIGINAL_SOURCE" >/dev/null 2>&1; then
        echo "Warning: could not restore input source $ORIGINAL_SOURCE" >&2
    fi
}
trap cleanup EXIT HUP INT TERM

if [ ! -x "$INSTALLED_EXECUTABLE" ]; then
    echo "Install NAKL locally before running the secure-input smoke test." >&2
    exit 1
fi

METHOD=$(defaults read com.nakl.inputmethod.NAKL input.method 2>/dev/null || echo telex)
ENABLED=$(defaults read com.nakl.inputmethod.NAKL input.enabled 2>/dev/null || echo 1)
if [ "$METHOD" != "telex" ] || [ "$ENABLED" = "0" ]; then
    echo "Select Telex and enable NAKL before running the secure-input smoke test." >&2
    exit 1
fi

open "$INSTALLED_APP"
NAKL_PID=""
for _ in 1 2 3 4 5 6 7 8 9 10; do
    NAKL_PID=$(ps -axo pid=,command= | awk -v command="$INSTALLED_EXECUTABLE" '
        { pid = $1; $1 = ""; sub(/^ /, "", $0); if ($0 == command) { print pid; exit } }
    ')
    [ -n "$NAKL_PID" ] && break
    sleep 0.2
done
if [ -z "$NAKL_PID" ]; then
    echo "Installed NAKL did not launch." >&2
    exit 1
fi
"$SCRIPT_DIR/input-source-tool.swift" restore-abc >/dev/null

swift build --package-path "$PACKAGE_DIR" --product NAKLCompatibilityFixture >/dev/null
FIXTURE_DIR=$(swift build --package-path "$PACKAGE_DIR" --show-bin-path)
FIXTURE_EXECUTABLE="$FIXTURE_DIR/NAKLCompatibilityFixture"
if ps -axo command= | grep -Fx "$FIXTURE_EXECUTABLE" >/dev/null; then
    echo "Close the existing compatibility fixture before running this test." >&2
    exit 1
fi

"$FIXTURE_EXECUTABLE" >/dev/null 2>&1 &
FIXTURE_PID=$!
FIXTURE_READY=0
for _ in 1 2 3 4 5 6 7 8 9 10; do
    if osascript -e 'tell application "System Events" to return exists process "NAKLCompatibilityFixture"' | grep -qx true; then
        FIXTURE_READY=1
        break
    fi
    sleep 0.2
done
if [ "$FIXTURE_READY" != "1" ] || ! kill -0 "$FIXTURE_PID" 2>/dev/null; then
    echo "Compatibility fixture did not launch." >&2
    exit 1
fi

RESULT=$(osascript <<'APPLESCRIPT'
tell application "System Events"
  tell process "NAKLCompatibilityFixture"
    set frontmost to true
    delay 0.5
    set uiItems to entire contents of front window
    set regularFields to {}
    set secureField to missing value
    repeat with itemRef in uiItems
      try
        if role of itemRef is "AXTextField" then
          if subrole of itemRef is "AXSecureTextField" then
            set secureField to itemRef
          else
            set end of regularFields to itemRef
          end if
        end if
      end try
    end repeat
    if secureField is missing value then error "Secure field not found"
    if (count regularFields) < 2 then error "Regular fields not found"

    set beforeField to item 1 of regularFields
    set afterField to item 2 of regularFields
    set focused of beforeField to true
    delay 0.3
    key code 0

    set focused of secureField to true
    delay 0.3
    -- Synthetic `tieengs` must pass through as seven raw characters.
    key code 17
    key code 34
    key code 14
    key code 14
    key code 45
    key code 5
    key code 1

    set focused of afterField to true
    delay 0.3
    key code 1
    delay 0.3

    set beforeText to value of beforeField as text
    set secureCount to count characters of (value of secureField as text)
    set afterText to value of afterField as text
    return beforeText & "|" & secureCount & "|" & afterText
  end tell
end tell
APPLESCRIPT
)

if [ "$RESULT" != "a|7|s" ]; then
    echo "Secure-input smoke test failed: expected 'a|7|s', received '$RESULT'." >&2
    exit 1
fi
if ! kill -0 "$FIXTURE_PID" 2>/dev/null; then
    echo "Compatibility fixture terminated while handling secure input." >&2
    exit 1
fi

echo "PASS: secure input passed through and reset composition on both transitions."
