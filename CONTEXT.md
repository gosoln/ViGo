# Vietnamese System Input Method

This context defines the language used for a privacy-preserving Vietnamese input method that behaves consistently across desktop operating systems.

## Language

**System Input Method (Bộ gõ hệ thống)**:
An operating-system input service that composes Vietnamese text in any compatible application.
_Avoid_: Keyboard hook, background typer, keyboard app

**Typing Method (Kiểu gõ)**:
A convention that maps ordinary key sequences to Vietnamese letters, diacritics, and tone marks.
_Avoid_: Keyboard layout, input language

**Composition Session (Phiên ghép chữ)**:
A contiguous sequence of keystrokes and provisional Vietnamese text within one active text context.
_Avoid_: Global key stream, application session

**Text Expansion (Từ gõ tắt)**:
A user-defined trigger that expands into a replacement text value during input.
_Avoid_: Keyboard shortcut, hotkey

**Excluded Application (Ứng dụng loại trừ)**:
An application in which Vietnamese composition is intentionally bypassed while the System Input Method remains enabled.
_Avoid_: Blocked application, unsupported application

**Behavioral Parity (Tương thích hành vi)**:
The guarantee that the same intentional input sequence produces the same Unicode text and user-visible control behavior across supported platforms, excluding confirmed legacy defects.
_Avoid_: Bug-for-bug compatibility, source compatibility

**Canonical Output (Đầu ra chuẩn)**:
Committed Vietnamese text represented in Unicode Normalization Form C (NFC).
_Avoid_: Decomposed output, legacy byte encoding

**Secure Text Context (Ngữ cảnh văn bản bảo mật)**:
An operating-system-identified text context intended for passwords or other secrets.
_Avoid_: Excluded Application, unsupported application

**Pass-through Input (Nhập nguyên trạng)**:
An input event delivered without Vietnamese composition or replacement.
_Avoid_: Failed input, discarded input

**Keystroke Content (Nội dung phím gõ)**:
The characters and key sequences processed to compose user text; this is sensitive user data.
_Avoid_: Telemetry, diagnostic metadata
