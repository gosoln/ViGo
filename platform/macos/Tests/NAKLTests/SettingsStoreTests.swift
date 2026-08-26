import Foundation
import Testing

@testable import NAKL

@Test("Settings persist between store instances")
@MainActor
func settingsPersistBetweenStoreInstances() throws {
  let suiteName = "SettingsStoreTests.\(UUID().uuidString)"
  let defaults = try #require(UserDefaults(suiteName: suiteName))
  defer { defaults.removePersistentDomain(forName: suiteName) }

  let first = SettingsStore(defaults: defaults)
  first.isEnabled = false
  first.method = .vni
  first.addExpansion()
  first.expansions[0].shortcut = "dc"
  first.expansions[0].replacement = "được"

  let restored = SettingsStore(defaults: defaults)
  #expect(restored.isEnabled == false)
  #expect(restored.method == .vni)
  #expect(restored.expansions.count == 1)
  #expect(restored.expansions[0].shortcut == "dc")
  #expect(restored.expansions[0].replacement == "được")
}

@Test("Accessibility status refreshes and opens the correct system pane")
@MainActor
func accessibilityStatusRefreshesAndOpensSystemSettings() {
  var isGranted = false
  var openedURL: URL?
  let authorization = AccessibilityAuthorization(
    checkAuthorization: { isGranted },
    requestSystemAuthorization: {
      isGranted = true
      return true
    },
    openURL: { openedURL = $0 }
  )

  #expect(!authorization.isGranted)
  authorization.request()
  #expect(authorization.isGranted)

  authorization.openSystemSettings()
  #expect(openedURL == AccessibilityAuthorization.systemSettingsURL)
}
