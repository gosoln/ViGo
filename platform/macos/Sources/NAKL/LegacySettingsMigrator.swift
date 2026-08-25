import Foundation

struct LegacySettingsSource {
  let keyboardMethod: Int?
  let shortcutsArchive: Data?

  static func loadFromDisk(fileManager: FileManager = .default) -> Self {
    let legacyDefaults = UserDefaults(suiteName: "com.zepvn.NAKL")
    let storedMethod = legacyDefaults?.object(forKey: "NAKLKeyboardMethod") as? NSNumber
    let archiveURL = fileManager.homeDirectoryForCurrentUser
      .appendingPathComponent("Library/Application Support/NAKL/shortcuts.setting")
    let archive = try? Data(contentsOf: archiveURL, options: [.mappedIfSafe])
    return Self(keyboardMethod: storedMethod?.intValue, shortcutsArchive: archive)
  }
}

enum LegacySettingsMigrator {
  private static let migrationKey = "migration.legacy.v1"

  static func migrateFromDiskIfNeeded(into defaults: UserDefaults) {
    guard !defaults.bool(forKey: migrationKey) else { return }
    migrateIfNeeded(source: LegacySettingsSource.loadFromDisk(), into: defaults)
  }

  static func migrateIfNeeded(
    source: LegacySettingsSource,
    into defaults: UserDefaults
  ) {
    guard !defaults.bool(forKey: migrationKey) else { return }

    if defaults.object(forKey: SettingsKey.method) == nil,
      let method = typingMethod(from: source.keyboardMethod)
    {
      defaults.set(method.rawValue, forKey: SettingsKey.method)
    }

    if defaults.object(forKey: SettingsKey.expansions) == nil,
      let archive = source.shortcutsArchive,
      let expansions = decodeExpansions(from: archive),
      let encoded = try? JSONEncoder().encode(expansions)
    {
      defaults.set(encoded, forKey: SettingsKey.expansions)
    }

    defaults.set(true, forKey: migrationKey)
  }

  private static func typingMethod(from legacyValue: Int?) -> TypingMethod? {
    switch legacyValue {
    case 1: .vni
    case 2: .telex
    default: nil
    }
  }

  private static func decodeExpansions(from archive: Data) -> [TextExpansion]? {
    let maximumArchiveBytes = 2 * 1_024 * 1_024
    guard archive.count <= maximumArchiveBytes,
      let archivedData = try? NSKeyedUnarchiver.unarchivedObject(
        ofClass: NSData.self,
        from: archive
      )
    else { return nil }
    let innerArchive = archivedData as Data
    guard innerArchive.count <= maximumArchiveBytes,
      let unarchiver = try? NSKeyedUnarchiver(forReadingFrom: innerArchive)
    else { return nil }
    defer { unarchiver.finishDecoding() }

    unarchiver.decodingFailurePolicy = .setErrorAndReturn
    unarchiver.setClass(LegacyShortcutRecord.self, forClassName: "ShortcutSetting")
    guard
      let records = unarchiver.decodeObject(
        of: [NSArray.self, LegacyShortcutRecord.self, NSString.self],
        forKey: NSKeyedArchiveRootObjectKey
      ) as? [LegacyShortcutRecord], records.count <= 500
    else { return nil }

    var expansions: [TextExpansion] = []
    for record in records {
      guard isValidShortcut(record.shortcut), isValidReplacement(record.text) else { continue }
      expansions.removeAll { $0.shortcut == record.shortcut }
      expansions.append(TextExpansion(shortcut: record.shortcut, replacement: record.text))
    }
    return expansions
  }

  private static func isValidShortcut(_ value: String) -> Bool {
    let scalars = value.unicodeScalars
    return (1...64).contains(scalars.count)
      && !scalars.contains { CharacterSet.whitespacesAndNewlines.contains($0) || $0.value == 0 }
  }

  private static func isValidReplacement(_ value: String) -> Bool {
    let scalars = value.unicodeScalars
    return (1...4_096).contains(scalars.count) && !scalars.contains { $0.value == 0 }
  }
}

@objc(NAKLLegacyShortcutRecord)
private final class LegacyShortcutRecord: NSObject, NSSecureCoding {
  static let supportsSecureCoding = true

  let shortcut: String
  let text: String

  required init?(coder: NSCoder) {
    guard let shortcut = coder.decodeObject(of: NSString.self, forKey: "shortcut") as String?,
      let text = coder.decodeObject(of: NSString.self, forKey: "text") as String?
    else { return nil }
    self.shortcut = shortcut
    self.text = text
  }

  func encode(with coder: NSCoder) {
    coder.encode(shortcut, forKey: "shortcut")
    coder.encode(text, forKey: "text")
  }
}
