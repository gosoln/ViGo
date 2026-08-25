# Core Fuzzing

This package feeds arbitrary Unicode text and boundaries through the public composition seam for both Telex and VNI. It asserts that returned actions never delete before the simulated document start; Rust's `String` type also preserves valid UTF-8 while replaying every action.

Install the current `cargo-fuzz` release and a minimal nightly toolchain, then run a bounded local smoke session:

```sh
cargo install cargo-fuzz --version 0.13.2 --locked
rustup toolchain install nightly --profile minimal
cargo +nightly fuzz run composition --fuzz-dir fuzz -- -max_total_time=10
```

Generated corpus, artifacts, and fuzz build output are ignored. Commit only small, reviewed regression inputs that reproduce a fixed defect.
