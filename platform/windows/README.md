# NAKL for Windows

This directory contains the Windows 11 x64 Text Services Framework adapter. The deep `CompositionSession` module hides UTF-8/UTF-16 conversion and the Rust C ABI behind `Process` and `Preview`; the TSF implementation owns COM lifetime, focus identity, secure-mode pass-through, key normalization, synchronous edit sessions, and profile registration.

## Current Scope

The first M2 slice provides:

- a native in-process TSF DLL and COM class factory;
- Vietnamese language-profile/category registration without setting a default IME;
- one active composition session per focused TSF context;
- Telex key processing, secure-mode pass-through, and ordered committed replacements;
- fake-editor contract coverage for shared regressions R1–R5, including VNI at the core seam.

Settings persistence, runtime Telex/VNI switching, text expansions, excluded applications, system-tray settings UI, signed MSI lifecycle, and real-client certification remain future M2 slices.

## Build and Test on Windows 11 x64

Use a Visual Studio Developer PowerShell from the repository root:

```powershell
cargo build -p nakl-core
cmake -S platform/windows -B build/windows -A x64 -DBUILD_TESTING=ON
cmake --build build/windows --config Debug
ctest --test-dir build/windows -C Debug --output-on-failure
platform/windows/scripts/verify-build.ps1
```

The DLL is `build/windows/Debug/NAKLTSF.dll`. The Rust core is statically linked; the TSF key path performs no filesystem, network, UI, telemetry, or logging work.

## Development Registration

Windows requires third-party IMEs to be digitally signed. After applying an appropriate development signature, open an elevated **x64** PowerShell:

```powershell
platform/windows/scripts/register-local.ps1
# Sign out/in, enable NAKL under Vietnamese language options, then test.
platform/windows/scripts/unregister-local.ps1
```

The registration script uses TSF profile/category interfaces and COM in-process registration. It does not write the default-keyboard registry state. Never replace a DLL while applications still have it loaded; unregister it and sign out first.

Use the required cases in [the Windows compatibility matrix](../../docs/testing/windows-compatibility.md). Only synthetic strings may be recorded during testing.
