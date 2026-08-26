# Cross-Platform Composition Regressions

Every macOS, Windows TSF, IBus, and Fcitx5 adapter must replay these cases through a fresh Composition Session. Final text must be NFC and must match `tests/golden/product-expected.tsv`; adapter tests must also assert the ordered replacement action so a correct core cannot be hidden by incorrect deletion or commit behavior.

| ID | Method and input | Expected | Forbidden regression |
| --- | --- | --- | --- |
| R1 | Telex `cha`, then `f` | `chà` | `chaà` |
| R2 | Telex `giai`, then `r` | `giải` | `giaỉ` |
| R3 | Telex `hai`, then `r` | `hải` | `haỉ` |
| R4 | Telex `hoai`, then `r` | `hoải` | `hoaỉ` |
| R5 | VNI `giai`, then `3` | `giải` | `giaỉ` |

For R2, the tone event must replace the two committed scalars `ai` with `ải`; replacing only the final `i` is a failure even if a later correction hides it.

Run R1–R5 in each platform's fake-client contract tests and in a Chromium address bar plus one native editor. Where the adapter uses committed-input mode, verify that typing has no marked-text underline and that client suggestions remain available. Record the OS, adapter, client version, architecture, NAKL commit, and PASS/FAIL/BLOCKED without capturing unrelated user text.
