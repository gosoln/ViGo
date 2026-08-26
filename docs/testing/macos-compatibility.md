# macOS Compatibility Matrix

Use only the synthetic strings below in fresh, unsaved documents. Record the test ID, PASS/FAIL/BLOCKED, application version, macOS build, and NAKL commit. Never record arbitrary typed text, secure-field contents, Console logs, or network traces.

## Activation Preflight

After the first local install, log out and back in. Keep `/Applications/NAKL.app` (legacy 0.2) closed, then run:

```sh
platform/macos/scripts/smoke-installed.sh
platform/macos/scripts/smoke-secure-input.sh
```

The smoke tests keep ABC selected, validate the committed-input regressions `giai` + `r` → `giải` and `cha` + `f` → `chà`, and confirm that a secure field receives seven raw characters without carrying composition state across either focus transition. To exercise the transitional InputMethodKit path separately, run:

```sh
platform/macos/scripts/input-source-tool.swift select
swift run --package-path platform/macos NAKLCompatibilityFixture
```

Restore ABC after testing with `platform/macos/scripts/input-source-tool.swift restore-abc`.

For committed-input compatibility, launch NAKL, grant Accessibility, keep ABC selected, and run the shared cases in [Cross-Platform Composition Regressions](./cross-platform-composition-regressions.md). Do not select the NAKL InputMethodKit source for those cases.

## Required Cases

| ID | Input/action | Expected result |
| --- | --- | --- |
| C1 | Telex `tieengs` | `tiếng` in NFC |
| C2 | Telex `uow` | `ươ` |
| C3 | Telex `TIEENGS` | `TIẾNG` |
| C4 | VNI `tie6ng1` | `tiếng` |
| C5 | `tieengs`, Backspace, `f` | `tiền` |
| C6 | Expansion `btw` → `by the way`, then `btw ` | `by the way ` |
| C7 | `xa`, Left Arrow, `a` | Literal `xaa`; no remote replacement |
| C8 | In `LtestR`, select only `test`, then type `as` | `LáR`; surrounding text is preserved |
| C9 | `a`, Command+A, Right Arrow, `s` | Literal `as`; stale composition is not reused |
| C10 | Type `a` in field A, focus field B, type `s` | A is `a`; B is `s` |
| C11 | Disable NAKL, type `tieengs`, then re-enable | Literal while disabled; C1 after re-enable |
| C12 | Type `tieengs` in a secure field | Seven raw characters; no crash or replacement |
| C13 | With ABC selected, Telex `giai`, then `r` | `giải`; never `giaỉ` |
| C14 | With ABC selected, Telex `cha`, then `f` | `chà`; never `chaà` |
| C15 | Type C13 in the Chrome address bar | No marked-text underline; suggestions remain visible |
| C16 | Choose “Tắt trong Google Chrome”, type `tieengs`, then re-enable | Literal while excluded; C1 after re-enable |
| C17 | Record global hot keys, trigger each from Chrome, then clear them | Toggle changes enabled state; switch alternates Telex/VNI; keys do not enter Chrome |

Run C1–C12 in the AppKit fixture first, switching the global typing method when its section requests it. Repeat C12 and run C13–C17 through committed-input compatibility with ABC selected. A green mark requires both the expected text and NFC. Then cover TextEdit, Chrome, VS Code, PhpStorm, and Word; repeat C1, C5, C7, C10, C12, C13, and C14 in each applicable client. Use local/offline fields in browser and Electron clients, and discard documents without saving.

Record completed runs in [macOS compatibility results](./macos-compatibility-results.md). Treat unavailable licenses, unfinished first-run setup, or missing release credentials as BLOCKED rather than PASS.
