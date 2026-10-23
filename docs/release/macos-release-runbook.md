# macOS Release Runbook

Runbook này mô tả cách tạo, ký, notarize và kiểm thử một bản ViGo `.pkg` phát hành ngoài Mac App Store. Thực hiện từ repository sạch trên máy macOS có Xcode và quyền Apple Developer phù hợp. Không đưa certificate, private key, Apple ID, app-specific password hoặc notary credential vào repository.

## Trạng thái hiện tại

**P0 — macOS implementation và release preparation: HOÀN TẤT (2026-08-26).** Việc thực thi production release được hoãn cho đến khi ứng dụng được ưu tiên phát hành.

Máy phát triển hiện có identity `Apple Development`, đủ cho bản test local. Bản phát hành còn cần `Developer ID Application`, `Developer ID Installer` và notary profile. Package unsigned đã vượt structural verification; điều đó không thay thế signing, notarization hoặc Gatekeeper verification. Khi tiếp tục release, bắt đầu từ checklist trong tài liệu này thay vì mở lại P0 implementation.

## 1. Chuẩn bị một lần

Yêu cầu:

- Xcode command-line tools và Rust 1.98.0 theo `rust-toolchain.toml`.
- Rust targets `aarch64-apple-darwin` và `x86_64-apple-darwin`.
- Apple Developer Program membership với quyền tạo Developer ID certificates.
- `Developer ID Application` để ký `.app` và `Developer ID Installer` để ký `.pkg`.

Kiểm tra toolchain và identities:

```sh
xcode-select -p
xcodebuild -version
rustup show active-toolchain
rustup target add aarch64-apple-darwin x86_64-apple-darwin
security find-identity -v -p codesigning
security find-certificate -a -c "Developer ID Installer"
```

Tạo hai certificate trong Apple Developer → Certificates, Identifiers & Profiles → Certificates → Developer ID, sau đó cài certificate cùng private key vào Keychain. Apple yêu cầu Account Holder hoặc quyền tương ứng để tạo certificate.

Lưu notary credential vào Keychain; không truyền password trực tiếp trên command line. Lệnh sau sẽ hỏi app-specific password bằng secure prompt:

```sh
xcrun notarytool store-credentials ViGo-notary \
  --apple-id "APPLE_ID" \
  --team-id "TEAM_ID"
```

## 2. Chuẩn bị release candidate

Làm việc trên commit đã review và không có thay đổi chưa commit:

```sh
git status --short
git rev-parse HEAD
cargo test --workspace
cargo fmt --all --check
cargo clippy --workspace --all-targets -- -D warnings
swift test --package-path platform/macos
swift format lint --recursive --strict \
  platform/macos/Sources platform/macos/Tests
```

Cập nhật `CFBundleShortVersionString` trong `platform/macos/Resources/Info.plist` và commit thay đổi version trước khi build. Chọn `VIGO_BUILD_NUMBER` dạng số thập phân, lớn hơn mọi build đã phát hành. Luôn đặt biến này rõ ràng cho release; không dựa vào số lượng Git commit vì lịch sử có thể được rebase.

Chạy corpus, property tests và fuzz campaign phù hợp với release gate. Mọi khác biệt hành vi phải được review trong `tests/golden/` trước khi phát hành.

## 3. Build và ký package

Điền chính xác tên identity hiển thị bởi Keychain:

```sh
export VIGO_BUILD_NUMBER="REPLACE_WITH_MONOTONIC_NUMBER"
export VIGO_CODESIGN_IDENTITY="Developer ID Application: NAME (TEAM_ID)"
export VIGO_INSTALLER_IDENTITY="Developer ID Installer: NAME (TEAM_ID)"
export VIGO_NOTARY_PROFILE="ViGo-notary"

platform/macos/scripts/package-app.sh
```

Artifact được tạo tại `dist/ViGo-<version>-<build>.pkg`; app nguồn nằm tại `build/ViGo.app`. Script bật Hardened Runtime, yêu cầu secure timestamp và từ chối tạo signed package nếu app không dùng Developer ID Application.

Đặt đường dẫn artifact cho các bước sau:

```sh
export VIGO_APP_PATH="build/ViGo.app"
export VIGO_PACKAGE_PATH="dist/ViGo-<version>-<build>.pkg"
```

## 4. Structural verification và notarization

Kiểm tra payload trước khi gửi Apple:

```sh
platform/macos/scripts/verify-package-structure.sh \
  "$VIGO_APP_PATH" "$VIGO_PACKAGE_PATH"
```

Gate này xác nhận arm64/x86_64, macOS 13 deployment target, bundle/package identifiers, version, payload giống app nguồn, system-wide install domain và logout requirement.

Gửi package tới Apple, lưu submission result/log dưới `dist/notarization/`, staple ticket và chạy Gatekeeper:

```sh
platform/macos/scripts/notarize.sh "$VIGO_PACKAGE_PATH"
platform/macos/scripts/verify-release.sh \
  "$VIGO_APP_PATH" "$VIGO_PACKAGE_PATH"
```

Không phát hành nếu `notarytool`, `stapler`, `codesign`, `pkgutil` hoặc `spctl` thất bại. Xem JSON log trong `dist/notarization/` để xử lý lỗi notarization.

## 5. Kiểm thử lifecycle trên máy sạch

Dùng máy ảo hoặc tài khoản macOS dùng riêng cho QA; chỉ tạo dữ liệu gõ tắt tổng hợp. Kiểm tra lần lượt:

1. Clean install: `sudo installer -pkg "$VIGO_PACKAGE_PATH" -target /`, sau đó logout/login.
2. Xác nhận bundle tại `/Library/Input Methods/ViGo.app`, receipt `com.gosoln.inputmethod.ViGo.pkg`, menu-bar UI và Accessibility authorization.
3. Chạy matrix trong `docs/testing/macos-compatibility.md`: Telex, VNI, `giai+r`, `cha+f`, không underline trong Chrome, secure pass-through, gõ tắt, hotkeys và excluded apps.
4. Upgrade từ bản đã phát hành trước; xác nhận settings tổng hợp được giữ nguyên.
5. Thử rollback/recovery khi upgrade thất bại; không làm mất bundle đang hoạt động hoặc preferences.
6. Tắt launch-at-login, logout để dừng input method, rồi chạy `platform/macos/scripts/uninstall-system.sh`. Xác nhận ABC được khôi phục, receipt bị quên và recovery bundle được giữ lại.

Lưu ý: `smoke-installed.sh` hiện kiểm tra bản user-local tại `~/Library/Input Methods/ViGo.app`. Không dùng kết quả đó để chứng nhận package system-wide; cần chạy matrix trên `/Library/Input Methods/ViGo.app` hoặc cập nhật smoke script trước release.

## 6. Hoàn tất artifact

Tạo checksum và ghi lại commit, version, build, macOS/Xcode version, kiến trúc, notary submission ID và kết quả compatibility matrix:

```sh
shasum -a 256 "$VIGO_PACKAGE_PATH" > "$VIGO_PACKAGE_PATH.sha256"
```

Chỉ tag release sau khi mọi gate đều PASS. Giữ `.pkg`, checksum và notarization log trong release storage; `build/`, `dist/` và credentials không được commit. Không ghi keystroke thật, secure-field content hoặc dữ liệu gõ tắt của người dùng vào log.

## Release Checklist

- [ ] Repository sạch; test, format, lint, corpus và fuzz PASS.
- [ ] Marketing version đã commit; build number tăng đơn điệu.
- [ ] App dùng Developer ID Application, Hardened Runtime và timestamp.
- [ ] Package dùng Developer ID Installer.
- [ ] Structural verification PASS.
- [ ] Notarization accepted và ticket đã staple.
- [ ] `verify-release.sh` và Gatekeeper PASS.
- [ ] Clean install, upgrade, recovery và uninstall PASS trên máy sạch.
- [ ] Client matrix của committed-input candidate PASS.
- [ ] Checksum, test record và release notes đã lưu.

## Apple References

- [Create Developer ID certificates](https://developer.apple.com/help/account/certificates/create-developer-id-certificates)
- [Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
