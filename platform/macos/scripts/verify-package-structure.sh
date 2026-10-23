#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <ViGo.app> <ViGo.pkg>" >&2
    exit 64
fi

APP_PATH=$1
PACKAGE_PATH=$2
EXECUTABLE_PATH="$APP_PATH/Contents/MacOS/ViGo"
EXPECTED_BUNDLE_ID="com.gosoln.inputmethod.ViGo"

if [ ! -x "$EXECUTABLE_PATH" ]; then
    echo "Missing ViGo executable: $EXECUTABLE_PATH" >&2
    exit 1
fi
if [ ! -f "$PACKAGE_PATH" ]; then
    echo "Missing installer package: $PACKAGE_PATH" >&2
    exit 1
fi

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
xar -tf "$PACKAGE_PATH" >/dev/null

AUDIT_DIR=$(mktemp -d /tmp/vigo-package-audit.XXXXXX)
cleanup() {
    rm -rf "$AUDIT_DIR"
}
trap cleanup EXIT HUP INT TERM
pkgutil --expand-full "$PACKAGE_PATH" "$AUDIT_DIR/expanded"
EMBEDDED_APP="$AUDIT_DIR/expanded/ViGo-component.pkg/Payload/Library/Input Methods/ViGo.app"
if [ ! -d "$EMBEDDED_APP" ]; then
    echo "Package does not contain the expected ViGo app path." >&2
    exit 1
fi
diff -qr "$APP_PATH" "$EMBEDDED_APP"

PACKAGE_INFO="$AUDIT_DIR/expanded/ViGo-component.pkg/PackageInfo"
grep -q "identifier=\"com.gosoln.inputmethod.ViGo.pkg\"" "$PACKAGE_INFO"
grep -q "version=\"$MARKETING_VERSION.$BUILD_NUMBER\"" "$PACKAGE_INFO"

DISTRIBUTION="$AUDIT_DIR/expanded/Distribution"
xmllint --noout "$DISTRIBUTION"
grep -q 'hostArchitectures="arm64,x86_64"' "$DISTRIBUTION"
grep -q '<os-version min="13.0"/>' "$DISTRIBUTION"
grep -q 'enable_currentUserHome="false"' "$DISTRIBUTION"
grep -q 'onConclusion="RequireLogout"' "$DISTRIBUTION"
if find "$AUDIT_DIR/expanded" -name '._*' -print -quit | grep -q .; then
    echo "Release package contains AppleDouble metadata." >&2
    exit 1
fi

echo "PASS: package payload, metadata, architectures, and deployment target are valid."
