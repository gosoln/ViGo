# Composition Core Interface

## Confirmed Seam

The composition core is one deep Module. Its Interface is a per-context `CompositionSession` that accepts one normalized `InputEvent` and returns one observable `Action`:

```text
CompositionSession::process(InputEvent) -> Action
```

Rust callers and tests use this Interface directly. Native platform Adapters use the same seam through a versioned C ABI; the ABI mirrors domain values rather than exposing Rust layouts.

## Interface Contract

- Create one `CompositionSession` for each operating-system text context; state is never process-global.
- Capture immutable settings with `CompositionSession::with_config`; later configuration changes create or refresh a session instead of mutating one during processing.
- Native adapters may clone a session to predict an operating-system test-key result. A clone owns independent composition state; preview processing must never advance the live session, and only the offered key event is processed on the live session.
- `InputEvent` represents semantic text or a composition boundary, not a macOS, Windows, or Linux key code.
- A backspace event removes one Unicode scalar from session state and returns Pass-through Input so the native client performs the visible deletion.
- `Action::PassThrough` leaves the event unchanged.
- `Action::ReplaceBefore` consumes the event, deletes a bounded number of Unicode scalars immediately before the cursor, and inserts Canonical Output.
- A boundary resets the session before returning Pass-through Input.
- The caller applies each returned action exactly once and in order.
- Processing is deterministic, synchronous, panic-contained, and free of filesystem, network, logging, UI, or platform calls.

## Test Surface

Behavioral, property, fuzz, and FFI tests cross only this seam. Tests may observe returned actions and replay them into a string, but may not inspect buffers, tables, parser phases, or private helpers. The legacy Objective-C engine supplies characterization fixtures; reviewed product fixtures remain the authority for NAKL 2.0 behavior.

## Outside the Core

Platform Adapters own input-source registration, focus and secure-field lifecycle, marked-text APIs, application identity, installation, and configuration snapshots. Persistence and preference migration sit behind the platform layer; the core receives immutable configuration values when a session is created or refreshed.
