# NAKL macOS UI Shell

This SwiftUI package is the minimal native menu-bar and settings shell for M1. It provides direct Telex/VNI selection, enable/disable, launch-at-login registration, locally persisted text expansions, About information, and quit. It does not contain the InputMethodKit adapter yet.

Build the source or assemble an ad-hoc signed `.app` bundle on macOS:

```sh
swift build --package-path platform/macos
sh platform/macos/scripts/build-app.sh
open build/NAKL.app
```

The bundle sets `LSUIElement` so NAKL appears in the menu bar without a Dock icon. Production signing, sandbox decisions, InputMethodKit embedding, and settings-to-core configuration transport remain M1 work.
