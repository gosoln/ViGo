import Foundation

enum ReplacementRangeResolver {
  static func resolve(
    selection: NSRange,
    deletingScalarCount: Int,
    precedingText: String
  ) -> NSRange? {
    guard selection.location != NSNotFound,
      selection.length == 0,
      deletingScalarCount > 0,
      precedingText.unicodeScalars.count >= deletingScalarCount
    else { return nil }

    let deletedScalars = precedingText.unicodeScalars.suffix(deletingScalarCount)
    let deletedUTF16Count = String(deletedScalars).utf16.count
    guard deletedUTF16Count <= selection.location else { return nil }

    return NSRange(
      location: selection.location - deletedUTF16Count,
      length: deletedUTF16Count
    )
  }
}
