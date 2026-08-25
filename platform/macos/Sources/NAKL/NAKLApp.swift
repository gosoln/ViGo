import AppKit
import SwiftUI

@main
struct NAKLApp: App {
  @StateObject private var settings = SettingsStore()

  var body: some Scene {
    MenuBarExtra(
      "NAKL",
      systemImage: settings.isEnabled ? "keyboard.fill" : "keyboard"
    ) {
      StatusMenu(settings: settings)
    }
    .menuBarExtraStyle(.menu)

    Settings {
      SettingsView(settings: settings)
    }
  }
}

private struct StatusMenu: View {
  @ObservedObject var settings: SettingsStore

  var body: some View {
    ForEach(TypingMethod.allCases) { method in
      Button {
        settings.method = method
        settings.isEnabled = true
      } label: {
        if settings.method == method, settings.isEnabled {
          Label(method.title, systemImage: "checkmark")
        } else {
          Text(method.title)
        }
      }
    }

    Button(settings.isEnabled ? "Tắt" : "Bật") {
      settings.isEnabled.toggle()
    }

    Divider()

    if #available(macOS 14.0, *) {
      SettingsLink {
        Label("Tùy chọn…", systemImage: "gearshape")
      }
    } else {
      Button {
        NSApp.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
        NSApp.activate(ignoringOtherApps: true)
      } label: {
        Label("Tùy chọn…", systemImage: "gearshape")
      }
    }

    Divider()

    Button("Thoát NAKL") {
      NSApp.terminate(nil)
    }
    .keyboardShortcut("q")
  }
}
