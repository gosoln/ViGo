#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <ViGo.app> <ViGo.pkg>" >&2
    exit 64
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP_PATH=$1
PACKAGE_PATH=$2

"$SCRIPT_DIR/verify-package-structure.sh" "$APP_PATH" "$PACKAGE_PATH"
if ! codesign -dv --verbose=4 "$APP_PATH" 2>&1 | \
    grep -q '^Authority=Developer ID Application:'; then
    echo "Release app is not signed with a Developer ID Application identity." >&2
    exit 1
fi
if ! codesign -dv --verbose=4 "$APP_PATH" 2>&1 | grep -q 'flags=.*runtime'; then
    echo "Release app does not enable Hardened Runtime." >&2
    exit 1
fi
if ! codesign -dv --verbose=4 "$APP_PATH" 2>&1 | grep -q '^Timestamp='; then
    echo "Release app does not have a secure timestamp." >&2
    exit 1
fi
if codesign -d --entitlements - "$APP_PATH" 2>&1 | grep -q 'get-task-allow'; then
    echo "Release entitlements must not contain get-task-allow." >&2
    exit 1
fi
spctl --assess --type execute --verbose=2 "$APP_PATH"
if ! pkgutil --check-signature "$PACKAGE_PATH" | grep -q 'Developer ID Installer:'; then
    echo "Release package is not signed with a Developer ID Installer identity." >&2
    exit 1
fi
xcrun stapler validate "$PACKAGE_PATH"
spctl --assess --type install --verbose=2 "$PACKAGE_PATH"
