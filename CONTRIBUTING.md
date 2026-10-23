# Contributing to ViGo

Thanks for helping. ViGo is licensed under GPLv3; contributions are accepted under the same license.

## Reporting bugs

Use the issue templates. Include platform, ViGo version, typing method, the app where it happens, and the **minimal key sequence**. Never paste private text.

## Development

Read `CONTEXT.md` (domain terms) and `docs/design/core-interface.md` before changing the core or the C ABI.

```sh
cargo fmt --all --check
cargo clippy --workspace --all-targets -- -D warnings
cargo test --workspace
swift build --package-path platform/macos
swift format lint --recursive --strict platform/macos/Sources platform/macos/Tests
```

Work red → green through the public `CompositionSession::process` seam: add one behavioral slice at a time, and assert observable actions or replayed text. Record approved behavior in `tests/golden/product-expected.tsv` and unresolved semantics in `tests/golden/pending-review.tsv`.

## Pull requests

- Keep the subject imperative and concise, for example `Add Telex tone composition`.
- Fill in the PR template: milestone, behavior and ABI changes, commands you ran.
- Keep the core synchronous, deterministic, and free of platform, filesystem, network, UI, or logging dependencies.
- Never log or transmit keystroke content.

## Attribution

Ported rules and mapping data keep the GPLv3/xvnkb attribution described in `NOTICE`.
