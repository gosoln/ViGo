#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPOSITORY_DIR=$(CDPATH= cd -- "$PACKAGE_DIR/../.." && pwd)
APP_DIR="$REPOSITORY_DIR/build/NAKL.app"
WORK_DIR="$REPOSITORY_DIR/build/macos-package"
PAYLOAD_ROOT="$WORK_DIR/root"
COMPONENT_PACKAGE="$WORK_DIR/NAKL-component.pkg"
DIST_DIR="$REPOSITORY_DIR/dist"

"$SCRIPT_DIR/build-app.sh" >/dev/null
VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
    "$APP_DIR/Contents/Info.plist")
BUILD_NUMBER=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" \
    "$APP_DIR/Contents/Info.plist")
PACKAGE_VERSION="$VERSION.$BUILD_NUMBER"
OUTPUT_PACKAGE="$DIST_DIR/NAKL-$VERSION-$BUILD_NUMBER.pkg"
STAGED_PACKAGE="$WORK_DIR/NAKL-$VERSION-$BUILD_NUMBER.pkg"

if [ -e "$OUTPUT_PACKAGE" ]; then
    echo "Refusing to overwrite release artifact: $OUTPUT_PACKAGE" >&2
    exit 1
fi

rm -rf "$WORK_DIR"
mkdir -p "$PAYLOAD_ROOT/Library/Input Methods" "$DIST_DIR"
ditto --noextattr --norsrc "$APP_DIR" "$PAYLOAD_ROOT/Library/Input Methods/NAKL.app"
if find "$PAYLOAD_ROOT" -name '._*' -print -quit | grep -q .; then
    echo "Package payload contains AppleDouble metadata." >&2
    exit 1
fi

xcrun pkgbuild \
    --root "$PAYLOAD_ROOT" \
    --component-plist "$PACKAGE_DIR/Packaging/component.plist" \
    --identifier "com.nakl.inputmethod.NAKL.pkg" \
    --version "$PACKAGE_VERSION" \
    --install-location / \
    --ownership recommended \
    "$COMPONENT_PACKAGE"

sed "s/@VERSION@/$PACKAGE_VERSION/g" \
    "$PACKAGE_DIR/Packaging/Distribution.xml.in" > "$WORK_DIR/Distribution.xml"

INSTALLER_IDENTITY=${NAKL_INSTALLER_IDENTITY:-}
if [ -n "$INSTALLER_IDENTITY" ]; then
    if [ -z "${NAKL_CODESIGN_IDENTITY:-}" ] || [ "$NAKL_CODESIGN_IDENTITY" = "-" ]; then
        echo "A signed package requires NAKL_CODESIGN_IDENTITY." >&2
        exit 1
    fi
    if ! codesign -dv --verbose=4 "$APP_DIR" 2>&1 | \
        grep -q '^Authority=Developer ID Application:'; then
        echo "Signed packages require a Developer ID Application-signed app." >&2
        exit 1
    fi
    if ! codesign -dv --verbose=4 "$APP_DIR" 2>&1 | grep -q 'flags=.*runtime'; then
        echo "Signed packages require Hardened Runtime." >&2
        exit 1
    fi
    if ! codesign -dv --verbose=4 "$APP_DIR" 2>&1 | grep -q '^Timestamp='; then
        echo "Signed packages require a secure timestamp." >&2
        exit 1
    fi
    if codesign -d --entitlements - "$APP_DIR" 2>&1 | grep -q 'get-task-allow'; then
        echo "Release entitlements must not contain get-task-allow." >&2
        exit 1
    fi
    xcrun productbuild \
        --distribution "$WORK_DIR/Distribution.xml" \
        --package-path "$WORK_DIR" \
        --sign "$INSTALLER_IDENTITY" \
        "$STAGED_PACKAGE"
else
    xcrun productbuild \
        --distribution "$WORK_DIR/Distribution.xml" \
        --package-path "$WORK_DIR" \
        "$STAGED_PACKAGE"
fi

xar -tf "$STAGED_PACKAGE" >/dev/null
mv "$STAGED_PACKAGE" "$OUTPUT_PACKAGE"
if [ -n "$INSTALLER_IDENTITY" ]; then
    pkgutil --check-signature "$OUTPUT_PACKAGE" | grep -q 'Developer ID Installer:'
else
    pkgutil --check-signature "$OUTPUT_PACKAGE" || true
fi
echo "$OUTPUT_PACKAGE"
