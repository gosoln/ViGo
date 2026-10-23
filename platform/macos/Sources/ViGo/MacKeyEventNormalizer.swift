import AppKit
import Foundation

enum MacKeyEventNormalizer {
  private static let boundaryKeyCodes: Set<UInt16> = [
    36,  // Return
    48,  // Tab
    53,  // Escape
    71,  // Clear
    76,  // Keypad Enter
    115,  // Home
    116,  // Page Up
    117,  // Forward Delete
    119,  // End
    121,  // Page Down
    123, 124, 125, 126,  // Arrow keys
  ]

  static func normalize(
    keyCode: UInt16,
    modifierFlags: NSEvent.ModifierFlags,
    characters: String?,
    method: TypingMethod
  ) -> CoreEvent {
    let boundaryModifiers: NSEvent.ModifierFlags = [.command, .control, .option, .function]
    if !modifierFlags.intersection(boundaryModifiers).isEmpty {
      return .boundary
    }
    if keyCode == 51 { return .backspace }
    if boundaryKeyCodes.contains(keyCode) { return .boundary }

    guard let characters, characters.unicodeScalars.count == 1,
      let scalar = characters.unicodeScalars.first
    else { return .boundary }

    if scalar == " " || CharacterSet.letters.contains(scalar) {
      return .text(scalar)
    }
    if method == .vni, CharacterSet.decimalDigits.contains(scalar) {
      return .text(scalar)
    }
    return .boundary
  }
}
