#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SCRIPT_PATH="$SCRIPT_DIR/$(basename -- "$0")"
INSTALLED_APP="/Library/Input Methods/ViGo.app"
EXPECTED_BUNDLE_ID="com.gosoln.inputmethod.ViGo"
RECEIPT_ID="com.gosoln.inputmethod.ViGo.pkg"

if [ "$(id -u)" -ne 0 ]; then
    CURRENT_SOURCE=$("$SCRIPT_DIR/input-source-tool.swift" current)
    if [ "$CURRENT_SOURCE" = "$EXPECTED_BUNDLE_ID" ]; then
        "$SCRIPT_DIR/input-source-tool.swift" restore-abc
    fi
    "$SCRIPT_DIR/input-source-tool.swift" disable
    exec sudo "$SCRIPT_PATH" --root-phase
fi

if [ "${1:-}" != "--root-phase" ]; then
    echo "Run this script as the logged-in user, not directly as root." >&2
    exit 64
fi
if [ ! -e "$INSTALLED_APP" ]; then
    echo "ViGo is not installed at $INSTALLED_APP"
    pkgutil --forget "$RECEIPT_ID" >/dev/null 2>&1 || true
    exit 0
fi

BUNDLE_ID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" \
    "$INSTALLED_APP/Contents/Info.plist" 2>/dev/null || true)
if [ "$BUNDLE_ID" != "$EXPECTED_BUNDLE_ID" ]; then
    echo "Refusing to remove unexpected bundle: $BUNDLE_ID" >&2
    exit 1
fi

INSTALLED_EXECUTABLE="$INSTALLED_APP/Contents/MacOS/ViGo"
if ps -axo command= | grep -Fx "$INSTALLED_EXECUTABLE" >/dev/null; then
    echo "Log out to stop ViGo before uninstalling it." >&2
    exit 1
fi

RECOVERY_APP="/Users/Shared/ViGo-Uninstalled-$(date +%Y%m%d-%H%M%S)-$$.app"
if [ -e "$RECOVERY_APP" ]; then
    echo "Refusing to overwrite recovery bundle: $RECOVERY_APP" >&2
    exit 1
fi
mv "$INSTALLED_APP" "$RECOVERY_APP"
if ! pkgutil --forget "$RECEIPT_ID"; then
    echo "Bundle moved, but receipt cleanup failed. Recovery: $RECOVERY_APP" >&2
    exit 1
fi

echo "Moved ViGo to $RECOVERY_APP; preferences were preserved."
echo "Log out and back in to refresh the input-source menu."
