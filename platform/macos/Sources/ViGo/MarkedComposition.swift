import Foundation

struct MarkedComposition: Equatable {
  private(set) var text = ""

  var isEmpty: Bool { text.isEmpty }

  mutating func apply(
    _ action: CoreAction,
    passThroughScalar: Unicode.Scalar
  ) -> Bool {
    switch action {
    case .passThrough:
      text.append(String(passThroughScalar))
      return true
    case .replaceBefore(let deleteScalars, let insert):
      guard deleteScalars <= text.unicodeScalars.count else { return false }
      text = String(text.unicodeScalars.dropLast(deleteScalars)) + insert
      return true
    }
  }

  mutating func removeLastScalar() -> Bool {
    guard !text.unicodeScalars.isEmpty else { return false }
    text = String(text.unicodeScalars.dropLast())
    return true
  }

  mutating func clear() {
    text = ""
  }
}
