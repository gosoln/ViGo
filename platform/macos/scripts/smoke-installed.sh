#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
INSTALLED_APP="$HOME/Library/Input Methods/NAKL.app"
INSTALLED_EXECUTABLE="$INSTALLED_APP/Contents/MacOS/NAKL"
EXPECTED_BUNDLE_ID="com.nakl.inputmethod.NAKL"

restore_committed_source() {
    if ! "$SCRIPT_DIR/input-source-tool.swift" restore-abc >/dev/null 2>&1; then
        echo "Warning: could not restore ABC committed-input mode" >&2
    fi
}
trap restore_committed_source EXIT HUP INT TERM

if [ ! -d "$INSTALLED_APP" ]; then
    echo "Install NAKL locally before running the smoke test." >&2
    exit 1
fi
BUNDLE_ID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" \
    "$INSTALLED_APP/Contents/Info.plist")
if [ "$BUNDLE_ID" != "$EXPECTED_BUNDLE_ID" ]; then
    echo "Unexpected installed bundle ID: $BUNDLE_ID" >&2
    exit 1
fi
codesign --verify --deep --strict "$INSTALLED_APP"

METHOD=$(defaults read "$EXPECTED_BUNDLE_ID" input.method 2>/dev/null || echo telex)
ENABLED=$(defaults read "$EXPECTED_BUNDLE_ID" input.enabled 2>/dev/null || echo 1)
if [ "$METHOD" != "telex" ] || [ "$ENABLED" = "0" ]; then
    echo "Select Telex and enable NAKL before running the smoke test." >&2
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
sleep 1
if ! kill -0 "$NAKL_PID" 2>/dev/null; then
    echo "Installed NAKL terminated during IMKServer startup." >&2
    exit 1
fi

"$SCRIPT_DIR/input-source-tool.swift" restore-abc >/dev/null
RESULT=$(osascript <<'APPLESCRIPT'
tell application "TextEdit"
  set testDocument to make new document
  set testWindow to first window whose name is name of testDocument
  set index of testWindow to 1
  activate
end tell
try
  tell application "System Events" to tell process "TextEdit"
    set frontmost to true
    perform action "AXRaise" of front window
    click text area 1 of scroll area 1 of front window
  end tell
  delay 0.5
  tell application "System Events"
    -- `giair chaf` covers tone relocation and the committed replacement boundary.
    key code 5
    key code 34
    key code 0
    key code 34
    key code 15
    key code 49
    key code 8
    key code 4
    key code 0
    key code 3
  end tell
  delay 0.5
  tell application "System Events" to tell process "TextEdit" to ¬
    set actualText to value of text area 1 of scroll area 1 of front window
on error messageText number messageNumber
  tell application "TextEdit" to close testDocument saving no
  error messageText number messageNumber
end try
tell application "TextEdit" to close testDocument saving no
return actualText
APPLESCRIPT
)

case "$RESULT" in
    "giải chà" | "Giải chà") ;;
    *)
        echo "Installed smoke test failed: expected 'giải chà', received '$RESULT'." >&2
        exit 1
        ;;
esac
if ! kill -0 "$NAKL_PID" 2>/dev/null; then
    echo "Installed NAKL terminated while handling input." >&2
    exit 1
fi

echo "PASS: installed NAKL composed committed Telex input in TextEdit without duplication."
