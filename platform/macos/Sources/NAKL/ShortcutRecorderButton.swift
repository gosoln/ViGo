import AppKit
import SwiftUI

struct ShortcutRecorderButton: View {
  let shortcut: KeyboardShortcut?
  let onChange: (KeyboardShortcut?) -> Void

  @StateObject private var recorder = ShortcutRecorderState()

  var body: some View {
    Button(recorder.isRecording ? "Nhấn tổ hợp phím…" : shortcut?.displayName ?? "Chọn phím…") {
      if recorder.isRecording {
        recorder.stop()
      } else {
        recorder.start(onChange: onChange)
      }
    }
    .frame(minWidth: 150)
    .contextMenu {
      Button("Xóa phím nóng") {
        recorder.stop()
        onChange(nil)
      }
      .disabled(shortcut == nil)
    }
    .onDisappear {
      recorder.stop()
    }
  }
}

@MainActor
private final class ShortcutRecorderState: ObservableObject {
  @Published private(set) var isRecording = false
  private var monitor: Any?

  func start(onChange: @escaping (KeyboardShortcut?) -> Void) {
    stop()
    isRecording = true
    monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
      guard let self else { return event }
      if event.keyCode == 53 {
        stop()
        return nil
      }
      guard
        let shortcut = KeyboardShortcut(
          keyCode: event.keyCode,
          modifierFlags: event.modifierFlags
        )
      else {
        NSSound.beep()
        return nil
      }
      onChange(shortcut)
      stop()
      return nil
    }
  }

  func stop() {
    if let monitor {
      NSEvent.removeMonitor(monitor)
    }
    monitor = nil
    isRecording = false
  }
}
