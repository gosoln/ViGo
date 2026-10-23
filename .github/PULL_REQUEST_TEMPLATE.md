## Tóm tắt / Summary

<!-- Thay đổi gì và vì sao. What changed and why. -->

## Loại thay đổi / Type

- [ ] Sửa lỗi / Bug fix
- [ ] Tính năng / Feature
- [ ] Hành vi gõ (cần cập nhật `tests/golden/`) / Composition behavior
- [ ] Thay đổi C ABI / C ABI change
- [ ] Tài liệu, CI / Docs, CI

## Kiểm tra / Checks

- [ ] `cargo fmt --all --check`
- [ ] `cargo clippy --workspace --all-targets -- -D warnings`
- [ ] `cargo test --workspace`
- [ ] `swift format lint --recursive --strict platform/macos/Sources platform/macos/Tests` (nếu đổi macOS)
- [ ] Không có nội dung phím bấm hay dữ liệu cá nhân trong log, test hoặc ảnh chụp

Issue liên quan / Related issue: #
