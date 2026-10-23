#pragma once

#include "composition_session.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vigo::linux
{

struct SettingsSnapshot
{
    bool enabled = true;
    std::vector<TextExpansion> expansions;
    std::vector<std::string> excluded_applications;
};

// Linux settings use a bounded, line-oriented v1 file. Adapters load a whole
// immutable snapshot only at focus/activation boundaries, never per key.
[[nodiscard]] SettingsSnapshot ParseSettings(std::string_view contents);
[[nodiscard]] SettingsSnapshot LoadSettings();
[[nodiscard]] bool IsApplicationExcluded(const SettingsSnapshot &settings,
                                         std::string_view application) noexcept;

} // namespace vigo::linux
