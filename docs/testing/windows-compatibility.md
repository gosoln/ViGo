# Windows Compatibility Matrix

Test on Windows 11 x64 with a signed NAKL TSF DLL registered under the Vietnamese language profile. Use fresh documents and only the synthetic strings below. Record Windows build, client version, architecture, NAKL commit, and PASS/FAIL/BLOCKED; never capture unrelated Keystroke Content or secure-field contents.

## Automated Contract

Run before system registration:

```powershell
cargo build -p nakl-core
cmake -S platform/windows -B build/windows -A x64 -DBUILD_TESTING=ON
cmake --build build/windows --config Debug
ctest --test-dir build/windows -C Debug --output-on-failure
```

The fake editor must pass R1–R6 from [Cross-Platform Composition Regressions](./cross-platform-composition-regressions.md), assert both deletion count and inserted UTF-16 text, and commit a configured Text Expansion on space.

## Required System Cases

| ID | Input/action | Expected result |
| --- | --- | --- |
| W1 | Telex `tieengs` | `tiếng` in NFC |
| W2 | Telex `giai`, then `r` | `giải`; ordered replacement deletes `ai`, never `giaỉ` |
| W3 | Telex `cha`, then `f` | `chà`; never `chaà` |
| W4 | VNI `giai`, then `3` | `giải` |
| W5 | Type `a` in field A, focus field B, type `s` | A is `a`; B is literal `s` |
| W6 | Type `tieengs` in a password field | Seven raw characters; no crash or replacement |
| W7 | Type C1, Backspace, then `f` | `tiền` |
| W8 | Expansion `btw` → `by the way`, then type `btw ` | `by the way ` |
| W9 | Disable NAKL and type `tieengs`, then re-enable | Literal while disabled; W1 after re-enable |
| W10 | Install, repair, upgrade, rollback, and uninstall MSI | Profile/receipt state is correct and settings survive supported transitions |
| W11 | Add the current `.exe` to exclusions, refocus, type `tieengs` | Literal input; W1 after removing the exclusion and refocusing |
| W12 | Invoke configured toggle and method-switch hot keys | Enabled state and Telex/VNI selection change once per press |
| W13 | Telex `v e r r s i o n` | `version`; never `vérion` |

Run W1–W9 and W11–W13 in Win32 Notepad, Microsoft Word, a WinUI text field, Chrome, and one app-container client. Repeat focus switching across two applications. Confirm the TSF DLL performs no network access and remains functional under app-container restrictions.

## Current Status

The portable adapter contract, including Text Expansion, passes on macOS host tooling. The native
DLL/settings/MSI sources and Windows CI gates are authored. Windows-native compile evidence,
signing, registration, real-client execution, and MSI lifecycle remain BLOCKED until run on a
Windows 11 x64 runner or machine. Do not mark W1–W10 PASS from portable tests.
