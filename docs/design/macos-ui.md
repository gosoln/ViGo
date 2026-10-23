# macOS UI

ViGo is a menu-bar utility with no default Dock icon or main document window. The menu keeps only frequent actions: choose Telex or VNI, enable or disable typing, open settings, and quit.

The 620×400 settings window uses native controls and three tabs:

- **Tùy chọn** controls enablement, typing method, launch at login, the excluded-application list, and three recordable global hot keys: toggle typing, switch typing method, and toggle typing for the application currently in front (which adds or removes it from the excluded-application list). A hot key needs a main key plus at least one of Control, Option, or Command (for example ⌃⇧Space).
- **Gõ tắt** edits locally persisted shortcut/replacement pairs in a compact list.
- **Giới thiệu** shows product identity, version, privacy, license, and source link.

Avoid dashboards, sidebars, decorative cards, custom control skins, and settings that do not yet work. Platform code owns persistence and converts saved values into an immutable core configuration snapshot. Typed content never appears in settings storage, logs, or analytics.

The same app bundle hosts `IMKServer`; InputMethodKit creates one controller and Rust core session per text context. Controllers consume plain text key-downs and use the client's replacement range for conversions. Command, Control, Option, unsupported document access, and composition boundaries remain native pass-through events.
