# Linux Compatibility Matrix

Record only NAKL test phrases. Never capture unrelated Keystroke Content. A compile or fake-client
PASS is not evidence for a desktop/client row.

| ID | Gate | Required evidence | Status |
| --- | --- | --- | --- |
| L1 | Ubuntu 24.04 x86_64 build | IBus 1.5.29 and Fcitx5 5.1.7 compile with warnings denied | PASS |
| L2 | Shared adapter contract | R1–R5, VNI, expansion, reset, Delete→Commit, stale-caret rejection, no preedit API | PASS |
| L3 | Debian packages | Separate `nakl-ibus`/`nakl-fcitx5` packages inspect, install, and uninstall cleanly | PASS |
| L4 | Fedora 44 x86_64 build/RPM | Native compile, contract tests, RPM inspection/install/uninstall | PASS |
| L5 | IBus GNOME Wayland | GTK3/GTK4/Electron, R1–R5, no underline, suggestions retained | BLOCKED |
| L6 | IBus X11 | GTK3/GTK4/Electron, R1–R5 | BLOCKED |
| L7 | Fcitx5 Wayland | Qt5/Qt6/Electron, R1–R5, no underline | BLOCKED |
| L8 | Fcitx5 X11 | Qt5/Qt6/Electron, R1–R5 | BLOCKED |
| L9 | Secure contexts | Password/sensitive/disabled fields pass through and reset | PARTIAL |
| L10 | Session isolation | Two fields/apps cannot share stale composition | PARTIAL |
| L11 | Lifecycle | Clean install, upgrade, rollback, uninstall, login activation | BLOCKED |
| L12 | Performance/privacy | Core p99 <1 ms; no key content in files, logs, or network | PARTIAL |

Run `cargo test -p nakl-core --release --test performance -- --ignored --nocapture` for the core
latency gate. IBus direct-Wayland clients require IBus 1.5.34+ validation because upstream added
delete-surrounding and input-purpose wiring after Ubuntu 24.04's baseline.

`PARTIAL` means the adapter guard and shared contract are present, but a real desktop/client result
has not yet been recorded. It must not be promoted to `PASS` from a fake client or source review.

Recorded on 2026-08-26: Fedora 44 x86_64 built both adapters with GCC 16.2 against IBus 1.5.34 and
Fcitx5 5.1.21, passed the shared contract, and produced separate GPL-3.0-or-later RPMs. Package
inspection and install/uninstall left neither engine binary behind. The Fedora CI job repeats this
with a Fedora-built Release core; desktop behavior remains covered only by L5–L11.
