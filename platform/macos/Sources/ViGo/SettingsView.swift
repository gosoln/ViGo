import SwiftUI

struct SettingsView: View {
  @ObservedObject var accessibility: AccessibilityAuthorization
  @ObservedObject var settings: SettingsStore

  var body: some View {
    TabView {
      GeneralSettingsView(accessibility: accessibility, settings: settings)
        .tabItem {
          Label("Tùy chọn", systemImage: "gearshape")
        }

      TextExpansionSettingsView(settings: settings)
        .tabItem {
          Label("Gõ tắt", systemImage: "text.badge.plus")
        }

      AboutView()
        .tabItem {
          Label("Giới thiệu", systemImage: "info.circle")
        }
    }
    .padding(20)
    .frame(width: 620, height: 400)
  }
}

private struct GeneralSettingsView: View {
  @ObservedObject var accessibility: AccessibilityAuthorization
  @ObservedObject var settings: SettingsStore

  private var launchAtLoginBinding: Binding<Bool> {
    Binding(
      get: { settings.launchAtLogin },
      set: { settings.setLaunchAtLogin($0) }
    )
  }

  var body: some View {
    Form {
      Section {
        Toggle("Bật bộ gõ tiếng Việt", isOn: $settings.isEnabled)

        Picker("Kiểu gõ", selection: $settings.method) {
          ForEach(TypingMethod.allCases) { method in
            Text(method.title).tag(method)
          }
        }
        .pickerStyle(.segmented)

        Toggle("Tự động chạy khi đăng nhập", isOn: launchAtLoginBinding)

        if let message = settings.launchAtLoginError {
          Text(message)
            .font(.caption)
            .foregroundStyle(.red)
        }
      } header: {
        Text("Bộ gõ")
      } footer: {
        Text("Thiết lập được lưu cục bộ trên máy và áp dụng cho các phiên gõ mới.")
      }

      Section {
        HStack {
          Text("Bật/Tắt gõ tiếng Việt")
          Spacer()
          ShortcutRecorderButton(shortcut: settings.toggleInputShortcut) {
            settings.setShortcut($0, for: .toggleInput)
          }
        }
        HStack {
          Text("Đổi kiểu gõ")
          Spacer()
          ShortcutRecorderButton(shortcut: settings.switchMethodShortcut) {
            settings.setShortcut($0, for: .switchMethod)
          }
        }
      } header: {
        Text("Phím nóng")
      } footer: {
        Text("Nhấn Esc để hủy; bấm chuột phải vào phím nóng để xóa.")
      }

      Section {
        HStack {
          Label("Accessibility", systemImage: "hand.raised")
          Spacer()
          if accessibility.isGranted {
            Label("Đã cấp", systemImage: "checkmark.circle.fill")
              .foregroundStyle(.green)
          } else {
            Button("Mở Cài đặt hệ thống…") {
              accessibility.openSystemSettings()
            }
          }
        }
      } header: {
        Text("Quyền hệ thống")
      } footer: {
        Text("ViGo cần quyền này để nhập văn bản đã commit mà không tạo gạch chân.")
      }

      if !settings.excludedApplications.isEmpty {
        Section {
          ForEach(settings.excludedApplications) { application in
            HStack {
              VStack(alignment: .leading, spacing: 2) {
                Text(application.displayName)
                Text(application.bundleIdentifier)
                  .font(.caption)
                  .foregroundStyle(.secondary)
              }
              Spacer()
              Button {
                settings.removeExcludedApplication(
                  bundleIdentifier: application.bundleIdentifier
                )
              } label: {
                Image(systemName: "trash")
              }
              .buttonStyle(.borderless)
              .help("Bỏ khỏi danh sách loại trừ")
            }
          }
        } header: {
          Text("Ứng dụng loại trừ")
        } footer: {
          Text("Thêm ứng dụng từ menu ViGo khi ứng dụng đó đang được sử dụng.")
        }
      }
    }
    .formStyle(.grouped)
  }
}

private struct TextExpansionSettingsView: View {
  @ObservedObject var settings: SettingsStore

  var body: some View {
    VStack(alignment: .leading, spacing: 12) {
      Text("Nhập từ viết tắt và nội dung thay thế khi nhấn dấu cách.")
        .foregroundStyle(.secondary)

      GroupBox {
        VStack(spacing: 0) {
          HStack {
            Text("Phím tắt")
              .frame(width: 170, alignment: .leading)
            Text("Từ đầy đủ")
              .frame(maxWidth: .infinity, alignment: .leading)
            Color.clear.frame(width: 24)
          }
          .font(.caption.weight(.semibold))
          .foregroundStyle(.secondary)
          .padding(.horizontal, 10)
          .padding(.vertical, 8)

          Divider()

          if settings.expansions.isEmpty {
            VStack(spacing: 8) {
              Image(systemName: "text.badge.plus")
                .font(.title2)
                .foregroundStyle(.secondary)
              Text("Chưa có mục gõ tắt")
                .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
          } else {
            ScrollView {
              LazyVStack(spacing: 0) {
                ForEach($settings.expansions) { $expansion in
                  HStack {
                    TextField("vd: dc", text: $expansion.shortcut)
                      .textFieldStyle(.plain)
                      .frame(width: 170)
                    Divider().frame(height: 20)
                    TextField(
                      "vd: được",
                      text: $expansion.replacement
                    )
                    .textFieldStyle(.plain)
                    Button {
                      settings.removeExpansion(id: expansion.id)
                    } label: {
                      Image(systemName: "trash")
                    }
                    .buttonStyle(.borderless)
                    .help("Xóa mục này")
                  }
                  .padding(.horizontal, 10)
                  .padding(.vertical, 8)

                  Divider()
                }
              }
            }
          }
        }
      }

      HStack {
        Button {
          settings.addExpansion()
        } label: {
          Label("Thêm", systemImage: "plus")
        }
        Spacer()
        Text("\(settings.expansions.count) mục")
          .font(.caption)
          .foregroundStyle(.secondary)
      }
    }
  }
}

private struct AboutView: View {
  var body: some View {
    VStack(spacing: 14) {
      Image(systemName: "keyboard.fill")
        .font(.system(size: 64))
        .foregroundStyle(.tint)

      Text("ViGo")
        .font(.largeTitle.bold())

      Text("Phiên bản 2.0 · M1")
        .foregroundStyle(.secondary)

      Text("Bộ gõ tiếng Việt riêng tư cho macOS, Windows và Linux.")
        .multilineTextAlignment(.center)

      Link("Mã nguồn và tài liệu", destination: URL(string: "https://github.com/huyphan/ViGo")!)

      Text("GPL-3.0-or-later · Xử lý phím hoàn toàn trên thiết bị")
        .font(.caption)
        .foregroundStyle(.secondary)
    }
    .frame(maxWidth: .infinity, maxHeight: .infinity)
  }
}
