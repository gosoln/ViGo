import AppKit
import ApplicationServices

private let accessibilityPromptKey = "AXTrustedCheckOptionPrompt"

final class DirectInputTap {
  private static let committedEditQueue = DispatchQueue(
    label: "com.nakl.inputmethod.committed-edit"
  )
  private static let syntheticEventMarker = CGEventFlags(rawValue: 1 << 29)

  private var activeApplicationPID: pid_t?
  private var configuration = CoreConfigurationSnapshot()
  private var engine: DirectTypingEngine?
  private var eventTap: CFMachPort?
  private var runLoopSource: CFRunLoopSource?

  init() {
    engine = DirectTypingEngine(configuration: configuration)
    NotificationCenter.default.addObserver(
      self,
      selector: #selector(settingsDidChange),
      name: .naklSettingsDidChange,
      object: nil
    )
  }

  deinit {
    NotificationCenter.default.removeObserver(self)
    stop()
  }

  static func requestAccessibilityPermission() -> Bool {
    AXIsProcessTrustedWithOptions([accessibilityPromptKey: true] as CFDictionary)
  }

  @discardableResult
  func startIfPermitted() -> Bool {
    guard eventTap == nil, AXIsProcessTrusted() else { return eventTap != nil }

    let mask = eventMask(
      for: [
        .keyDown,
        .leftMouseDown,
        .rightMouseDown,
        .otherMouseDown,
      ]
    )
    guard
      let tap = CGEvent.tapCreate(
        tap: .cgSessionEventTap,
        place: .headInsertEventTap,
        options: .defaultTap,
        eventsOfInterest: mask,
        callback: directInputTapCallback,
        userInfo: Unmanaged.passUnretained(self).toOpaque()
      )
    else { return false }

    let source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0)
    eventTap = tap
    runLoopSource = source
    CFRunLoopAddSource(CFRunLoopGetMain(), source, .commonModes)
    CGEvent.tapEnable(tap: tap, enable: true)
    return true
  }

  func stop() {
    if let eventTap {
      CGEvent.tapEnable(tap: eventTap, enable: false)
    }
    if let runLoopSource {
      CFRunLoopRemoveSource(CFRunLoopGetMain(), runLoopSource, .commonModes)
    }
    runLoopSource = nil
    eventTap = nil
    resetSession()
  }

  fileprivate func handle(
    type: CGEventType,
    event: CGEvent
  ) -> Unmanaged<CGEvent>? {
    if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
      if let eventTap {
        CGEvent.tapEnable(tap: eventTap, enable: true)
      }
      resetSession()
      return Unmanaged.passUnretained(event)
    }

    guard !event.flags.contains(Self.syntheticEventMarker) else {
      return Unmanaged.passUnretained(event)
    }

    if type == .leftMouseDown || type == .rightMouseDown || type == .otherMouseDown {
      resetSession()
      return Unmanaged.passUnretained(event)
    }
    guard type == .keyDown else { return Unmanaged.passUnretained(event) }

    let currentPID = NSWorkspace.shared.frontmostApplication?.processIdentifier
    if currentPID != activeApplicationPID {
      activeApplicationPID = currentPID
      resetSession()
    }

    let keyCode = UInt16(event.getIntegerValueField(.keyboardEventKeycode))
    let characters = unicodeCharacters(from: event)
    let normalized = MacKeyEventNormalizer.normalize(
      keyCode: keyCode,
      modifierFlags: NSEvent.ModifierFlags(rawValue: UInt(event.flags.rawValue)),
      characters: characters,
      method: configuration.method
    )

    guard let engine else { return Unmanaged.passUnretained(event) }
    switch normalized {
    case .boundary, .backspace:
      _ = engine.process(normalized)
      return Unmanaged.passUnretained(event)
    case .text:
      switch engine.process(normalized) {
      case .passThrough:
        return Unmanaged.passUnretained(event)
      case .replaceCommitted(let deleteScalars, let insert):
        deferCommittedReplacement(deleteScalars: deleteScalars, insert: insert)
        return nil
      }
    }
  }

  @objc private func settingsDidChange() {
    configuration = CoreConfigurationSnapshot()
    engine = DirectTypingEngine(configuration: configuration)
  }

  private func resetSession() {
    engine = DirectTypingEngine(configuration: configuration)
  }

  private func unicodeCharacters(from event: CGEvent) -> String? {
    var units = [UniChar](repeating: 0, count: 8)
    var actualLength = 0
    units.withUnsafeMutableBufferPointer { buffer in
      event.keyboardGetUnicodeString(
        maxStringLength: buffer.count,
        actualStringLength: &actualLength,
        unicodeString: buffer.baseAddress
      )
    }
    guard actualLength > 0 else { return nil }
    return String(utf16CodeUnits: units, count: actualLength)
  }

  private func deferCommittedReplacement(
    deleteScalars: Int,
    insert: String
  ) {
    Self.committedEditQueue.async {
      Self.postCommittedReplacement(deleteScalars: deleteScalars, insert: insert)
    }
  }

  private static func postCommittedReplacement(
    deleteScalars: Int,
    insert: String
  ) {
    for _ in 0..<deleteScalars {
      postKeyPair(keyCode: 51, unicode: nil)
    }
    postKeyPair(keyCode: 0, unicode: Array(insert.utf16))
  }

  private static func postKeyPair(
    keyCode: CGKeyCode,
    unicode: [UniChar]?
  ) {
    guard
      let down = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: true),
      let up = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: false)
    else { return }

    for event in [down, up] {
      event.flags = Self.syntheticEventMarker
      if let unicode {
        unicode.withUnsafeBufferPointer { buffer in
          event.keyboardSetUnicodeString(
            stringLength: buffer.count,
            unicodeString: buffer.baseAddress
          )
        }
      }
      event.post(tap: .cgSessionEventTap)
    }
  }
}

private func eventMask(for types: [CGEventType]) -> CGEventMask {
  types.reduce(0) { mask, type in
    mask | (CGEventMask(1) << type.rawValue)
  }
}

private func directInputTapCallback(
  proxy: CGEventTapProxy,
  type: CGEventType,
  event: CGEvent,
  userInfo: UnsafeMutableRawPointer?
) -> Unmanaged<CGEvent>? {
  guard let userInfo else { return Unmanaged.passUnretained(event) }
  let inputTap = Unmanaged<DirectInputTap>.fromOpaque(userInfo).takeUnretainedValue()
  return inputTap.handle(type: type, event: event)
}
