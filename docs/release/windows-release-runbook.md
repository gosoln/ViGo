# Windows Release Runbook

## Status and Scope

P1 targets Windows 11 x64 with a native TSF DLL, separate tray/settings executable, and per-machine
MSI. This runbook records the remaining human/Windows-only release gates. Do not mark P1 complete
from a macOS host build.

## Prerequisites

- Windows 11 x64 VM snapshots for clean install, upgrade, rollback, and uninstall.
- Visual Studio x64 C++ tools, Rust stable, CMake, .NET SDK, `signtool`, and WiX 7.0.0.
- A trusted code-signing certificate available without exporting its private key.
- Organizational review of the [WiX OSMF EULA](https://docs.firegiant.com/wix/osmf/). Pass
  `-AcceptWix7Eula` only after its fee/acceptance requirements are satisfied.

## Build and Sign

1. Run all repository gates and create the native Release outputs:

   ```powershell
   platform/windows/scripts/build-release.ps1 -AcceptWix7Eula
   ```

2. Before rebuilding the MSI, sign `NAKLTSF.dll` and `NAKLSettings.exe` with SHA-256 plus a trusted
   RFC 3161 timestamp. Verify each with `signtool verify /pa /all /v <file>`.
3. Rebuild the MSI so it contains those signed binaries, then sign and verify the MSI itself.
4. Archive the commit, Rust/Visual Studio/WiX versions, certificate subject, timestamp URL, SHA-256
   hashes, and build log. Never archive certificate secrets.

## Lifecycle Verification

On disposable VM snapshots, capture verbose `msiexec /l*v` logs for:

- clean install and sign-out/sign-in activation;
- repair with no setting loss;
- upgrade from the previous signed MSI;
- forced failure proving rollback restores the prior profile/files;
- uninstall proving the TSF profile, COM class, installed files, shortcut, and current-user startup
  entry are removed while `HKCU\Software\NAKL` preferences remain.

Applications may retain the in-process DLL until sign-out. Never overwrite or delete a loaded TSF
DLL manually.

## Compatibility and Release Decision

Record W1–W12 from the [Windows compatibility matrix](../testing/windows-compatibility.md) in
Notepad, Word, WinUI, Chrome, and an app-container client. Confirm password fields are raw
pass-through, settings take effect after a context transition, and no network or Keystroke Content
logging occurs. P1 is releasable only when every required case is PASS and the produced hashes match
the signed artifacts; otherwise retain **IN PROGRESS** with the exact blocker.
