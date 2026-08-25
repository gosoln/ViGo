import Foundation
import NAKLCoreBridge

enum CoreEvent {
  case text(Unicode.Scalar)
  case boundary
  case backspace
}

enum CoreAction: Equatable {
  case passThrough
  case replaceBefore(deleteScalars: Int, insert: String)
}

struct CoreConfigurationSnapshot: Equatable {
  let isEnabled: Bool
  let method: TypingMethod
  let expansions: [TextExpansion]

  init(defaults: UserDefaults = .standard) {
    isEnabled = defaults.object(forKey: SettingsKey.enabled) as? Bool ?? true
    method = TypingMethod(rawValue: defaults.string(forKey: SettingsKey.method) ?? "") ?? .telex

    if let data = defaults.data(forKey: SettingsKey.expansions),
      let saved = try? JSONDecoder().decode([TextExpansion].self, from: data)
    {
      expansions = saved
    } else {
      expansions = []
    }
  }

  init(isEnabled: Bool, method: TypingMethod, expansions: [TextExpansion] = []) {
    self.isEnabled = isEnabled
    self.method = method
    self.expansions = expansions
  }
}

final class CoreSession {
  private var handle: OpaquePointer?

  init?(configuration: CoreConfigurationSnapshot) {
    guard nakl_abi_version() == NAKL_ABI_VERSION,
      let config = nakl_config_new()
    else { return nil }
    defer { nakl_config_free(config) }

    for expansion in configuration.expansions
    where !expansion.shortcut.isEmpty && !expansion.replacement.isEmpty {
      let trigger = Array(expansion.shortcut.utf8)
      let replacement = Array(expansion.replacement.utf8)
      let status = trigger.withUnsafeBufferPointer { triggerBuffer in
        replacement.withUnsafeBufferPointer { replacementBuffer in
          nakl_config_add_text_expansion(
            config,
            triggerBuffer.baseAddress,
            triggerBuffer.count,
            replacementBuffer.baseAddress,
            replacementBuffer.count
          )
        }
      }
      guard status == NAKL_STATUS_OK else { return nil }
    }

    let method =
      switch configuration.method {
      case .telex: UInt32(NAKL_METHOD_TELEX)
      case .vni: UInt32(NAKL_METHOD_VNI)
      }
    handle = nakl_session_new_with_config(method, config)
    guard handle != nil else { return nil }
  }

  deinit {
    nakl_session_free(handle)
  }

  func process(_ event: CoreEvent) -> CoreAction {
    guard let handle else { return .passThrough }

    let input: (kind: UInt32, scalar: UInt32) =
      switch event {
      case .text(let scalar): (UInt32(NAKL_EVENT_TEXT), scalar.value)
      case .boundary: (UInt32(NAKL_EVENT_BOUNDARY), 0)
      case .backspace: (UInt32(NAKL_EVENT_BACKSPACE), 0)
      }
    var output = NaklAction()
    let status = nakl_session_process(handle, input.kind, input.scalar, &output)
    guard status == NAKL_STATUS_OK,
      output.kind == NAKL_ACTION_REPLACE_BEFORE,
      let insertPointer = output.insert_ptr
    else { return .passThrough }

    let bytes = UnsafeBufferPointer(start: insertPointer, count: output.insert_len)
    return .replaceBefore(
      deleteScalars: output.delete_scalars,
      insert: String(decoding: bytes, as: UTF8.self)
    )
  }
}
