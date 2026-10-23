#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPOSITORY_DIR=$(CDPATH= cd -- "$PACKAGE_DIR/../.." && pwd)
SOURCE_APP="$REPOSITORY_DIR/build/ViGo.app"
INSTALL_DIR="$HOME/Library/Input Methods"
INSTALLED_APP="$INSTALL_DIR/ViGo.app"
EXPECTED_BUNDLE_ID="com.gosoln.inputmethod.ViGo"
BUILD_EXECUTABLE="$REPOSITORY_DIR/build/ViGo.app/Contents/MacOS/ViGo"
INSTALLED_EXECUTABLE="$INSTALLED_APP/Contents/MacOS/ViGo"

if ps -axo command= | grep -Fx "$BUILD_EXECUTABLE" >/dev/null; then
    echo "Close the build-tree ViGo process before installing." >&2
    exit 1
fi

if ps -axo command= | grep -Fx "$INSTALLED_EXECUTABLE" >/dev/null; then
    echo "Log out to stop the installed input method before upgrading it." >&2
    exit 1
fi

if [ "${VIGO_SKIP_BUILD:-0}" != "1" ]; then
    "$SCRIPT_DIR/build-app.sh" >/dev/null
fi

SOURCE_BUNDLE_ID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" \
    "$SOURCE_APP/Contents/Info.plist")
if [ "$SOURCE_BUNDLE_ID" != "$EXPECTED_BUNDLE_ID" ]; then
    echo "Refusing to install unexpected bundle ID: $SOURCE_BUNDLE_ID" >&2
    exit 1
fi
codesign --verify --deep --strict "$SOURCE_APP"

mkdir -p "$INSTALL_DIR"
STAGED_APP="$INSTALL_DIR/.ViGo.app.install.$$"
if [ -e "$STAGED_APP" ]; then
    echo "Refusing to overwrite stale staging path: $STAGED_APP" >&2
    exit 1
fi
cleanup() {
    if [ -e "$STAGED_APP" ]; then
        rm -rf "$STAGED_APP"
    fi
}
trap cleanup EXIT HUP INT TERM
ditto "$SOURCE_APP" "$STAGED_APP"
codesign --verify --deep --strict "$STAGED_APP"

BACKUP_APP=""
if [ -e "$INSTALLED_APP" ]; then
    INSTALLED_BUNDLE_ID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" \
        "$INSTALLED_APP/Contents/Info.plist" 2>/dev/null || true)
    if [ "$INSTALLED_BUNDLE_ID" != "$EXPECTED_BUNDLE_ID" ]; then
        echo "Refusing to replace unexpected bundle: $INSTALLED_APP" >&2
        exit 1
    fi
    BACKUP_DIR="$REPOSITORY_DIR/build/local-install-backups"
    mkdir -p "$BACKUP_DIR"
    BACKUP_APP="$BACKUP_DIR/ViGo-$(date +%Y%m%d-%H%M%S)-$$.app"
    if [ -e "$BACKUP_APP" ]; then
        echo "Refusing to overwrite backup: $BACKUP_APP" >&2
        exit 1
    fi
    mv "$INSTALLED_APP" "$BACKUP_APP"
    echo "Previous local build preserved at $BACKUP_APP"
fi

if ! mv "$STAGED_APP" "$INSTALLED_APP"; then
    if [ -n "$BACKUP_APP" ] && [ ! -e "$INSTALLED_APP" ]; then
        mv "$BACKUP_APP" "$INSTALLED_APP"
    fi
    exit 1
fi
if ! codesign --verify --deep --strict "$INSTALLED_APP" || \
    ! "$SCRIPT_DIR/input-source-tool.swift" register "$INSTALLED_APP" || \
    ! "$SCRIPT_DIR/input-source-tool.swift" restore-abc >/dev/null; then
    FAILED_DIR="$REPOSITORY_DIR/build/failed-local-installs"
    mkdir -p "$FAILED_DIR"
    FAILED_APP="$FAILED_DIR/ViGo-$(date +%Y%m%d-%H%M%S)-$$.app"
    mv "$INSTALLED_APP" "$FAILED_APP"
    if [ -n "$BACKUP_APP" ] && [ ! -e "$INSTALLED_APP" ]; then
        mv "$BACKUP_APP" "$INSTALLED_APP"
    fi
    echo "Install verification failed; failed bundle preserved at $FAILED_APP" >&2
    exit 1
fi

/usr/bin/open -g "$INSTALLED_APP"
echo "Installed $INSTALLED_APP"
echo "ViGo started in committed-input mode with ABC selected."
echo "Keep ABC selected; the ViGo InputMethodKit source is only for compatibility testing."
