import AppKit
import ApplicationServices
import Combine

@MainActor
final class AccessibilityAuthorization: ObservableObject {
  static let shared = AccessibilityAuthorization()
  static let systemSettingsURL = URL(
    string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"
  )!

  @Published private(set) var isGranted: Bool

  private let checkAuthorization: () -> Bool
  private let requestSystemAuthorization: () -> Bool
  private let openURL: (URL) -> Void

  init(
    checkAuthorization: @escaping () -> Bool = { AXIsProcessTrusted() },
    requestSystemAuthorization: @escaping () -> Bool = {
      AXIsProcessTrustedWithOptions(
        ["AXTrustedCheckOptionPrompt": true] as CFDictionary
      )
    },
    openURL: @escaping (URL) -> Void = { _ = NSWorkspace.shared.open($0) }
  ) {
    self.checkAuthorization = checkAuthorization
    self.requestSystemAuthorization = requestSystemAuthorization
    self.openURL = openURL
    isGranted = checkAuthorization()
  }

  func refresh() {
    isGranted = checkAuthorization()
  }

  func request() {
    _ = requestSystemAuthorization()
    refresh()
  }

  func openSystemSettings() {
    openURL(Self.systemSettingsURL)
  }
}
