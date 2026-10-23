#pragma once

#include "composition_session.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vigo::windows
{

struct HotKey
{
    std::uint32_t modifiers = 0;
    std::uint32_t virtual_key = 0;

    [[nodiscard]] bool enabled() const noexcept
    {
        return modifiers != 0 && virtual_key != 0;
    }
};

struct SettingsSnapshot
{
    bool enabled = true;
    TypingMethod method = TypingMethod::Telex;
    std::vector<TextExpansion> expansions;
    std::vector<std::wstring> excluded_executables;
    HotKey toggle_input_hot_key;
    HotKey switch_method_hot_key;
};

// Settings are local to the current user. The TSF service loads a complete
// snapshot only at activation or context changes, never in the core key path.
[[nodiscard]] SettingsSnapshot LoadSettings();
[[nodiscard]] bool SaveSettings(const SettingsSnapshot &settings) noexcept;

[[nodiscard]] bool IsCurrentProcessExcluded(const SettingsSnapshot &settings) noexcept;
[[nodiscard]] std::wstring CurrentProcessExecutableName();

[[nodiscard]] bool IsLaunchAtLoginEnabled() noexcept;
[[nodiscard]] bool SetLaunchAtLogin(bool enabled, std::wstring_view executable_path) noexcept;

[[nodiscard]] std::optional<std::string> WideToUtf8(std::wstring_view value);
[[nodiscard]] std::optional<std::wstring> Utf8ToWide(std::string_view value);

} // namespace vigo::windows
