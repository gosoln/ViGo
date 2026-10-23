import AppKit
import SwiftUI

@MainActor
private final class FixtureAppDelegate: NSObject, NSApplicationDelegate {
  func applicationDidFinishLaunching(_ notification: Notification) {
    NSApp.setActivationPolicy(.regular)
    NSApp.activate(ignoringOtherApps: true)
  }
}

@main
struct ViGoCompatibilityFixtureApp: App {
  @NSApplicationDelegateAdaptor(FixtureAppDelegate.self) private var appDelegate

  var body: some Scene {
    WindowGroup("ViGo Compatibility Fixture") {
      CompatibilityFixtureView()
        .frame(width: 620, height: 520)
    }
    .windowResizability(.contentSize)
  }
}

private struct CompatibilityFixtureView: View {
  @State private var telex = ""
  @State private var telexHorn = ""
  @State private var uppercase = ""
  @State private var vni = ""
  @State private var backspace = ""
  @State private var expansion = ""
  @State private var contextA = ""
  @State private var contextB = ""
  @State private var navigation = ""
  @State private var selection = "LtestR"
  @State private var modifier = ""
  @State private var disabled = ""
  @State private var secure = ""

  var body: some View {
    Form {
      Section("Telex — select Telex before running") {
        TestField(label: "Telex: tieengs", expected: "tiếng", text: $telex)
        TestField(label: "Telex: uow", expected: "ươ", text: $telexHorn)
        TestField(label: "Telex: TIEENGS", expected: "TIẾNG", text: $uppercase)
        TestField(
          label: "Type tieengs, Backspace, f",
          expected: "tiền",
          text: $backspace
        )
        TestField(
          label: "After configuring btw → by the way, type btw + Space",
          expected: "by the way ",
          text: $expansion
        )
      }

      Section("VNI — switch to VNI, then reset this field") {
        TestField(label: "VNI: tie6ng1", expected: "tiếng", text: $vni)
      }

      Section("Context isolation") {
        TextField("Field A: type a", text: $contextA)
        TextField("Field B: then type s (must remain s)", text: $contextB)
        StatusMark(passed: contextA == "a" && contextB == "s")
      }

      Section("Caret boundary") {
        TextField("Type xa, Left Arrow, then a (must be xaa)", text: $navigation)
        StatusMark(passed: navigation == "xaa")
      }

      Section("Selection and modifiers") {
        TextField("Select only test in LtestR, then type as", text: $selection)
        StatusMark(passed: selection == "LáR")
        TextField("Type a, Cmd+A, Right Arrow, then s", text: $modifier)
        StatusMark(passed: modifier == "as")
      }

      Section("Disabled mode") {
        TextField("Disable ViGo, then type tieengs", text: $disabled)
        StatusMark(passed: disabled == "tieengs")
      }

      Section("Secure context") {
        SecureField("Type tieengs (must remain seven characters)", text: $secure)
        StatusMark(passed: secure.count == 7)
      }

      HStack {
        Spacer()
        Button("Reset synthetic fields") {
          telex = ""
          telexHorn = ""
          uppercase = ""
          vni = ""
          backspace = ""
          expansion = ""
          contextA = ""
          contextB = ""
          navigation = ""
          selection = "LtestR"
          modifier = ""
          disabled = ""
          secure = ""
        }
      }
    }
    .formStyle(.grouped)
    .padding()
  }
}

private struct TestField: View {
  let label: String
  let expected: String
  @Binding var text: String

  var body: some View {
    HStack {
      TextField(label, text: $text)
      StatusMark(
        passed: text == expected && text == text.precomposedStringWithCanonicalMapping
      )
    }
  }
}

private struct StatusMark: View {
  let passed: Bool

  var body: some View {
    Image(systemName: passed ? "checkmark.circle.fill" : "circle")
      .foregroundStyle(passed ? .green : .secondary)
      .accessibilityLabel(passed ? "PASS" : "Not passed")
  }
}
