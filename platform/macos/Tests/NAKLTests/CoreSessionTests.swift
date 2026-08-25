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
