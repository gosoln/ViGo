#!/usr/bin/env swift
import Carbon
import Foundation

private let defaultIdentifier = "com.nakl.inputmethod.NAKL"

private func property(_ source: TISInputSource, _ key: CFString) -> AnyObject? {
  guard let pointer = TISGetInputSourceProperty(source, key) else { return nil }
  return Unmanaged<AnyObject>.fromOpaque(pointer).takeUnretainedValue()
}

private func identifier(of source: TISInputSource) -> String? {
  property(source, kTISPropertyInputSourceID) as? String
}

private func booleanProperty(_ source: TISInputSource, _ key: CFString) -> Bool {
  (property(source, key) as? NSNumber)?.boolValue ?? false
}

private func findSource(identifier: String) -> TISInputSource? {
  let filter = [kTISPropertyInputSourceID as String: identifier as CFString] as CFDictionary
  guard let result = TISCreateInputSourceList(filter, true) else { return nil }
  let sources = result.takeRetainedValue() as NSArray
  return sources.firstObject as! TISInputSource?
}

private func currentIdentifier() -> String? {
  guard let result = TISCopyCurrentKeyboardInputSource() else { return nil }
  let source = result.takeRetainedValue()
  return identifier(of: source)
}

private func printStatus(identifier: String) -> Bool {
  guard let source = findSource(identifier: identifier) else {
    print("registered=false enabled=false selectCapable=false selected=false")
    return false
  }
  let enabled = booleanProperty(source, kTISPropertyInputSourceIsEnabled)
  let selectCapable = booleanProperty(source, kTISPropertyInputSourceIsSelectCapable)
  let selected = currentIdentifier() == identifier
  print(
    "registered=true enabled=\(enabled) selectCapable=\(selectCapable) selected=\(selected)"
  )
  return enabled && selectCapable && selected
}

private func requireSource(_ identifier: String) -> TISInputSource {
  guard let source = findSource(identifier: identifier) else {
    fputs("Input source not registered: \(identifier)\n", stderr)
    exit(2)
  }
  return source
}

private func enable(identifier: String) {
  let status = TISEnableInputSource(requireSource(identifier))
  guard status == noErr else {
    fputs("TISEnableInputSource failed with status \(status)\n", stderr)
    exit(3)
  }
  for _ in 0..<20 {
    if let refreshed = findSource(identifier: identifier),
      booleanProperty(refreshed, kTISPropertyInputSourceIsEnabled)
    {
      return
    }
    Thread.sleep(forTimeInterval: 0.1)
  }
  fputs("TISEnableInputSource returned success but the source remains disabled\n", stderr)
  exit(4)
}

private func disable(identifier: String) {
  guard let source = findSource(identifier: identifier) else { return }
  let status = TISDisableInputSource(source)
  guard status == noErr else {
    fputs("TISDisableInputSource failed with status \(status)\n", stderr)
    exit(8)
  }
  for _ in 0..<20 {
    guard let refreshed = findSource(identifier: identifier) else { return }
    if !booleanProperty(refreshed, kTISPropertyInputSourceIsEnabled) { return }
    Thread.sleep(forTimeInterval: 0.1)
  }
  fputs("TISDisableInputSource returned success but the source remains enabled\n", stderr)
  exit(9)
}

private func select(identifier: String) {
  enable(identifier: identifier)
  let status = TISSelectInputSource(requireSource(identifier))
  guard status == noErr else {
    fputs("TISSelectInputSource failed with status \(status)\n", stderr)
    exit(5)
  }
  for _ in 0..<20 {
    if currentIdentifier() == identifier { return }
    Thread.sleep(forTimeInterval: 0.1)
  }
  fputs("TISSelectInputSource returned success but did not select \(identifier)\n", stderr)
  exit(6)
}

let arguments = Array(CommandLine.arguments.dropFirst())
guard let command = arguments.first else {
  fputs(
    "Usage: input-source-tool.swift <current|status|register|enable|disable|select|restore-abc> [value]\n",
    stderr
  )
  exit(64)
}

switch command {
case "current":
  print(currentIdentifier() ?? "unknown")
case "status":
  _ = printStatus(identifier: arguments.dropFirst().first ?? defaultIdentifier)
case "register":
  guard let bundlePath = arguments.dropFirst().first else {
    fputs("register requires an app bundle path\n", stderr)
    exit(64)
  }
  let status = TISRegisterInputSource(URL(fileURLWithPath: bundlePath) as CFURL)
  guard status == noErr else {
    fputs("TISRegisterInputSource failed with status \(status)\n", stderr)
    exit(7)
  }
  for _ in 0..<20 {
    if findSource(identifier: defaultIdentifier) != nil {
      _ = printStatus(identifier: defaultIdentifier)
      exit(0)
    }
    Thread.sleep(forTimeInterval: 0.1)
  }
  fputs("TISRegisterInputSource returned success but the source was not registered\n", stderr)
  exit(10)
case "enable":
  let targetIdentifier = arguments.dropFirst().first ?? defaultIdentifier
  enable(identifier: targetIdentifier)
  _ = printStatus(identifier: targetIdentifier)
case "disable":
  let targetIdentifier = arguments.dropFirst().first ?? defaultIdentifier
  disable(identifier: targetIdentifier)
  _ = printStatus(identifier: targetIdentifier)
case "select":
  let targetIdentifier = arguments.dropFirst().first ?? defaultIdentifier
  select(identifier: targetIdentifier)
  _ = printStatus(identifier: targetIdentifier)
case "restore-abc":
  select(identifier: "com.apple.keylayout.ABC")
  _ = printStatus(identifier: "com.apple.keylayout.ABC")
default:
  fputs("Unknown command: \(command)\n", stderr)
  exit(64)
}
