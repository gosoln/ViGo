# Linux Release Runbook

P2 targets Linux x86_64 with IBus for GNOME/Ubuntu and Fcitx5 for KDE/other deployments. Do not
mark P2 released until every required row in the Linux compatibility matrix is PASS.

## Build and inspect

1. Build from a clean signed tag on Ubuntu 24.04 and current Fedora using the pinned Rust toolchain.
2. Run `cargo fmt --all --check`, Clippy, workspace tests, the ignored release performance test, and
   `platform/linux/scripts/build-release.sh`.
3. Inspect `.deb` files with `dpkg-deb --info/--contents` and `.rpm` files with `rpm -qip/-qlp`.
   Confirm only the selected engine binary/addon, framework metadata, `LICENSE`, and `NOTICE` ship.
4. Publish SHA-256 checksums. Package signing follows the target repository's signing process.

## Install and activate

- Install `nakl-ibus` or `nakl-fcitx5`; do not require both frameworks.
- Restart manually with `ibus restart` or `fcitx5 -rd`, then add **NAKL Telex** or **NAKL VNI** in
  the desktop input-method configuration. Verify activation again after logout/login.
- Do not restart or kill a user's input daemon from package maintainer scripts.

## Behavioral and security gates

Replay R1–R5 in GTK3, GTK4, Qt5, Qt6, and Electron on Wayland and X11. For each committed edit,
verify deletion occurs before commit, no preedit underline appears, and suggestions remain usable.
Test password/sensitive/disabled fields, focus changes, cursor movement, two simultaneous contexts,
client crashes, and clients lacking surrounding-text deletion. Unsupported clients must pass through;
never fall back to synthetic Backspace.

## Lifecycle gates

Test clean install, same-version repair, upgrade, forced rollback, uninstall, and reinstall for both
package formats. Framework metadata must disappear on uninstall without removing unrelated user
configuration. Record distro/framework/client versions, display protocol, architecture, commit,
package hashes, and PASS/FAIL/BLOCKED in `docs/testing/linux-compatibility.md`.
