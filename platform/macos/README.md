# NAKL for macOS

This package contains the native SwiftUI menu-bar/settings UI and the InputMethodKit adapter. Each macOS text context owns a Rust composition session through the versioned C ABI. Telex/VNI selection, enable/disable, launch at login, and text expansions are persisted with `UserDefaults`.

On first launch, the app checks the legacy `com.zepvn.NAKL` preference domain and `~/Library/Application Support/NAKL/shortcuts.setting`. It migrates a valid typing method and bounded text-expansion records only when the corresponding new setting is absent. Current settings win, and legacy global hotkeys, excluded apps, and Accessibility state are intentionally left behind.

Build the source or assemble an ad-hoc signed `.app` bundle on macOS:

```sh
cargo build -p nakl-core
swift build --package-path platform/macos
rustup target add x86_64-apple-darwin
sh platform/macos/scripts/build-app.sh
```

The build script produces a universal arm64/x86_64 `build/NAKL.app`. It uses ad-hoc signing by default; set `NAKL_CODESIGN_IDENTITY` to use a release identity. Install a recoverable user-local test build with:

```sh
platform/macos/scripts/install-local.sh
# Log out and back in once.
platform/macos/scripts/input-source-tool.swift select
```

Keep the source bundle closed before upgrading an installed build. `uninstall-local.sh` restores ABC when NAKL is active and moves the bundle to Trash without deleting preferences. The compatibility fixture and required synthetic test cases are documented in [the macOS matrix](../../docs/testing/macos-compatibility.md).

Create an unsigned system-wide installer for structural testing with `platform/macos/scripts/package-app.sh`. A production package additionally requires `NAKL_CODESIGN_IDENTITY`, `NAKL_INSTALLER_IDENTITY`, and a notarytool keychain profile passed as `NAKL_NOTARY_PROFILE`; run `notarize.sh` and `verify-release.sh` before distribution. Disable “launch at login” in NAKL before uninstalling; `uninstall-system.sh` then disables the source, preserves a recovery bundle, and forgets the installer receipt.

The adapter only consumes unmodified text key-downs. Command, Control, Option, navigation, unsupported document access, and other boundaries pass through without synthetic keyboard events. Production distribution still requires Developer ID signing and notarization.
