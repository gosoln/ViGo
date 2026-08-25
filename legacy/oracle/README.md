# Legacy Behavioral Oracle

This command-line harness drives the existing `KeyboardHandler` through its public methods without the menu-bar application or global event tap. It exists only to capture reviewable characterization fixtures before the Objective-C code is frozen.

Build and run on macOS:

```sh
make -C legacy/oracle
build/legacy-oracle telex tieengs
build/legacy-oracle vni tie6ng1
```

The oracle intentionally preserves legacy behavior, including defects. Its output must be reviewed before copying a case into `tests/golden/product-expected.tsv`.
