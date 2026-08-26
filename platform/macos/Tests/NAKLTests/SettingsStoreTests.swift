import Foundation
import Testing

@testable import NAKL

@Test("Active application follows foreground changes and ignores NAKL itself")
@MainActor
func activeApplicationFollowsForegroundChanges() {
  let monitor = ActiveApplicationMonitor(
    ownBundleIdentifier: "com.nakl.inputmethod.NAKL",
    observeWorkspace: false
  )

  monitor.recordActivation(
    bundleIdentifier: "com.apple.finder",
    displayName: "Finder"
  )
  #expect(monitor.activeApplication?.displayName == "Finder")

  monitor.recordActivation(
    bundleIdentifier: "com.google.Chrome",
    displayName: "Google Chrome"
  )
  #expect(monitor.activeApplication?.displayName == "Google Chrome")

  monitor.recordActivation(
    bundleIdentifier: "com.nakl.inputmethod.NAKL",
    displayName: "NAKL"
  )
  #expect(monitor.activeApplication?.displayName == "Google Chrome")
}

@Test("Settings menu opens its window and activates NAKL")
@MainActor
func settingsMenuOpensAndActivatesApplication() {
  var events: [String] = []
  let presenter = SettingsWindowPresenter(
    activateApplication: { events.append("activate") }
  )

  presenter.present {
    events.append("open")
  }

  #expect(events == ["activate", "open"])
}

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

@Test("Excluded applications persist and can be removed")
@MainActor
func excludedApplicationsPersistAndCanBeRemoved() throws {
  let suiteName = "ExcludedApplicationTests.\(UUID().uuidString)"
  let defaults = try #require(UserDefaults(suiteName: suiteName))
  defer { defaults.removePersistentDomain(forName: suiteName) }

  let application = ExcludedApplication(
    bundleIdentifier: "com.example.Editor",
    displayName: "Editor"
  )
  let first = SettingsStore(defaults: defaults)
  first.toggleExclusion(application)

  let restored = SettingsStore(defaults: defaults)
  #expect(restored.isExcluded(bundleIdentifier: application.bundleIdentifier))
  #expect(restored.excludedApplications == [application])

  restored.removeExcludedApplication(bundleIdentifier: application.bundleIdentifier)
  #expect(restored.excludedApplications.isEmpty)
}

@Test("Hot keys require modifiers, persist, and remain unique")
@MainActor
func hotKeysRequireModifiersPersistAndRemainUnique() throws {
  let suiteName = "HotKeyTests.\(UUID().uuidString)"
  let defaults = try #require(UserDefaults(suiteName: suiteName))
  defer { defaults.removePersistentDomain(forName: suiteName) }

  #expect(KeyboardShortcut(keyCode: 0, modifierFlags: .shift) == nil)
  let shortcut = try #require(
    KeyboardShortcut(keyCode: 49, modifierFlags: [.control, .option])
  )
  let first = SettingsStore(defaults: defaults)
  first.setShortcut(shortcut, for: .toggleInput)

  let restored = SettingsStore(defaults: defaults)
  #expect(restored.toggleInputShortcut == shortcut)
  #expect(shortcut.displayName == "⌃⌥Space")

  restored.setShortcut(shortcut, for: .switchMethod)
  #expect(restored.toggleInputShortcut == nil)
  #expect(restored.switchMethodShortcut == shortcut)
}
