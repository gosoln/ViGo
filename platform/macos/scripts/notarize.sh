#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <signed-package.pkg>" >&2
    exit 64
fi
if [ -z "${VIGO_NOTARY_PROFILE:-}" ]; then
    echo "Set VIGO_NOTARY_PROFILE to a notarytool keychain profile." >&2
    exit 64
fi

PACKAGE_PATH=$1
AUDIT_DIR="$(dirname -- "$PACKAGE_PATH")/notarization"
PACKAGE_NAME=$(basename -- "$PACKAGE_PATH" .pkg)
RESULT_PATH="$AUDIT_DIR/$PACKAGE_NAME-submit.json"
LOG_PATH="$AUDIT_DIR/$PACKAGE_NAME-log.json"
mkdir -p "$AUDIT_DIR"

pkgutil --check-signature "$PACKAGE_PATH"
xcrun notarytool submit "$PACKAGE_PATH" \
    --keychain-profile "$VIGO_NOTARY_PROFILE" \
    --wait --timeout 30m --output-format json > "$RESULT_PATH"
SUBMISSION_ID=$(plutil -extract id raw -o - "$RESULT_PATH")
xcrun notarytool log "$SUBMISSION_ID" "$LOG_PATH" \
    --keychain-profile "$VIGO_NOTARY_PROFILE"
xcrun stapler staple "$PACKAGE_PATH"
xcrun stapler validate "$PACKAGE_PATH"
spctl --assess --type install --verbose=2 "$PACKAGE_PATH"
