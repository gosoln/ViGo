enum DirectTypingDecision: Equatable {
  case passThrough
  case replaceCommitted(deleteScalars: Int, insert: String)
}

final class DirectTypingEngine {
  private let session: CoreSession

  init?(configuration: CoreConfigurationSnapshot) {
    guard configuration.isEnabled, let session = CoreSession(configuration: configuration) else {
      return nil
    }
    self.session = session
  }

  func process(_ event: CoreEvent) -> DirectTypingDecision {
    switch session.process(event) {
    case .passThrough:
      .passThrough
    case .replaceBefore(let deleteScalars, let insert):
      .replaceCommitted(deleteScalars: deleteScalars, insert: insert)
    }
  }

  func reset() {
    _ = session.process(.boundary)
  }
}
