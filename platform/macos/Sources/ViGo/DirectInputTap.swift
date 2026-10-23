import AppKit
import ApplicationServices
import Carbon

enum DirectInputSecurityDecision: Equatable {
  case process(resetSession: Bool)
  case passThrough(resetSession: Bool)
}

struct DirectInputSecurityPolicy {
  private var wasSecureInputEnabled = false

  mutating func evaluate(isSecureInputEnabled: Bool) -> DirectInputSecurityDecision {
    let resetSession = isSecureInputEnabled != wasSecureInputEnabled
    wasSecureInputEnabled = isSecureInputEnabled
    if isSecureInputEnabled {
      return .passThrough(resetSession: resetSession)
    }
    return .process(resetSession: resetSession)
  }
}

struct DirectInputApplicationPolicy {
  let excludedBundleIdentifiers: Set<String>

  func shouldProcess(bundleIdentifier: String?) -> Bool {
    guard let bundleIdentifier else { return true }
    return !excludedBundleIdentifiers.contains(bundleIdentifier)
  }
}

struct DirectInputHotKeyPolicy {
  let toggleInput: KeyboardShortcut?
  let switchMethod: KeyboardShortcut?
  var toggleCurrentApplication: KeyboardShortcut? = nil

  func action(
    keyCode: UInt16,
    modifierFlags: NSEvent.ModifierFlags
  ) -> HotKeyAction? {
    if toggleInput?.matches(keyCode: keyCode, modifierFlags: modifierFlags) == true {
      return .toggleInput
    }
    if switchMethod?.matches(keyCode: keyCode, modifierFlags: modifierFlags) == true {
      return .switchMethod
    }
    if toggleCurrentApplication?.matches(
      keyCode: keyCode,
      modifierFlags: modifierFlags
    ) == true {
      return .toggleCurrentApplication
    }
    return nil
  }
}

final class DirectInputTap {
  private var activeApplicationPID: pid_t?
  private var applicationPolicy = DirectInputApplicationPolicy(excludedBundleIdentifiers: [])
  private var configuration = CoreConfigurationSnapshot()
  private var engine: DirectTypingEngine?
  private var eventTap: CFMachPort?
  private let handleHotKey: (HotKeyAction, ExcludedApplication?) -> Void
  private var hotKeyPolicy = DirectInputHotKeyPolicy(toggleInput: nil, switchMethod: nil)
  private var runLoopSource: CFRunLoopSource?
  private var securityPolicy = DirectInputSecurityPolicy()
  private var secureInputPollTimer: Timer?

  init(
    handleHotKey: @escaping (HotKeyAction, ExcludedApplication?) -> Void = { _, _ in }
  ) {
    self.handleHotKey = handleHotKey
    engine = DirectTypingEngine(configuration: configuration)
    refreshApplicationPolicy()
    refreshHotKeyPolicy()
    NotificationCenter.default.addObserver(
      self,
      selector: #selector(settingsDidChange),
      name: .vigoSettingsDidChange,
      object: nil
    )
    NSWorkspace.shared.notificationCenter.addObserver(
      self,
      selector: #selector(activeApplicationDidChange(_:)),
      name: NSWorkspace.didActivateApplicationNotification,
      object: nil
    )
  }

  deinit {
    NotificationCenter.default.removeObserver(self)
    NSWorkspace.shared.notificationCenter.removeObserver(self)
    stop()
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
    _ = refreshSecureInputState()
    startSecureInputPolling()
    return true
  }

  func stop() {
    secureInputPollTimer?.invalidate()
    secureInputPollTimer = nil
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
    proxy: CGEventTapProxy,
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

    if type == .leftMouseDown || type == .rightMouseDown || type == .otherMouseDown {
      resetSession()
      return Unmanaged.passUnretained(event)
    }
    guard type == .keyDown else { return Unmanaged.passUnretained(event) }
    guard !refreshSecureInputState() else {
      return Unmanaged.passUnretained(event)
    }

    let activeApplication = NSWorkspace.shared.frontmostApplication
    let currentPID = activeApplication?.processIdentifier
    if currentPID != activeApplicationPID {
      activeApplicationPID = currentPID
      resetSession()
    }

    let keyCode = UInt16(event.getIntegerValueField(.keyboardEventKeycode))
    let modifierFlags = NSEvent.ModifierFlags(rawValue: UInt(event.flags.rawValue))
    if activeApplication?.bundleIdentifier != Bundle.main.bundleIdentifier,
      let action = hotKeyPolicy.action(
        keyCode: keyCode,
        modifierFlags: modifierFlags
      )
    {
      resetSession()
      let currentApplication = activeApplication?.bundleIdentifier.map {
        ExcludedApplication(
          bundleIdentifier: $0,
          displayName: activeApplication?.localizedName ?? $0
        )
      }
      handleHotKey(action, currentApplication)
      return nil
    }
    guard applicationPolicy.shouldProcess(bundleIdentifier: activeApplication?.bundleIdentifier)
    else {
      resetSession()
      return Unmanaged.passUnretained(event)
    }

    let characters = unicodeCharacters(from: event)
    let normalized = MacKeyEventNormalizer.normalize(
      keyCode: keyCode,
      modifierFlags: modifierFlags,
      characters: characters,
      method: configuration.method
    )

    guard let engine else { return Unmanaged.passUnretained(event) }
    switch normalized {
    case .boundary:
      resetSession()
      return Unmanaged.passUnretained(event)
    case .backspace:
      _ = engine.process(normalized)
      return Unmanaged.passUnretained(event)
    case .text:
      switch engine.process(normalized) {
      case .passThrough:
        return Unmanaged.passUnretained(event)
      case .replaceCommitted(let deleteScalars, let insert):
        postCommittedReplacement(
          proxy: proxy,
          deleteScalars: deleteScalars,
          insert: insert
        )
        return nil
      }
    }
  }

  @objc private func settingsDidChange() {
    configuration = CoreConfigurationSnapshot()
    refreshApplicationPolicy()
    refreshHotKeyPolicy()
    engine = DirectTypingEngine(configuration: configuration)
  }

  @objc private func activeApplicationDidChange(_ notification: Notification) {
    activeApplicationPID = NSWorkspace.shared.frontmostApplication?.processIdentifier
    resetSession()
  }

  @objc private func pollSecureInputState(_ timer: Timer) {
    _ = refreshSecureInputState()
  }

  private func resetSession() {
    engine?.reset()
  }

  private func refreshApplicationPolicy(defaults: UserDefaults = .standard) {
    let applications: [ExcludedApplication]
    if let data = defaults.data(forKey: SettingsKey.excludedApplications),
      let saved = try? JSONDecoder().decode([ExcludedApplication].self, from: data)
    {
      applications = saved
    } else {
      applications = []
    }
    applicationPolicy = DirectInputApplicationPolicy(
      excludedBundleIdentifiers: Set(applications.map(\.bundleIdentifier))
    )
  }

  private func refreshHotKeyPolicy(defaults: UserDefaults = .standard) {
    func shortcut(for key: String) -> KeyboardShortcut? {
      guard let data = defaults.data(forKey: key) else { return nil }
      return try? JSONDecoder().decode(KeyboardShortcut.self, from: data)
    }
    hotKeyPolicy = DirectInputHotKeyPolicy(
      toggleInput: shortcut(for: SettingsKey.toggleInputShortcut),
      switchMethod: shortcut(for: SettingsKey.switchMethodShortcut),
      toggleCurrentApplication: shortcut(
        for: SettingsKey.toggleCurrentApplicationShortcut
      )
    )
  }

  private func startSecureInputPolling() {
    secureInputPollTimer?.invalidate()
    let timer = Timer(
      timeInterval: 0.25,
      target: self,
      selector: #selector(pollSecureInputState(_:)),
      userInfo: nil,
      repeats: true
    )
    secureInputPollTimer = timer
    RunLoop.main.add(timer, forMode: .common)
  }

  @discardableResult
  private func refreshSecureInputState() -> Bool {
    let decision = securityPolicy.evaluate(
      isSecureInputEnabled: IsSecureEventInputEnabled()
    )
    switch decision {
    case .process(let shouldReset):
      if shouldReset { resetSession() }
      return false
    case .passThrough(let shouldReset):
      if shouldReset { resetSession() }
      return true
    }
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

  private func postCommittedReplacement(
    proxy: CGEventTapProxy,
    deleteScalars: Int,
    insert: String
  ) {
    for _ in 0..<deleteScalars {
      postKeyPair(proxy: proxy, keyCode: 51, unicode: nil)
    }
    postKeyPair(proxy: proxy, keyCode: 0, unicode: Array(insert.utf16))
  }

  private func postKeyPair(
    proxy: CGEventTapProxy,
    keyCode: CGKeyCode,
    unicode: [UniChar]?
  ) {
    guard
      let down = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: true),
      let up = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: false)
    else { return }

    for event in [down, up] {
      if let unicode {
        unicode.withUnsafeBufferPointer { buffer in
          event.keyboardSetUnicodeString(
            stringLength: buffer.count,
            unicodeString: buffer.baseAddress
          )
        }
      }
      event.tapPostEvent(proxy)
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
  return inputTap.handle(proxy: proxy, type: type, event: event)
}
