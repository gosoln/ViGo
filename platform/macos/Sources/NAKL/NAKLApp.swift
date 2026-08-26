import AppKit
import InputMethodKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
  private var accessibilityPollTimer: Timer?
  private var directInputTap: DirectInputTap?
  private var inputMethodServer: IMKServer?

  func applicationDidFinishLaunching(_ notification: Notification) {
    let connectionName =
      Bundle.main.object(forInfoDictionaryKey: "InputMethodConnectionName")
      as? String ?? "com.nakl.inputmethod.NAKL_Connection"
    guard let bundleIdentifier = Bundle.main.bundleIdentifier else {
      preconditionFailure("NAKL requires a bundle identifier")
    }
    inputMethodServer = IMKServer(
      name: connectionName,
      bundleIdentifier: bundleIdentifier
    )

    let directInputTap = DirectInputTap()
    self.directInputTap = directInputTap
    if !directInputTap.startIfPermitted() {
      _ = DirectInputTap.requestAccessibilityPermission()
      accessibilityPollTimer = Timer.scheduledTimer(
        timeInterval: 1,
        target: self,
        selector: #selector(startDirectInputWhenPermitted(_:)),
        userInfo: nil,
        repeats: true
      )
    }
  }

  @objc private func startDirectInputWhenPermitted(_ timer: Timer) {
    if directInputTap?.startIfPermitted() == true {
      timer.invalidate()
      accessibilityPollTimer = nil
    }
  }
}

@main
struct NAKLApp: App {
  @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
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
