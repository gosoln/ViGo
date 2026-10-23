#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
INSTALL_DIR="$HOME/Library/Input Methods"
INSTALLED_APP="$INSTALL_DIR/ViGo.app"
EXPECTED_BUNDLE_ID="com.gosoln.inputmethod.ViGo"
INSTALLED_EXECUTABLE="$INSTALLED_APP/Contents/MacOS/ViGo"

if [ ! -e "$INSTALLED_APP" ]; then
    echo "ViGo is not installed at $INSTALLED_APP"
    exit 0
fi

BUNDLE_ID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" \
    "$INSTALLED_APP/Contents/Info.plist" 2>/dev/null || true)
if [ "$BUNDLE_ID" != "$EXPECTED_BUNDLE_ID" ]; then
    echo "Refusing to remove unexpected bundle: $BUNDLE_ID" >&2
    exit 1
fi

if ps -axo command= | grep -Fx "$INSTALLED_EXECUTABLE" >/dev/null; then
    echo "Log out to stop ViGo before uninstalling it." >&2
    exit 1
fi

CURRENT_SOURCE=$("$SCRIPT_DIR/input-source-tool.swift" current)
if [ "$CURRENT_SOURCE" = "$EXPECTED_BUNDLE_ID" ]; then
    "$SCRIPT_DIR/input-source-tool.swift" restore-abc
fi
"$SCRIPT_DIR/input-source-tool.swift" disable

TRASH_APP="$HOME/.Trash/ViGo-$(date +%Y%m%d-%H%M%S)-$$.app"
if [ -e "$TRASH_APP" ]; then
    echo "Refusing to overwrite Trash item: $TRASH_APP" >&2
    exit 1
fi
mv "$INSTALLED_APP" "$TRASH_APP"

echo "Moved ViGo to $TRASH_APP; preferences were preserved."
echo "Log out and back in to refresh the input-source menu."
