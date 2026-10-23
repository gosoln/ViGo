# ViGo for macOS

This package contains the native SwiftUI menu-bar/settings UI and the InputMethodKit adapter. Each macOS text context owns a Rust composition session through the versioned C ABI. Telex/VNI selection, enable/disable, launch at login, and text expansions are persisted with `UserDefaults`.

On first launch, the app checks the legacy `com.zepvn.NAKL` preference domain and `~/Library/Application Support/NAKL/shortcuts.setting`. It migrates a valid typing method and bounded text-expansion records only when the corresponding new setting is absent. Current settings win, and legacy global hotkeys, excluded apps, and Accessibility state are intentionally left behind.

Build the source or assemble an ad-hoc signed `.app` bundle on macOS:

```sh
cargo build -p vigo-core
swift build --package-path platform/macos
rustup target add x86_64-apple-darwin
sh platform/macos/scripts/build-app.sh
```

The build script produces a universal arm64/x86_64 `build/ViGo.app`. It uses ad-hoc signing by default; set `VIGO_CODESIGN_IDENTITY` to use a release identity. Install a recoverable user-local test build with:

```sh
platform/macos/scripts/install-local.sh
```

The installer registers and launches the menu-bar app while keeping ABC selected. Keep ABC as the
macOS input source for underline-free committed input; do not select the transitional ViGo
InputMethodKit source for normal typing. Keep the source bundle closed before upgrading an installed
build. `uninstall-local.sh` restores ABC when ViGo is active and moves the bundle to Trash without
deleting preferences. The compatibility fixture and required synthetic test cases are documented in
[the macOS matrix](../../docs/testing/macos-compatibility.md).

After a clean login, select Telex in ViGo and run `platform/macos/scripts/smoke-installed.sh`. It launches the installed bundle, temporarily selects ABC, composes only the synthetic sequence `giair chaf` in an unsaved TextEdit document, checks for `giải chà`, closes the document without saving, and restores the previously selected input source. This covers committed tone relocation and replacement without relying on marked text.

Run `platform/macos/scripts/smoke-secure-input.sh` to exercise the direct-input security boundary. It types only synthetic data into the compatibility fixture and verifies raw pass-through plus session reset when entering and leaving a secure field.

Create an unsigned system-wide installer with `platform/macos/scripts/package-app.sh`, then validate it with `platform/macos/scripts/verify-package-structure.sh build/ViGo.app <package-path>`. This checks the universal payload, deployment target, identifiers, version, install domain, and logout requirement without treating ad-hoc signing as production-ready. A production package additionally requires `VIGO_CODESIGN_IDENTITY`, `VIGO_INSTALLER_IDENTITY`, and a notarytool keychain profile passed as `VIGO_NOTARY_PROFILE`; run `notarize.sh` and `verify-release.sh` before distribution. Disable “launch at login” in ViGo before uninstalling; `uninstall-system.sh` then disables the source, preserves a recovery bundle, and forgets the installer receipt.

Follow the complete [macOS release runbook](../../docs/release/macos-release-runbook.md) when preparing a signed release candidate.

The direct adapter consumes only unmodified text key-downs and emits synchronous committed replacements; command modifiers, navigation, mouse focus changes, excluded apps, and secure input reset composition and pass through. The transitional InputMethodKit source continues to expose its active word as marked text. Production distribution still requires Developer ID signing and notarization.
