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

@MainActor
final class SettingsStore: ObservableObject {
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

  private func persistExpansions() {
    guard let data = try? JSONEncoder().encode(expansions) else { return }
    defaults.set(data, forKey: SettingsKey.expansions)
  }

  private func persistExcludedApplications() {
    guard let data = try? JSONEncoder().encode(excludedApplications) else { return }
    defaults.set(data, forKey: SettingsKey.excludedApplications)
  }

  private func notifySettingsChanged() {
    NotificationCenter.default.post(name: .naklSettingsDidChange, object: nil)
  }
}
