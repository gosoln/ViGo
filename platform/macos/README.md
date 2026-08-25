# NAKL for macOS

This package contains the native SwiftUI menu-bar/settings UI and the InputMethodKit adapter. Each macOS text context owns a Rust composition session through the versioned C ABI. Telex/VNI selection, enable/disable, launch at login, and text expansions are persisted with `UserDefaults`.

Build the source or assemble an ad-hoc signed `.app` bundle on macOS:

```sh
cargo build -p nakl-core
swift build --package-path platform/macos
sh platform/macos/scripts/build-app.sh
```

The build script produces an ad-hoc signed `build/NAKL.app`; it does not install the input method. For a local manual test, copy that bundle into `~/Library/Input Methods`, log out and back in, then enable NAKL under System Settings > Keyboard > Text Input > Edit. Keep the source bundle closed before replacing an installed test build.

The adapter only consumes unmodified text key-downs. Command, Control, Option, navigation, unsupported document access, and other boundaries pass through without synthetic keyboard events. Production distribution still requires Developer ID signing and notarization.
