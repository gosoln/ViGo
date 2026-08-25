# Repository Guidelines

## Project Structure & Module Organization

`core/` is the Rust composition Module and versioned C interface; public behavior is tested in `core/tests/`. Reviewed Telex/VNI fixtures live in `tests/golden/`. `legacy/oracle/` drives the frozen Objective-C `KeyboardHandler` under `legacy/macos-hook/` only to capture characterization data. The adjacent Xcode project is historical reference code, not the new macOS adapter.

Read `CONTEXT.md` before naming domain concepts. Read `docs/adr/` and `docs/design/core-interface.md` before changing the core seam or C ABI. `docs/modernization-plan.md` owns scope, platform order, and release gates.

## Build, Test, and Development Commands

- `cargo test --workspace` runs Rust behavioral and FFI tests with the pinned toolchain.
- `cargo fmt --all --check` verifies Rust formatting.
- `cargo clippy --workspace --all-targets -- -D warnings` treats Rust diagnostics as failures.
- `cargo +nightly fuzz run composition --fuzz-dir fuzz -- -max_total_time=10` runs a bounded coverage-guided core safety campaign.
- `make -C legacy/oracle` builds `build/legacy-oracle`; for example, run `build/legacy-oracle telex tieengs`.
- `cargo build -p nakl-core` emits the Rust library. Compile `core/tests/ffi_smoke.c` against `core/include/nakl_core.h` when changing the ABI.

The checked-in Xcode app still has legacy architecture and macOS 10.5 XIB metadata. Treat it as a behavioral oracle, not the production base.

## Coding Style & Naming Conventions

Use `rustfmt` defaults and descriptive domain names such as `CompositionSession`, `InputEvent`, and `Action`. Keep the core synchronous, deterministic, and free of platform, filesystem, network, UI, or logging dependencies. Maintain one session per text context; keep process-global composition state out of new code. Preserve four-space Objective-C style in project-owned legacy files and avoid reformatting bundled code.

## Testing Guidelines

Work red → green through the public `CompositionSession::process` seam. Add one behavioral slice at a time; assert observable actions or replayed text, never private buffers or helper calls. Record unchanged legacy output in `legacy-characterization.tsv`, approved NAKL 2.0 behavior in `product-expected.tsv`, and unresolved semantics in `pending-review.tsv`. Keep C ABI ownership and UTF-8 lifetime covered by integration tests.

## Commit & Pull Request Guidelines

History has no established format beyond `init`. Use concise imperative subjects, such as `Add Telex tone composition`. PRs should name the milestone, summarize behavior and ABI changes, report test/lint commands, link relevant ADRs or issues, and include UI screenshots only for platform-interface changes.

## Privacy & Licensing

Keep keystroke content on-device and absent from logs or telemetry. Preserve GPLv3/xvnkb attribution for ported rules and mapping data.
