import AppKit
import InputMethodKit

@objc(ViGoInputController)
final class ViGoInputController: IMKInputController {
  private var activeConfiguration: CoreConfigurationSnapshot?
  private var coreSession: CoreSession?
  private var markedComposition = MarkedComposition()
  private var configurationNeedsRefresh = true

  override init!(server: IMKServer!, delegate: Any!, client inputClient: Any!) {
    super.init(server: server, delegate: delegate, client: inputClient)
    NotificationCenter.default.addObserver(
      self,
      selector: #selector(settingsDidChange),
      name: .vigoSettingsDidChange,
      object: nil
    )
    refreshConfigurationIfNeeded()
  }

  deinit {
    NotificationCenter.default.removeObserver(self)
  }

  override func recognizedEvents(_ sender: Any!) -> Int {
    Int(NSEvent.EventTypeMask.keyDown.rawValue)
  }

  override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
    guard let event, let client = sender as? any IMKTextInput, event.type == .keyDown else {
      return false
    }
    if configurationNeedsRefresh {
      commitMarkedComposition(to: client)
    }
    refreshConfigurationIfNeeded()

    guard activeConfiguration?.isEnabled == true, let coreSession else {
      commitMarkedComposition(to: client)
      resetComposition()
      return false
    }

    let normalizedEvent = MacKeyEventNormalizer.normalize(
      keyCode: event.keyCode,
      modifierFlags: event.modifierFlags,
      characters: event.characters,
      method: activeConfiguration?.method ?? .telex
    )
    switch normalizedEvent {
    case .boundary:
      commitMarkedComposition(to: client)
      resetComposition()
      return false
    case .backspace:
      _ = coreSession.process(.backspace)
      guard markedComposition.removeLastScalar() else { return false }
      updateMarkedComposition(in: client)
      return true
    case .text(let scalar):
      let action = coreSession.process(normalizedEvent)
      if scalar == " " {
        switch action {
        case .passThrough:
          commitMarkedComposition(to: client)
          return false
        case .replaceBefore:
          guard markedComposition.apply(action, passThroughScalar: scalar) else {
            commitMarkedComposition(to: client)
            resetComposition()
            return false
          }
          commitMarkedComposition(to: client)
          return true
        }
      }

      guard markedComposition.apply(action, passThroughScalar: scalar) else {
        commitMarkedComposition(to: client)
        resetComposition()
        return false
      }
      updateMarkedComposition(in: client)
      return true
    }
  }

  override func commitComposition(_ sender: Any!) {
    if let client = sender as? any IMKTextInput {
      commitMarkedComposition(to: client)
    }
    resetComposition()
  }

  override func deactivateServer(_ sender: Any!) {
    if let client = sender as? any IMKTextInput {
      commitMarkedComposition(to: client)
    }
    resetComposition()
    super.deactivateServer(sender)
  }

  @objc private func settingsDidChange() {
    configurationNeedsRefresh = true
  }

  private func refreshConfigurationIfNeeded() {
    guard configurationNeedsRefresh else { return }
    let configuration = CoreConfigurationSnapshot()
    configurationNeedsRefresh = false
    guard configuration != activeConfiguration else { return }
    activeConfiguration = configuration
    coreSession = CoreSession(configuration: configuration)
  }

  private func resetComposition() {
    _ = coreSession?.process(.boundary)
    markedComposition.clear()
  }

  private func updateMarkedComposition(in client: any IMKTextInput) {
    let attributes =
      mark(
        forStyle: kTSMHiliteConvertedText,
        at: NSRange(location: NSNotFound, length: 0)
      ) as? [NSAttributedString.Key: Any] ?? [:]
    let markedText = NSAttributedString(
      string: markedComposition.text,
      attributes: attributes
    )
    client.setMarkedText(
      markedText,
      selectionRange: NSRange(location: markedComposition.text.utf16.count, length: 0),
      replacementRange: NSRange(location: NSNotFound, length: 0)
    )
  }

  private func commitMarkedComposition(to client: any IMKTextInput) {
    guard !markedComposition.isEmpty else { return }
    client.insertText(
      markedComposition.text,
      replacementRange: NSRange(location: NSNotFound, length: NSNotFound)
    )
    markedComposition.clear()
  }
}
