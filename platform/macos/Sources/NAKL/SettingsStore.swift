import AppKit
import Combine
import Foundation
import ServiceManagement

extension Notification.Name {
  static let naklSettingsDidChange = Notification.Name("NAKLSettingsDidChange")
}

enum SettingsKey {
  static let enabled = "input.enabled"
  static let method = "input.method"
  static let expansions = "textExpansions"
  static let excludedApplications = "excludedApplications"
  static let toggleInputShortcut = "hotKey.toggleInput"
  static let switchMethodShortcut = "hotKey.switchMethod"
}

enum TypingMethod: String, CaseIterable, Codable, Identifiable {
  case telex
  case vni

  var id: Self { self }

  var title: String {
    switch self {
    case .telex: "Telex"
    case .vni: "VNI"
    }
  }
}

struct TextExpansion: Codable, Hashable, Identifiable {
  let id: UUID
  var shortcut: String
  var replacement: String

  init(id: UUID = UUID(), shortcut: String = "", replacement: String = "") {
    self.id = id
    self.shortcut = shortcut
    self.replacement = replacement
  }
}

struct ExcludedApplication: Codable, Equatable, Identifiable {
  let bundleIdentifier: String
  let displayName: String

  var id: String { bundleIdentifier }
}

enum HotKeyAction: Equatable {
  case toggleInput
  case switchMethod
}

struct KeyboardShortcut: Codable, Equatable {
  let keyCode: UInt16
  let modifierFlagsRawValue: UInt

  init?(keyCode: UInt16, modifierFlags: NSEvent.ModifierFlags) {
    let allowed: NSEvent.ModifierFlags = [.command, .control, .option, .shift]
    let normalized = modifierFlags.intersection(allowed)
    let required: NSEvent.ModifierFlags = [.command, .control, .option]
    guard !normalized.intersection(required).isEmpty else { return nil }
    self.keyCode = keyCode
    modifierFlagsRawValue = normalized.rawValue
  }

  var modifierFlags: NSEvent.ModifierFlags {
    NSEvent.ModifierFlags(rawValue: modifierFlagsRawValue)
  }

  var displayName: String {
    var result = ""
    if modifierFlags.contains(.control) { result += "⌃" }
    if modifierFlags.contains(.option) { result += "⌥" }
    if modifierFlags.contains(.shift) { result += "⇧" }
    if modifierFlags.contains(.command) { result += "⌘" }
    return result + Self.keyNames[keyCode, default: "Phím \(keyCode)"]
  }

  func matches(keyCode: UInt16, modifierFlags: NSEvent.ModifierFlags) -> Bool {
    guard let candidate = KeyboardShortcut(keyCode: keyCode, modifierFlags: modifierFlags) else {
      return false
    }
    return self == candidate
  }

  private static let keyNames: [UInt16: String] = [
    0: "A", 1: "S", 2: "D", 3: "F", 4: "H", 5: "G", 6: "Z", 7: "X",
    8: "C", 9: "V", 11: "B", 12: "Q", 13: "W", 14: "E", 15: "R", 16: "Y",
    17: "T", 18: "1", 19: "2", 20: "3", 21: "4", 22: "6", 23: "5", 24: "=",
    25: "9", 26: "7", 27: "-", 28: "8", 29: "0", 30: "]", 31: "O", 32: "U",
    33: "[", 34: "I", 35: "P", 36: "↩", 37: "L", 38: "J", 39: "'", 40: "K",
    41: ";", 42: "\\", 43: ",", 44: "/", 45: "N", 46: "M", 47: ".", 48: "⇥",
    49: "Space", 50: "`", 51: "⌫", 53: "⎋", 123: "←", 124: "→", 125: "↓",
    126: "↑",
  ]
}

@MainActor
final class SettingsStore: ObservableObject {
  static let shared = SettingsStore()

  private let defaults: UserDefaults

  @Published var isEnabled: Bool {
    didSet {
      defaults.set(isEnabled, forKey: SettingsKey.enabled)
      notifySettingsChanged()
    }
  }

  @Published var method: TypingMethod {
    didSet {
      defaults.set(method.rawValue, forKey: SettingsKey.method)
      notifySettingsChanged()
    }
  }

  @Published var expansions: [TextExpansion] {
    didSet {
      persistExpansions()
      notifySettingsChanged()
    }
  }

  @Published private(set) var excludedApplications: [ExcludedApplication] {
    didSet {
      persistExcludedApplications()
      notifySettingsChanged()
    }
  }

  @Published private(set) var toggleInputShortcut: KeyboardShortcut? {
    didSet { persistShortcut(toggleInputShortcut, key: SettingsKey.toggleInputShortcut) }
  }

  @Published private(set) var switchMethodShortcut: KeyboardShortcut? {
    didSet { persistShortcut(switchMethodShortcut, key: SettingsKey.switchMethodShortcut) }
  }

  @Published private(set) var launchAtLogin: Bool
  @Published private(set) var launchAtLoginError: String?

  convenience init() {
    let defaults = UserDefaults.standard
    LegacySettingsMigrator.migrateFromDiskIfNeeded(into: defaults)
    self.init(defaults: defaults)
  }

  init(defaults: UserDefaults) {
    self.defaults = defaults
    isEnabled = defaults.object(forKey: SettingsKey.enabled) as? Bool ?? true
    method = TypingMethod(rawValue: defaults.string(forKey: SettingsKey.method) ?? "") ?? .telex
    if let data = defaults.data(forKey: SettingsKey.expansions),
      let saved = try? JSONDecoder().decode([TextExpansion].self, from: data)
    {
      expansions = saved
    } else {
      expansions = []
    }
    if let data = defaults.data(forKey: SettingsKey.excludedApplications),
      let saved = try? JSONDecoder().decode([ExcludedApplication].self, from: data)
    {
      excludedApplications = saved
    } else {
      excludedApplications = []
    }
    toggleInputShortcut = Self.loadShortcut(
      defaults: defaults,
      key: SettingsKey.toggleInputShortcut
    )
    switchMethodShortcut = Self.loadShortcut(
      defaults: defaults,
      key: SettingsKey.switchMethodShortcut
    )
    launchAtLogin = SMAppService.mainApp.status == .enabled
  }

  func setLaunchAtLogin(_ enabled: Bool) {
    do {
      if enabled {
        try SMAppService.mainApp.register()
      } else {
        try SMAppService.mainApp.unregister()
      }
      launchAtLogin = enabled
      launchAtLoginError = nil
    } catch {
      launchAtLogin = SMAppService.mainApp.status == .enabled
      launchAtLoginError = "Không thể thay đổi mục đăng nhập. Hãy thử lại sau."
    }
  }

  func addExpansion() {
    expansions.append(TextExpansion())
  }

  func removeExpansion(id: TextExpansion.ID) {
    expansions.removeAll { $0.id == id }
  }

  func isExcluded(bundleIdentifier: String) -> Bool {
    excludedApplications.contains { $0.bundleIdentifier == bundleIdentifier }
  }

  func toggleExclusion(_ application: ExcludedApplication) {
    if isExcluded(bundleIdentifier: application.bundleIdentifier) {
      removeExcludedApplication(bundleIdentifier: application.bundleIdentifier)
    } else {
      excludedApplications.append(application)
      excludedApplications.sort {
        $0.displayName.localizedCaseInsensitiveCompare($1.displayName)
          == .orderedAscending
      }
    }
  }

  func removeExcludedApplication(bundleIdentifier: String) {
    excludedApplications.removeAll { $0.bundleIdentifier == bundleIdentifier }
  }

  func setShortcut(_ shortcut: KeyboardShortcut?, for action: HotKeyAction) {
    switch action {
    case .toggleInput:
      toggleInputShortcut = shortcut
      if shortcut == switchMethodShortcut { switchMethodShortcut = nil }
    case .switchMethod:
      switchMethodShortcut = shortcut
      if shortcut == toggleInputShortcut { toggleInputShortcut = nil }
    }
    notifySettingsChanged()
  }

  func perform(_ action: HotKeyAction) {
    switch action {
    case .toggleInput:
      isEnabled.toggle()
    case .switchMethod:
      method = method == .telex ? .vni : .telex
      isEnabled = true
    }
  }

  private func persistExpansions() {
    guard let data = try? JSONEncoder().encode(expansions) else { return }
    defaults.set(data, forKey: SettingsKey.expansions)
  }

  private func persistExcludedApplications() {
    guard let data = try? JSONEncoder().encode(excludedApplications) else { return }
    defaults.set(data, forKey: SettingsKey.excludedApplications)
  }

  private func persistShortcut(_ shortcut: KeyboardShortcut?, key: String) {
    guard let shortcut else {
      defaults.removeObject(forKey: key)
      return
    }
    guard let data = try? JSONEncoder().encode(shortcut) else { return }
    defaults.set(data, forKey: key)
  }

  private static func loadShortcut(defaults: UserDefaults, key: String) -> KeyboardShortcut? {
    guard let data = defaults.data(forKey: key) else { return nil }
    return try? JSONDecoder().decode(KeyboardShortcut.self, from: data)
  }

  private func notifySettingsChanged() {
    NotificationCenter.default.post(name: .naklSettingsDidChange, object: nil)
  }
}
