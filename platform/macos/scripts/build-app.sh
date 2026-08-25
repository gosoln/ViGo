#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPOSITORY_DIR=$(CDPATH= cd -- "$PACKAGE_DIR/../.." && pwd)
APP_DIR="$REPOSITORY_DIR/build/NAKL.app"

swift build --package-path "$PACKAGE_DIR" -c release
BIN_DIR=$(swift build --package-path "$PACKAGE_DIR" -c release --show-bin-path)

rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS" "$APP_DIR/Contents/Resources"
cp "$BIN_DIR/NAKL" "$APP_DIR/Contents/MacOS/NAKL"
cp "$PACKAGE_DIR/Resources/Info.plist" "$APP_DIR/Contents/Info.plist"
codesign --force --sign - "$APP_DIR"

echo "$APP_DIR"
