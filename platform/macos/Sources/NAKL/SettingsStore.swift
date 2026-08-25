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

  @Published private(set) var launchAtLogin: Bool
  @Published private(set) var launchAtLoginError: String?

  init(defaults: UserDefaults = .standard) {
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

  private func persistExpansions() {
    guard let data = try? JSONEncoder().encode(expansions) else { return }
    defaults.set(data, forKey: SettingsKey.expansions)
  }

  private func notifySettingsChanged() {
    NotificationCenter.default.post(name: .naklSettingsDidChange, object: nil)
  }
}
