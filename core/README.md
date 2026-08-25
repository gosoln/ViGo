# NAKL Composition Core

`nakl-core` is the platform-independent composition Module for NAKL 2.0. One `CompositionSession` belongs to one operating-system text context and exposes a single processing seam:

```text
CompositionSession::process(InputEvent) -> Action
```

The core covers the reviewed M0 Telex/VNI corpus, boundaries and backspace, repeated-modifier behavior, Unicode NFC lookup tables, immutable session configuration, exact text expansion on space, deterministic property-style safety coverage, coverage-guided composition and FFI fuzzers, and the version-1 C ABI. Native callers populate an opaque `NaklConfig`, create a session that copies its values, then release the builder. Platform adapters, wider compatibility testing, and release hardening begin in later milestones.

## Verification

```sh
cargo test -p nakl-core
cargo clippy -p nakl-core --all-targets -- -D warnings
cargo build -p nakl-core
xcrun clang -std=c11 -Wall -Wextra -Werror -Icore/include \
  core/tests/ffi_smoke.c target/debug/libnakl_core.a -o build/ffi-smoke
./build/ffi-smoke
```

Native adapters include [`nakl_core.h`](include/nakl_core.h). Config strings are copied, while replacement UTF-8 returned through `NaklAction` is borrowed from its session and remains valid until that session's next process call or release.
