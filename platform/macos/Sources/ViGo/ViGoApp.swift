import AppKit
import InputMethodKit
import SwiftUI

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
  private let accessibility = AccessibilityAuthorization.shared
  private let settings = SettingsStore.shared
  private var directInputTap: DirectInputTap?
  private var inputMethodServer: IMKServer?
  private var runtimeStatusPollTimer: Timer?

  func applicationDidFinishLaunching(_ notification: Notification) {
    let connectionName =
      Bundle.main.object(forInfoDictionaryKey: "InputMethodConnectionName")
      as? String ?? "com.gosoln.inputmethod.ViGo_Connection"
    guard let bundleIdentifier = Bundle.main.bundleIdentifier else {
      preconditionFailure("ViGo requires a bundle identifier")
    }
    inputMethodServer = IMKServer(
      name: connectionName,
      bundleIdentifier: bundleIdentifier
    )

    let directInputTap = DirectInputTap { [weak settings] action, application in
      Task { @MainActor in
        settings?.perform(action, currentApplication: application)
      }
    }
    self.directInputTap = directInputTap
    accessibility.refresh()
    if !accessibility.isGranted {
      accessibility.request()
    }
    refreshRuntimeStatus()
    runtimeStatusPollTimer = Timer.scheduledTimer(
      timeInterval: 1,
      target: self,
      selector: #selector(pollRuntimeStatus(_:)),
      userInfo: nil,
      repeats: true
    )
  }

  func applicationWillTerminate(_ notification: Notification) {
    runtimeStatusPollTimer?.invalidate()
    runtimeStatusPollTimer = nil
    directInputTap?.stop()
  }

  @objc private func pollRuntimeStatus(_ timer: Timer) {
    refreshRuntimeStatus()
  }

  private func refreshRuntimeStatus() {
    accessibility.refresh()
    if accessibility.isGranted {
      _ = directInputTap?.startIfPermitted()
    } else {
      directInputTap?.stop()
    }
  }
}

@main
struct ViGoApp: App {
  @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
  @StateObject private var accessibility = AccessibilityAuthorization.shared
  @StateObject private var activeApplication = ActiveApplicationMonitor()
  @StateObject private var settings = SettingsStore.shared

  var body: some Scene {
    MenuBarExtra {
      StatusMenu(
        accessibility: accessibility,
        activeApplicationMonitor: activeApplication,
        settings: settings
      )
    } label: {
      menuBarLabel
    }
    .menuBarExtraStyle(.menu)

    Settings {
      SettingsView(accessibility: accessibility, settings: settings)
    }
  }

  private var menuBarImageName: String {
    let excluded =
      activeApplication.activeApplication.map {
        settings.isExcluded(bundleIdentifier: $0.bundleIdentifier)
      } ?? false
    return MenuBarImage.name(method: settings.method, isActive: settings.isEnabled && !excluded)
  }

  @ViewBuilder
  private var menuBarLabel: some View {
    if !accessibility.isGranted {
      Image(systemName: "exclamationmark.triangle")
    } else if let image = MenuBarImage.load(name: menuBarImageName) {
      Image(nsImage: image)
    } else {
      Image(systemName: settings.isEnabled ? "keyboard.fill" : "keyboard")
    }
  }
}

/// Template images bundled by `scripts/build-app.sh`; absent when running via `swift run`.
enum MenuBarImage {
  static func name(method: TypingMethod, isActive: Bool) -> String {
    guard isActive else { return "ViGoMenuBarOff" }
    switch method {
    case .telex: return "ViGoMenuBarTelex"
    case .vni: return "ViGoMenuBarVNI"
    }
  }

  static func load(name: String, bundle: Bundle = .main) -> NSImage? {
    guard let url = bundle.url(forResource: name, withExtension: "png"),
      let image = NSImage(contentsOf: url)
    else { return nil }
    image.size = NSSize(width: 26, height: 18)
    image.isTemplate = true
    return image
  }
}

@MainActor
struct SettingsWindowPresenter {
  private let activateApplication: () -> Void

  init(
    activateApplication: @escaping () -> Void = {
      NSApp.activate(ignoringOtherApps: true)
    }
  ) {
    self.activateApplication = activateApplication
  }

  func present(_ openSettings: () -> Void) {
    activateApplication()
    openSettings()
  }
}

@available(macOS 14.0, *)
private struct ModernSettingsButton: View {
  @Environment(\.openSettings) private var openSettings

  var body: some View {
    Button {
      SettingsWindowPresenter().present {
        openSettings()
      }
    } label: {
      Label("Tùy chọn…", systemImage: "gearshape")
    }
  }
}

private struct StatusMenu: View {
  @ObservedObject var accessibility: AccessibilityAuthorization
  @ObservedObject var activeApplicationMonitor: ActiveApplicationMonitor
  @ObservedObject var settings: SettingsStore

  var body: some View {
    if accessibility.isGranted {
      Label("Đang hoạt động", systemImage: "checkmark.circle")
    } else {
      Button {
        accessibility.openSystemSettings()
      } label: {
        Label("Cần cấp quyền Accessibility…", systemImage: "exclamationmark.triangle")
      }
    }

    Divider()

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

    // Exactly one of Telex, VNI, or Tắt is always checked so the menu keeps the same
    // checkmark column (and text alignment) whether typing is on or off.
    Button {
      settings.isEnabled = false
    } label: {
      if settings.isEnabled {
        Text("Tắt")
      } else {
        Label("Tắt", systemImage: "checkmark")
      }
    }

    if let activeApplication = activeApplicationMonitor.activeApplication {
      Button {
        settings.toggleExclusion(activeApplication)
      } label: {
        if settings.isExcluded(bundleIdentifier: activeApplication.bundleIdentifier) {
          Text("Bật trong \(activeApplication.displayName)")
        } else {
          Text("Tắt trong \(activeApplication.displayName)")
        }
      }
    }

    Divider()

    if #available(macOS 14.0, *) {
      ModernSettingsButton()
    } else {
      Button {
        SettingsWindowPresenter().present {
          NSApp.sendAction(Selector(("showSettingsWindow:")), to: nil, from: nil)
        }
      } label: {
        Label("Tùy chọn…", systemImage: "gearshape")
      }
    }

    Divider()

    Button("Thoát ViGo") {
      NSApp.terminate(nil)
    }
    .keyboardShortcut("q")
  }
}
