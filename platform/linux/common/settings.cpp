#include "settings.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace vigo::linux
{
namespace
{

constexpr std::size_t MaxSettingsBytes = 1024 * 1024;
constexpr std::size_t MaxEntries = 256;

[[nodiscard]] std::filesystem::path SettingsPath()
{
    if (const char *override_path = std::getenv("VIGO_CONFIG_FILE");
        override_path != nullptr && *override_path != '\0')
    {
        return override_path;
    }
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0')
    {
        return std::filesystem::path(xdg) / "vigo" / "settings.conf";
    }
    if (const char *user_home = std::getenv("HOME"); user_home != nullptr && *user_home != '\0')
    {
        return std::filesystem::path(user_home) / ".config" / "vigo" / "settings.conf";
    }
    return {};
}

} // namespace

SettingsSnapshot ParseSettings(std::string_view contents)
{
    SettingsSnapshot settings;
    if (contents.size() > MaxSettingsBytes)
    {
        return settings;
    }

    std::size_t offset = 0;
    while (offset <= contents.size())
    {
        const std::size_t end = contents.find('\n', offset);
        std::string_view line = contents.substr(
            offset, end == std::string_view::npos ? contents.size() - offset : end - offset);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        if (line.starts_with("version=") && line != "version=1")
        {
            return {};
        }
        if (line == "enabled=false")
        {
            settings.enabled = false;
        }
        else if (line == "enabled=true")
        {
            settings.enabled = true;
        }
        else if (line.starts_with("expansion=") && settings.expansions.size() < MaxEntries)
        {
            line.remove_prefix(std::string_view("expansion=").size());
            const std::size_t separator = line.find('\t');
            if (separator > 0 && separator != std::string_view::npos && separator + 1 < line.size())
            {
                settings.expansions.push_back({
                    .trigger = std::string(line.substr(0, separator)),
                    .replacement = std::string(line.substr(separator + 1)),
                });
            }
        }
        else if (line.starts_with("excluded=") &&
                 settings.excluded_applications.size() < MaxEntries)
        {
            line.remove_prefix(std::string_view("excluded=").size());
            if (!line.empty())
            {
                settings.excluded_applications.emplace_back(line);
            }
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        offset = end + 1;
    }
    return settings;
}

SettingsSnapshot LoadSettings()
{
    try
    {
        const auto path = SettingsPath();
        if (path.empty())
        {
            return {};
        }
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return {};
        }
        std::string contents;
        contents.resize(MaxSettingsBytes + 1);
        input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
        contents.resize(static_cast<std::size_t>(input.gcount()));
        if (contents.size() > MaxSettingsBytes)
        {
            return {};
        }
        return ParseSettings(contents);
    }
    catch (...)
    {
        return {};
    }
}

bool IsApplicationExcluded(const SettingsSnapshot &settings,
                           std::string_view application) noexcept
{
    if (application.empty())
    {
        return false;
    }
    const auto separator = application.rfind(':');
    const std::string_view normalized =
        separator != std::string_view::npos && separator + 1 < application.size()
            ? application.substr(separator + 1)
            : application;
    return std::any_of(settings.excluded_applications.begin(),
                       settings.excluded_applications.end(),
                       [application, normalized](const std::string &excluded) {
                           return excluded == application || excluded == normalized;
                       });
}

} // namespace vigo::linux
