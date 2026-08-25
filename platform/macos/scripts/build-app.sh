#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPOSITORY_DIR=$(CDPATH= cd -- "$PACKAGE_DIR/../.." && pwd)
APP_DIR="$REPOSITORY_DIR/build/NAKL.app"
ARM64_CORE="$REPOSITORY_DIR/target/aarch64-apple-darwin/release/libnakl_core.a"
X86_64_CORE="$REPOSITORY_DIR/target/x86_64-apple-darwin/release/libnakl_core.a"
ARM64_SCRATCH="$REPOSITORY_DIR/build/swift-arm64"
X86_64_SCRATCH="$REPOSITORY_DIR/build/swift-x86_64"
mkdir -p "$REPOSITORY_DIR/build"
STAGING_DIR=$(mktemp -d "$REPOSITORY_DIR/build/.nakl-app.XXXXXX")
STAGED_APP="$STAGING_DIR/NAKL.app"

cleanup() {
    rm -rf "$STAGING_DIR"
}
trap cleanup EXIT HUP INT TERM

for rust_target in aarch64-apple-darwin x86_64-apple-darwin; do
    if ! rustup target list --installed | grep -qx "$rust_target"; then
        echo "Missing Rust target: run 'rustup target add $rust_target'" >&2
        exit 1
    fi
done

MACOSX_DEPLOYMENT_TARGET=13.0 cargo build \
    --manifest-path "$REPOSITORY_DIR/Cargo.toml" \
    -p nakl-core --release --target aarch64-apple-darwin
MACOSX_DEPLOYMENT_TARGET=13.0 cargo build \
    --manifest-path "$REPOSITORY_DIR/Cargo.toml" \
    -p nakl-core --release --target x86_64-apple-darwin

NAKL_CORE_LIBRARY="$ARM64_CORE" swift build \
    --package-path "$PACKAGE_DIR" --scratch-path "$ARM64_SCRATCH" \
    --triple arm64-apple-macosx13.0 -c release
NAKL_CORE_LIBRARY="$X86_64_CORE" swift build \
    --package-path "$PACKAGE_DIR" --scratch-path "$X86_64_SCRATCH" \
    --triple x86_64-apple-macosx13.0 -c release

ARM64_BIN=$(NAKL_CORE_LIBRARY="$ARM64_CORE" swift build \
    --package-path "$PACKAGE_DIR" --scratch-path "$ARM64_SCRATCH" \
    --triple arm64-apple-macosx13.0 -c release --show-bin-path)
X86_64_BIN=$(NAKL_CORE_LIBRARY="$X86_64_CORE" swift build \
    --package-path "$PACKAGE_DIR" --scratch-path "$X86_64_SCRATCH" \
    --triple x86_64-apple-macosx13.0 -c release --show-bin-path)

mkdir -p "$STAGED_APP/Contents/MacOS" "$STAGED_APP/Contents/Resources"
lipo -create "$ARM64_BIN/NAKL" "$X86_64_BIN/NAKL" \
    -output "$STAGED_APP/Contents/MacOS/NAKL"
cp "$PACKAGE_DIR/Resources/Info.plist" "$STAGED_APP/Contents/Info.plist"
cp -X "$REPOSITORY_DIR/LICENSE" "$STAGED_APP/Contents/Resources/LICENSE"

BUILD_NUMBER=${NAKL_BUILD_NUMBER:-$(git -C "$REPOSITORY_DIR" rev-list --count HEAD)}
case "$BUILD_NUMBER" in
    ''|*[!0-9]*) echo "NAKL_BUILD_NUMBER must contain only decimal digits." >&2; exit 1 ;;
esac
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $BUILD_NUMBER" \
    "$STAGED_APP/Contents/Info.plist"
xattr -cr "$STAGED_APP"

SIGNING_IDENTITY=${NAKL_CODESIGN_IDENTITY:--}
if [ "$SIGNING_IDENTITY" = "-" ]; then
    codesign --force --sign - "$STAGED_APP"
else
    codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$STAGED_APP"
fi
codesign --verify --deep --strict "$STAGED_APP"

PREVIOUS_APP="$STAGING_DIR/Previous-NAKL.app"
if [ -e "$APP_DIR" ]; then
    mv "$APP_DIR" "$PREVIOUS_APP"
fi
if ! mv "$STAGED_APP" "$APP_DIR"; then
    if [ -e "$PREVIOUS_APP" ] && [ ! -e "$APP_DIR" ]; then
        mv "$PREVIOUS_APP" "$APP_DIR"
    fi
    exit 1
fi

echo "$APP_DIR"
