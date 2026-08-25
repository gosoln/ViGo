# Frozen macOS Hook

This directory preserves the original NAKL menu-bar application as M0 reference code. The snapshot contains 78 previously tracked files: 76 application sources and assets, one Xcode project file, and one Accessibility helper script. Git records the move without content changes.

Do not use this application as the NAKL 2.0 macOS runtime. It relies on a global `CGEventTap`, Accessibility permission, and synthetic key events. The replacement belongs in `platform/macos/` and must use InputMethodKit.

`legacy/oracle/` compiles only `KeyboardHandler.m` with a test preference stub. Change frozen sources only when repairing the oracle itself, and accompany any intentional behavior change with reviewed characterization fixtures.

On macOS, verify that the snapshot remains readable and the oracle remains runnable:

```sh
xcodebuild -list -project legacy/macos-hook/NAKL.xcodeproj
make -C legacy/oracle
build/legacy-oracle telex tieengs
```
