#include "settings.h"

#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace nakl::windows
{
namespace
{

constexpr wchar_t SettingsPath[] = L"Software\\NAKL";
constexpr wchar_t RunPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RunValue[] = L"NAKL";
constexpr std::size_t MaxSettingsBytes = 1024 * 1024;
constexpr std::size_t MaxExpansionCount = 256;
constexpr std::size_t MaxExcludedCount = 256;
constexpr DWORD AllowedHotKeyModifiers = MOD_ALT | MOD_CONTROL | MOD_SHIFT;

class RegistryKey final
{
  public:
    RegistryKey() = default;
    ~RegistryKey()
    {
        if (key_ != nullptr)
        {
            RegCloseKey(key_);
        }
    }

    RegistryKey(const RegistryKey &) = delete;
    RegistryKey &operator=(const RegistryKey &) = delete;

    [[nodiscard]] HKEY *Put() noexcept
    {
        return &key_;
    }

    [[nodiscard]] HKEY Get() const noexcept
    {
        return key_;
    }

  private:
    HKEY key_ = nullptr;
};

[[nodiscard]] std::optional<DWORD> ReadDword(HKEY key, const wchar_t *name)
{
    DWORD value = 0;
    DWORD size = static_cast<DWORD>(sizeof(value));
    DWORD type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE *>(&value), &size) !=
            ERROR_SUCCESS ||
        type != REG_DWORD || size != sizeof(value))
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::vector<std::wstring> ReadMultiString(HKEY key, const wchar_t *name,
                                                        std::size_t limit)
{
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        type != REG_MULTI_SZ || bytes < sizeof(wchar_t) || bytes > MaxSettingsBytes ||
        bytes % sizeof(wchar_t) != 0)
    {
        return {};
    }

    std::vector<wchar_t> storage((bytes / sizeof(wchar_t)) + 1, L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE *>(storage.data()),
                         &bytes) != ERROR_SUCCESS)
    {
        return {};
    }

    std::vector<std::wstring> values;
    const wchar_t *cursor = storage.data();
    const wchar_t *end = storage.data() + storage.size();
    while (cursor < end && *cursor != L'\0' && values.size() < limit)
    {
        const std::size_t remaining = static_cast<std::size_t>(end - cursor);
        const std::size_t length = wcsnlen_s(cursor, remaining);
        if (length == remaining)
        {
            return {};
        }
        values.emplace_back(cursor, length);
        cursor += length + 1;
    }
    return values;
}

[[nodiscard]] bool WriteDword(HKEY key, const wchar_t *name, DWORD value) noexcept
{
    return RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&value),
                          static_cast<DWORD>(sizeof(value))) == ERROR_SUCCESS;
}

[[nodiscard]] bool WriteMultiString(HKEY key, const wchar_t *name,
                                    const std::vector<std::wstring> &values)
{
    std::vector<wchar_t> storage;
    std::size_t units = 1;
    for (const auto &value : values)
    {
        if (value.find(L'\0') != std::wstring::npos ||
            value.size() > (MaxSettingsBytes / sizeof(wchar_t)) - units - 1)
        {
            return false;
        }
        units += value.size() + 1;
    }
    if (units > MaxSettingsBytes / sizeof(wchar_t))
    {
        return false;
    }

    storage.reserve(units);
    for (const auto &value : values)
    {
        storage.insert(storage.end(), value.begin(), value.end());
        storage.push_back(L'\0');
    }
    if (storage.empty())
    {
        storage.push_back(L'\0');
    }
    storage.push_back(L'\0');
    return RegSetValueExW(key, name, 0, REG_MULTI_SZ,
                          reinterpret_cast<const BYTE *>(storage.data()),
                          static_cast<DWORD>(storage.size() * sizeof(wchar_t))) == ERROR_SUCCESS;
}

[[nodiscard]] bool CaseInsensitiveEqual(std::wstring_view left, std::wstring_view right) noexcept
{
    return left.size() == right.size() && _wcsnicmp(left.data(), right.data(), left.size()) == 0;
}

[[nodiscard]] bool IsValidHotKey(HotKey hot_key) noexcept
{
    const bool valid_modifiers = (hot_key.modifiers & ~AllowedHotKeyModifiers) == 0 &&
                                 (hot_key.modifiers & (MOD_ALT | MOD_CONTROL)) != 0;
    const bool valid_key = (hot_key.virtual_key >= 'A' && hot_key.virtual_key <= 'Z') ||
                           (hot_key.virtual_key >= VK_F1 && hot_key.virtual_key <= VK_F12);
    return valid_modifiers && valid_key;
}

[[nodiscard]] HotKey ReadHotKey(HKEY key, const wchar_t *modifiers_name, const wchar_t *key_name,
                                HotKey fallback)
{
    HotKey hot_key{
        .modifiers = ReadDword(key, modifiers_name).value_or(fallback.modifiers),
        .virtual_key = ReadDword(key, key_name).value_or(fallback.virtual_key),
    };
    return IsValidHotKey(hot_key) ? hot_key : fallback;
}

} // namespace

std::optional<std::string> WideToUtf8(std::wstring_view value)
{
    if (value.empty())
    {
        return std::string{};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::nullopt;
    }
    const int size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        return std::nullopt;
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), size, nullptr,
                            nullptr) != size)
    {
        return std::nullopt;
    }
    return result;
}

std::optional<std::wstring> Utf8ToWide(std::string_view value)
{
    if (value.empty())
    {
        return std::wstring{};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::nullopt;
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0)
    {
        return std::nullopt;
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), size) != size)
    {
        return std::nullopt;
    }
    return result;
}

SettingsSnapshot LoadSettings()
{
    SettingsSnapshot settings;
    settings.toggle_input_hot_key = {
        .modifiers = MOD_CONTROL | MOD_ALT,
        .virtual_key = 'V',
    };
    settings.switch_method_hot_key = {
        .modifiers = MOD_CONTROL | MOD_ALT,
        .virtual_key = 'M',
    };
    RegistryKey key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, SettingsPath, 0, KEY_QUERY_VALUE, key.Put()) !=
        ERROR_SUCCESS)
    {
        return settings;
    }

    settings.enabled = ReadDword(key.Get(), L"Enabled").value_or(1) != 0;
    settings.method = ReadDword(key.Get(), L"TypingMethod").value_or(1) == 2 ? TypingMethod::Vni
                                                                             : TypingMethod::Telex;

    for (const auto &entry : ReadMultiString(key.Get(), L"TextExpansions", MaxExpansionCount))
    {
        const auto separator = entry.find(L'\t');
        if (separator == std::wstring::npos || separator == 0)
        {
            continue;
        }
        auto trigger = WideToUtf8(std::wstring_view(entry).substr(0, separator));
        auto replacement = WideToUtf8(std::wstring_view(entry).substr(separator + 1));
        if (trigger.has_value() && replacement.has_value())
        {
            settings.expansions.push_back(
                {.trigger = std::move(*trigger), .replacement = std::move(*replacement)});
        }
    }
    settings.excluded_executables =
        ReadMultiString(key.Get(), L"ExcludedExecutables", MaxExcludedCount);
    settings.toggle_input_hot_key =
        ReadHotKey(key.Get(), L"ToggleHotKeyModifiers", L"ToggleHotKeyVirtualKey",
                   settings.toggle_input_hot_key);
    settings.switch_method_hot_key =
        ReadHotKey(key.Get(), L"SwitchHotKeyModifiers", L"SwitchHotKeyVirtualKey",
                   settings.switch_method_hot_key);
    return settings;
}

bool SaveSettings(const SettingsSnapshot &settings) noexcept
{
    try
    {
        if (settings.expansions.size() > MaxExpansionCount ||
            settings.excluded_executables.size() > MaxExcludedCount ||
            !IsValidHotKey(settings.toggle_input_hot_key) ||
            !IsValidHotKey(settings.switch_method_hot_key) ||
            (settings.toggle_input_hot_key.modifiers == settings.switch_method_hot_key.modifiers &&
             settings.toggle_input_hot_key.virtual_key ==
                 settings.switch_method_hot_key.virtual_key))
        {
            return false;
        }
        std::vector<std::wstring> expansions;
        expansions.reserve(settings.expansions.size());
        for (const auto &expansion : settings.expansions)
        {
            auto trigger = Utf8ToWide(expansion.trigger);
            auto replacement = Utf8ToWide(expansion.replacement);
            if (!trigger.has_value() || !replacement.has_value() || trigger->empty() ||
                trigger->find(L'\t') != std::wstring::npos ||
                replacement->find(L'\t') != std::wstring::npos)
            {
                return false;
            }
            expansions.push_back(*trigger + L'\t' + *replacement);
        }

        RegistryKey key;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, SettingsPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                            key.Put(), nullptr) != ERROR_SUCCESS)
        {
            return false;
        }
        return WriteDword(key.Get(), L"TypingMethod",
                          settings.method == TypingMethod::Vni ? 2 : 1) &&
               WriteMultiString(key.Get(), L"TextExpansions", expansions) &&
               WriteMultiString(key.Get(), L"ExcludedExecutables", settings.excluded_executables) &&
               WriteDword(key.Get(), L"ToggleHotKeyModifiers",
                          settings.toggle_input_hot_key.modifiers) &&
               WriteDword(key.Get(), L"ToggleHotKeyVirtualKey",
                          settings.toggle_input_hot_key.virtual_key) &&
               WriteDword(key.Get(), L"SwitchHotKeyModifiers",
                          settings.switch_method_hot_key.modifiers) &&
               WriteDword(key.Get(), L"SwitchHotKeyVirtualKey",
                          settings.switch_method_hot_key.virtual_key) &&
               WriteDword(key.Get(), L"Enabled", settings.enabled ? 1 : 0);
    }
    catch (...)
    {
        return false;
    }
}

std::wstring CurrentProcessExecutableName()
{
    std::vector<wchar_t> path(260);
    while (path.size() <= 32768)
    {
        const DWORD length =
            GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
        {
            return {};
        }
        if (length < path.size())
        {
            const std::wstring_view full(path.data(), length);
            const auto separator = full.find_last_of(L"\\/");
            return std::wstring(
                full.substr(separator == std::wstring_view::npos ? 0 : separator + 1));
        }
        path.resize(path.size() * 2);
    }
    return {};
}

bool IsCurrentProcessExcluded(const SettingsSnapshot &settings) noexcept
{
    try
    {
        const std::wstring executable = CurrentProcessExecutableName();
        return !executable.empty() &&
               std::any_of(settings.excluded_executables.begin(),
                           settings.excluded_executables.end(), [&](const auto &candidate) {
                               return CaseInsensitiveEqual(executable, candidate);
                           });
    }
    catch (...)
    {
        return true;
    }
}

bool IsLaunchAtLoginEnabled() noexcept
{
    RegistryKey key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RunPath, 0, KEY_QUERY_VALUE, key.Put()) != ERROR_SUCCESS)
    {
        return false;
    }
    return RegQueryValueExW(key.Get(), RunValue, nullptr, nullptr, nullptr, nullptr) ==
           ERROR_SUCCESS;
}

bool SetLaunchAtLogin(bool enabled, std::wstring_view executable_path) noexcept
{
    RegistryKey key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RunPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                        key.Put(), nullptr) != ERROR_SUCCESS)
    {
        return false;
    }
    if (!enabled)
    {
        const LSTATUS status = RegDeleteValueW(key.Get(), RunValue);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    try
    {
        const std::wstring command = L"\"" + std::wstring(executable_path) + L"\"";
        return RegSetValueExW(
                   key.Get(), RunValue, 0, REG_SZ, reinterpret_cast<const BYTE *>(command.c_str()),
                   static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }
    catch (...)
    {
        return false;
    }
}

} // namespace nakl::windows
