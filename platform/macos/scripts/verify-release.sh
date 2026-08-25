#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <NAKL.app> <NAKL.pkg>" >&2
    exit 64
fi

APP_PATH=$1
PACKAGE_PATH=$2
EXECUTABLE_PATH="$APP_PATH/Contents/MacOS/NAKL"
EXPECTED_BUNDLE_ID="com.nakl.inputmethod.NAKL"
ARCHITECTURES=$(lipo -archs "$EXECUTABLE_PATH")
set -- $ARCHITECTURES
if [ "$#" -ne 2 ]; then
    echo "Expected exactly arm64 and x86_64; found: $ARCHITECTURES" >&2
    exit 1
fi
for expected_architecture in arm64 x86_64; do
    case " $ARCHITECTURES " in
        *" $expected_architecture "*) ;;
        *) echo "Missing $expected_architecture slice: $ARCHITECTURES" >&2; exit 1 ;;
    esac
done

plutil -lint "$APP_PATH/Contents/Info.plist"
BUNDLE_ID=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" \
    "$APP_PATH/Contents/Info.plist")
MARKETING_VERSION=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" \
    "$APP_PATH/Contents/Info.plist")
BUILD_NUMBER=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" \
    "$APP_PATH/Contents/Info.plist")
if [ "$BUNDLE_ID" != "$EXPECTED_BUNDLE_ID" ]; then
    echo "Unexpected bundle ID: $BUNDLE_ID" >&2
    exit 1
fi
if [ "$(vtool -show-build "$EXECUTABLE_PATH" | grep -c 'minos 13\.0')" -ne 2 ]; then
    echo "Every architecture must target macOS 13.0." >&2
    exit 1
fi

codesign --verify --deep --strict --verbose=2 "$APP_PATH"
codesign -dv --verbose=4 "$APP_PATH" 2>&1 | grep -q '^Authority=Developer ID Application:'
codesign -dv --verbose=4 "$APP_PATH" 2>&1 | grep -q 'flags=.*runtime'
codesign -dv --verbose=4 "$APP_PATH" 2>&1 | grep -q '^Timestamp='
if codesign -d --entitlements - "$APP_PATH" 2>&1 | grep -q 'get-task-allow'; then
    echo "Release entitlements must not contain get-task-allow." >&2
    exit 1
fi
spctl --assess --type execute --verbose=2 "$APP_PATH"
pkgutil --check-signature "$PACKAGE_PATH" | grep -q 'Developer ID Installer:'
xcrun stapler validate "$PACKAGE_PATH"
spctl --assess --type install --verbose=2 "$PACKAGE_PATH"

AUDIT_DIR=$(mktemp -d /tmp/nakl-release-audit.XXXXXX)
cleanup() {
    rm -rf "$AUDIT_DIR"
}
trap cleanup EXIT HUP INT TERM
pkgutil --expand-full "$PACKAGE_PATH" "$AUDIT_DIR/expanded"
EMBEDDED_APP="$AUDIT_DIR/expanded/NAKL-component.pkg/Payload/Library/Input Methods/NAKL.app"
if [ ! -d "$EMBEDDED_APP" ]; then
    echo "Package does not contain the expected NAKL app path." >&2
    exit 1
fi
cmp "$APP_PATH/Contents/Info.plist" "$EMBEDDED_APP/Contents/Info.plist"
cmp "$EXECUTABLE_PATH" "$EMBEDDED_APP/Contents/MacOS/NAKL"
PACKAGE_INFO="$AUDIT_DIR/expanded/NAKL-component.pkg/PackageInfo"
grep -q "identifier=\"com.nakl.inputmethod.NAKL.pkg\"" "$PACKAGE_INFO"
grep -q "version=\"$MARKETING_VERSION.$BUILD_NUMBER\"" "$PACKAGE_INFO"
if find "$AUDIT_DIR/expanded" -name '._*' -print -quit | grep -q .; then
    echo "Release package contains AppleDouble metadata." >&2
    exit 1
fi
