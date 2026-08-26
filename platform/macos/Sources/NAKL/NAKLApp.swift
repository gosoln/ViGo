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
      as? String ?? "com.nakl.inputmethod.NAKL_Connection"
    guard let bundleIdentifier = Bundle.main.bundleIdentifier else {
      preconditionFailure("NAKL requires a bundle identifier")
    }
    inputMethodServer = IMKServer(
      name: connectionName,
      bundleIdentifier: bundleIdentifier
    )

    let directInputTap = DirectInputTap { [weak settings] action in
      Task { @MainActor in
        settings?.perform(action)
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
struct NAKLApp: App {
  @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
  @StateObject private var accessibility = AccessibilityAuthorization.shared
  @StateObject private var settings = SettingsStore.shared

  var body: some Scene {
    MenuBarExtra(
      "NAKL",
      systemImage: menuBarIcon
    ) {
      StatusMenu(accessibility: accessibility, settings: settings)
    }
    .menuBarExtraStyle(.menu)

    Settings {
      SettingsView(accessibility: accessibility, settings: settings)
    }
  }

  private var menuBarIcon: String {
    if !accessibility.isGranted { return "exclamationmark.triangle" }
    return settings.isEnabled ? "keyboard.fill" : "keyboard"
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
  @ObservedObject var settings: SettingsStore
  @State private var activeApplication: ExcludedApplication?

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

    Button(settings.isEnabled ? "Tắt" : "Bật") {
      settings.isEnabled.toggle()
    }

    if let activeApplication {
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

    Button("Thoát NAKL") {
      NSApp.terminate(nil)
    }
    .keyboardShortcut("q")
    .onAppear {
      let application = NSWorkspace.shared.frontmostApplication
      guard let bundleIdentifier = application?.bundleIdentifier,
        bundleIdentifier != Bundle.main.bundleIdentifier
      else {
        activeApplication = nil
        return
      }
      activeApplication = ExcludedApplication(
        bundleIdentifier: bundleIdentifier,
        displayName: application?.localizedName ?? bundleIdentifier
      )
    }
  }
}
