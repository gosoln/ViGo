# NAKL for Linux

The Linux platform uses committed edits through native IBus and Fcitx5 contracts. Both adapters
share one C++ composition module over the Rust C ABI, keep one session per input context, delete
surrounding text before committing NFC UTF-8, and never expose a preedit/marked-text surface.

## Build

Ubuntu 24.04 dependencies:

```sh
sudo apt install build-essential cmake ninja-build pkg-config \
  libibus-1.0-dev libfcitx5core-dev libfcitx5config-dev \
  libfcitx5utils-dev extra-cmake-modules
cargo build -p nakl-core --release
cmake -S platform/linux -B build/linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DNAKL_BUILD_IBUS=ON -DNAKL_BUILD_FCITX5=ON -DBUILD_TESTING=ON
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
```

Use `platform/linux/scripts/build-release.sh`; CPack emits separate `nakl-ibus` and
`nakl-fcitx5` packages. Select **NAKL Telex** or **NAKL VNI** in the desktop input-method tool.
After a local install, run `ibus restart` or `fcitx5 -rd`; packages do not kill a user's daemon.

Adapters load an immutable settings snapshot from `$XDG_CONFIG_HOME/nakl/settings.conf` (or
`~/.config/nakl/settings.conf`) on focus/activation. The bounded v1 format is line-oriented:

```text
version=1
enabled=true
expansion=btw	by the way
excluded=code
```

Fcitx5 applies executable exclusions through `InputContext::program()`. IBus client identity is not
consistent across GTK, Qt, and Wayland; NAKL accepts both the full identity and the segment after a
frontend prefix such as `gtk3-im:code`. Other identity forms remain a compatibility gate. P2 uses
this explicit settings file; a shared Linux tray/settings UI remains an M4 decision.

## Compatibility rules

Password, sensitive, and input-disabled contexts reset composition and receive pass-through input.
Clients without native surrounding-text deletion also fail closed: NAKL passes the modifier through
instead of synthesizing Backspace or risking unrelated text. IBus direct-Wayland deletion requires
runtime validation with IBus 1.5.34 or newer; Ubuntu 24.04's 1.5.29 remains a build baseline only.

See the [Linux release runbook](../../docs/release/linux-release-runbook.md) and
[compatibility matrix](../../docs/testing/linux-compatibility.md) before marking a package releasable.
