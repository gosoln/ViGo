import Foundation
import Testing

@testable import NAKL

@Test("Legacy method and text expansions migrate once")
func legacySettingsMigrateOnce() throws {
  let suiteName = "LegacySettingsMigratorTests.\(UUID().uuidString)"
  let defaults = try #require(UserDefaults(suiteName: suiteName))
  defer { defaults.removePersistentDomain(forName: suiteName) }

  let source = LegacySettingsSource(
    keyboardMethod: 1,
    shortcutsArchive: try legacyArchive([
      ("dc", "được"),
      ("omw", "on my way"),
      ("has space", "ignored"),
    ])
  )
  LegacySettingsMigrator.migrateIfNeeded(source: source, into: defaults)

  #expect(defaults.string(forKey: SettingsKey.method) == TypingMethod.vni.rawValue)
  let data = try #require(defaults.data(forKey: SettingsKey.expansions))
  let expansions = try JSONDecoder().decode([TextExpansion].self, from: data)
  #expect(expansions.map(\.shortcut) == ["dc", "omw"])
  #expect(expansions.map(\.replacement) == ["được", "on my way"])

  LegacySettingsMigrator.migrateIfNeeded(
    source: LegacySettingsSource(keyboardMethod: 2, shortcutsArchive: nil),
    into: defaults
  )
  #expect(defaults.string(forKey: SettingsKey.method) == TypingMethod.vni.rawValue)
}

@Test("Migration preserves current settings")
func migrationPreservesCurrentSettings() throws {
  let suiteName = "LegacySettingsMigratorPreservationTests.\(UUID().uuidString)"
  let defaults = try #require(UserDefaults(suiteName: suiteName))
  defer { defaults.removePersistentDomain(forName: suiteName) }

  defaults.set(TypingMethod.telex.rawValue, forKey: SettingsKey.method)
  let current = [TextExpansion(shortcut: "brb", replacement: "be right back")]
  defaults.set(try JSONEncoder().encode(current), forKey: SettingsKey.expansions)

  LegacySettingsMigrator.migrateIfNeeded(
    source: LegacySettingsSource(
      keyboardMethod: 1,
      shortcutsArchive: try legacyArchive([("dc", "được")])
    ),
    into: defaults
  )

  #expect(defaults.string(forKey: SettingsKey.method) == TypingMethod.telex.rawValue)
  let data = try #require(defaults.data(forKey: SettingsKey.expansions))
  #expect(try JSONDecoder().decode([TextExpansion].self, from: data) == current)
}

private func legacyArchive(_ values: [(String, String)]) throws -> Data {
  let innerArchiver = NSKeyedArchiver(requiringSecureCoding: false)
  innerArchiver.setClassName("ShortcutSetting", for: LegacyShortcutFixture.self)
  let records = values.map(LegacyShortcutFixture.init)
  innerArchiver.encode(records, forKey: NSKeyedArchiveRootObjectKey)
  innerArchiver.finishEncoding()
  return try NSKeyedArchiver.archivedData(
    withRootObject: innerArchiver.encodedData,
    requiringSecureCoding: true
  )
}

@objc(NAKLTestLegacyShortcutFixture)
private final class LegacyShortcutFixture: NSObject, NSCoding {
  let shortcut: String
  let text: String

  init(_ value: (String, String)) {
    shortcut = value.0
    text = value.1
  }

  required init?(coder: NSCoder) {
    nil
  }

  func encode(with coder: NSCoder) {
    coder.encode(shortcut, forKey: "shortcut")
    coder.encode(text, forKey: "text")
  }
}
