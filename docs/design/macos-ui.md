# macOS UI

NAKL is a menu-bar utility with no default Dock icon or main document window. The menu keeps only frequent actions: choose Telex or VNI, enable or disable typing, open settings, and quit.

The 620×400 settings window uses native controls and three tabs:

- **Tùy chọn** controls enablement, typing method, and launch at login.
- **Gõ tắt** edits locally persisted shortcut/replacement pairs in a compact list.
- **Giới thiệu** shows product identity, version, privacy, license, and source link.

Avoid dashboards, sidebars, decorative cards, custom control skins, and settings that do not yet work. Platform code owns persistence and converts saved values into an immutable core configuration snapshot. Typed content never appears in settings storage, logs, or analytics.
