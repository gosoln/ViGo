import AppKit
import Foundation
import Testing

@testable import NAKL

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

@Test("Replacement ranges use UTF-16 document offsets")
func replacementRangesUseUTF16Offsets() {
  #expect(
    ReplacementRangeResolver.resolve(
      selection: NSRange(location: 5, length: 0),
      deletingScalarCount: 3,
      precedingText: "abc"
    ) == NSRange(location: 2, length: 3)
  )
  #expect(
    ReplacementRangeResolver.resolve(
      selection: NSRange(location: 4, length: 0),
      deletingScalarCount: 1,
      precedingText: "a😀"
    ) == NSRange(location: 2, length: 2)
  )
  #expect(
    ReplacementRangeResolver.resolve(
      selection: NSRange(location: 2, length: 0),
      deletingScalarCount: 1,
      precedingText: "ế"
    ) == NSRange(location: 1, length: 1)
  )
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
