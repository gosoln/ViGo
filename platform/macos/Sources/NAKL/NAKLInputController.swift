import AppKit
import InputMethodKit

@objc(NAKLInputController)
final class NAKLInputController: IMKInputController {
  private var activeConfiguration: CoreConfigurationSnapshot?
  private var coreSession: CoreSession?
  private var configurationNeedsRefresh = true

  override init!(server: IMKServer!, delegate: Any!, client inputClient: Any!) {
    super.init(server: server, delegate: delegate, client: inputClient)
    NotificationCenter.default.addObserver(
      self,
      selector: #selector(settingsDidChange),
      name: .naklSettingsDidChange,
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
    guard let event, let sender, event.type == .keyDown else { return false }
    refreshConfigurationIfNeeded()

    guard activeConfiguration?.isEnabled == true, let coreSession else {
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
      resetComposition()
      return false
    case .backspace:
      _ = coreSession.process(.backspace)
      return false
    case .text:
      break
    }

    switch coreSession.process(normalizedEvent) {
    case .passThrough:
      return false
    case .replaceBefore(let deleteScalars, let insert):
      return replaceText(
        before: sender,
        deletingScalarCount: deleteScalars,
        with: insert
      )
    }
  }

  override func commitComposition(_ sender: Any!) {
    resetComposition()
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
  }

  private func replaceText(
    before sender: Any,
    deletingScalarCount: Int,
    with insert: String
  ) -> Bool {
    guard let client = sender as? any IMKTextInput else {
      resetComposition()
      return false
    }

    let selection = client.selectedRange()
    guard selection.location != NSNotFound, selection.length == 0 else {
      resetComposition()
      return false
    }

    let maximumLookback = deletingScalarCount.multipliedReportingOverflow(by: 2)
    guard !maximumLookback.overflow else {
      resetComposition()
      return false
    }
    let lookback = min(selection.location, maximumLookback.partialValue)
    let contextRange = NSRange(location: selection.location - lookback, length: lookback)
    guard let precedingText = client.attributedSubstring(from: contextRange)?.string,
      let replacementRange = ReplacementRangeResolver.resolve(
        selection: selection,
        deletingScalarCount: deletingScalarCount,
        precedingText: precedingText
      )
    else {
      resetComposition()
      return false
    }

    client.insertText(insert, replacementRange: replacementRange)
    return true
  }
}
