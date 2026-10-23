import AppKit
import Foundation
import Testing

@testable import ViGo

@Test("C ABI session applies configured text expansions")
func coreSessionAppliesTextExpansion() throws {
  let configuration = CoreConfigurationSnapshot(
    isEnabled: true,
    method: .telex,
    expansions: [TextExpansion(shortcut: "btw", replacement: "by the way")]
  )
  let session = try #require(CoreSession(configuration: configuration))

  for scalar in "btw".unicodeScalars {
    #expect(session.process(.text(scalar)) == .passThrough)
  }
  #expect(
    session.process(.text(" "))
      == .replaceBefore(deleteScalars: 3, insert: "by the way ")
  )
}

@Test("Marked composition follows core replacement actions")
func markedCompositionFollowsCoreReplacementActions() throws {
  let session = try #require(
    CoreSession(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: []
      )
    )
  )
  var composition = MarkedComposition()

  for scalar in "tieengs".unicodeScalars {
    let applied = composition.apply(
      session.process(.text(scalar)),
      passThroughScalar: scalar
    )
    #expect(applied)
  }

  #expect(composition.text == "tiếng")
  #expect(session.process(.backspace) == .passThrough)
  let removed = composition.removeLastScalar()
  #expect(removed)
  #expect(composition.text == "tiến")

  let retuned = composition.apply(
    session.process(.text("f")),
    passThroughScalar: "f"
  )
  #expect(retuned)
  #expect(composition.text == "tiền")
}

@Test("Marked composition applies expansion before commit")
func markedCompositionAppliesExpansionBeforeCommit() throws {
  let session = try #require(
    CoreSession(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: [TextExpansion(shortcut: "btw", replacement: "by the way")]
      )
    )
  )
  var composition = MarkedComposition()

  for scalar in "btw".unicodeScalars {
    let applied = composition.apply(
      session.process(.text(scalar)),
      passThroughScalar: scalar
    )
    #expect(applied)
  }
  let expanded = composition.apply(
    session.process(.text(" ")),
    passThroughScalar: " "
  )
  #expect(expanded)
  #expect(composition.text == "by the way ")
}

@Test("Direct typing commits Telex without marked text")
func directTypingCommitsTelexWithoutMarkedText() throws {
  let engine = try #require(
    DirectTypingEngine(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: []
      )
    )
  )

  #expect(applyDirectInput("tieengs", using: engine) == "tiếng")
}

@Test("Direct typing places a late tone on the giai nucleus")
func directTypingPlacesLateToneOnGiaiNucleus() throws {
  let engine = try #require(
    DirectTypingEngine(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: []
      )
    )
  )

  #expect(applyDirectInput("giair", using: engine) == "giải")
}

@Test("Direct typing replaces a vowel instead of duplicating it")
func directTypingReplacesVowelWithoutDuplication() throws {
  let engine = try #require(
    DirectTypingEngine(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: []
      )
    )
  )

  #expect(applyDirectInput("chaf", using: engine) == "chà")
}

@Test("Direct typing keeps a restored foreign word literal")
func directTypingKeepsRestoredForeignWordLiteral() throws {
  let engine = try #require(
    DirectTypingEngine(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: []
      )
    )
  )

  #expect(applyDirectInput("verrsion", using: engine) == "version")
}

@Test("Direct typing reset prevents composition crossing text contexts")
func directTypingResetPreventsCrossContextComposition() throws {
  let engine = try #require(
    DirectTypingEngine(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: []
      )
    )
  )

  #expect(engine.process(.text("a")) == .passThrough)
  engine.reset()
  #expect(engine.process(.text("s")) == .passThrough)
}

@Test("Secure input passes through and resets on both transitions")
func secureInputPassesThroughAndResetsOnTransitions() {
  var policy = DirectInputSecurityPolicy()

  #expect(policy.evaluate(isSecureInputEnabled: false) == .process(resetSession: false))
  #expect(policy.evaluate(isSecureInputEnabled: true) == .passThrough(resetSession: true))
  #expect(policy.evaluate(isSecureInputEnabled: true) == .passThrough(resetSession: false))
  #expect(policy.evaluate(isSecureInputEnabled: false) == .process(resetSession: true))
}

@Test("Excluded application policy bypasses only configured bundle identifiers")
func excludedApplicationPolicyBypassesConfiguredIdentifiers() {
  let policy = DirectInputApplicationPolicy(
    excludedBundleIdentifiers: ["com.example.Editor"]
  )

  #expect(!policy.shouldProcess(bundleIdentifier: "com.example.Editor"))
  #expect(policy.shouldProcess(bundleIdentifier: "com.example.Browser"))
  #expect(policy.shouldProcess(bundleIdentifier: nil))
}

@Test("Hot key policy maps configured combinations to actions")
func hotKeyPolicyMapsConfiguredCombinationsToActions() throws {
  let toggle = try #require(
    KeyboardShortcut(keyCode: 49, modifierFlags: [.control, .option])
  )
  let switchMethod = try #require(
    KeyboardShortcut(keyCode: 17, modifierFlags: [.control, .option])
  )
  let policy = DirectInputHotKeyPolicy(
    toggleInput: toggle,
    switchMethod: switchMethod
  )

  #expect(
    policy.action(keyCode: 49, modifierFlags: [.control, .option]) == .toggleInput
  )
  #expect(
    policy.action(keyCode: 17, modifierFlags: [.control, .option]) == .switchMethod
  )
  #expect(policy.action(keyCode: 0, modifierFlags: [.command]) == nil)
}

@Test("Direct typing commits text expansion on space")
func directTypingCommitsTextExpansionOnSpace() throws {
  let engine = try #require(
    DirectTypingEngine(
      configuration: CoreConfigurationSnapshot(
        isEnabled: true,
        method: .telex,
        expansions: [TextExpansion(shortcut: "btw", replacement: "by the way")]
      )
    )
  )

  #expect(applyDirectInput("btw ", using: engine) == "by the way ")
}

@Test("macOS key events normalize to semantic core events")
func macOSKeyEventsNormalizeToCoreEvents() {
  #expect(normalized(keyCode: 123, characters: "\u{F702}") == .boundary)
  #expect(normalized(keyCode: 117, characters: "\u{F728}") == .boundary)
  #expect(normalized(keyCode: 51, characters: "\u{08}") == .backspace)
  #expect(normalized(keyCode: 0, characters: ".") == .boundary)
  #expect(normalized(keyCode: 0, characters: "1") == .boundary)
  #expect(normalized(keyCode: 0, characters: "1", method: .vni) == .text("1"))
  #expect(normalized(keyCode: 0, modifiers: .command, characters: "a") == .boundary)
  #expect(normalized(keyCode: 0, characters: "a") == .text("a"))
}

private func normalized(
  keyCode: UInt16,
  modifiers: NSEvent.ModifierFlags = [],
  characters: String?,
  method: TypingMethod = .telex
) -> CoreEvent {
  MacKeyEventNormalizer.normalize(
    keyCode: keyCode,
    modifierFlags: modifiers,
    characters: characters,
    method: method
  )
}

private func applyDirectInput(_ input: String, using engine: DirectTypingEngine) -> String {
  var committedText = ""
  for scalar in input.unicodeScalars {
    switch engine.process(.text(scalar)) {
    case .passThrough:
      committedText.append(String(scalar))
    case .replaceCommitted(let deleteScalars, let insert):
      committedText = String(committedText.unicodeScalars.dropLast(deleteScalars)) + insert
    }
  }
  return committedText
}
