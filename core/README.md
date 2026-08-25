# NAKL Composition Core

`nakl-core` is the platform-independent composition Module for NAKL 2.0. One `CompositionSession` belongs to one operating-system text context and exposes a single processing seam:

```text
CompositionSession::process(InputEvent) -> Action
```

The core currently covers the reviewed M0 Telex/VNI corpus, session reset, repeated-modifier behavior, Unicode NFC lookup tables, immutable Rust-side session configuration, exact text expansion on space, and the initial version-1 C ABI. It remains an M0 prototype: platform adapters, complete orthographic policy, configuration transport through the C ABI, fuzzing, and release hardening are still pending.

## Verification

```sh
cargo test -p nakl-core
cargo clippy -p nakl-core --all-targets -- -D warnings
cargo build -p nakl-core
xcrun clang -std=c11 -Wall -Wextra -Werror -Icore/include \
  core/tests/ffi_smoke.c target/debug/libnakl_core.a -o build/ffi-smoke
./build/ffi-smoke
```

Native adapters include [`nakl_core.h`](include/nakl_core.h). Replacement UTF-8 returned through `NaklAction` is borrowed from its session and remains valid until that session's next process call or release.
