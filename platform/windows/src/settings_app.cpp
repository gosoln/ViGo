#include "settings.h"

// windows.h must precede the other Windows headers; the blank line keeps tools from reordering.
#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <string>
#include <string_view>
#include <vector>

namespace vigo::windows
{
namespace
{

constexpr wchar_t WindowClassName[] = L"ViGoSettingsWindow";
constexpr int AppIconResourceId = 1; // matches resources/vigo.rc
constexpr UINT TrayMessage = WM_APP + 1;
constexpr UINT ShowSettingsMessage = WM_APP + 2;
constexpr UINT TrayId = 1;
constexpr UINT BaseDpi = USER_DEFAULT_SCREEN_DPI;

// Windows lists an enabled keyboard as a value named "<LANGID>:{CLSID}{ProfileGUID}" under
// HKCU\Control Panel\International\User Profile\<language tag>. These identifiers match the
// text service and profile that registration.cpp installs.
constexpr wchar_t InputMethodTip[] =
    L"042A:{027A6B19-4551-46F7-B8FD-FB09AE4135BA}{6DFD5E1A-DE07-49D4-B3AE-BD420B160656}";
constexpr wchar_t UserProfileKeyPath[] = L"Control Panel\\International\\User Profile";
constexpr wchar_t EnableInputMethodArguments[] =
    L"-NoProfile -ExecutionPolicy Bypass -Command \""
    L"$tip='042A:{027A6B19-4551-46F7-B8FD-FB09AE4135BA}{6DFD5E1A-DE07-49D4-B3AE-BD420B160656}'; "
    L"$list=Get-WinUserLanguageList; "
    L"$vi=$list | Where-Object { $_.LanguageTag -like 'vi*' }; "
    L"if ($null -eq $vi) { $vi=(New-WinUserLanguageList vi)[0]; $vi.InputMethodTips.Clear(); "
    L"$list.Add($vi) }; "
    L"if (-not $vi.InputMethodTips.Contains($tip)) { $vi.InputMethodTips.Add($tip) }; "
    L"Set-WinUserLanguageList $list -Force\"";
constexpr DWORD EnableInputMethodTimeoutMs = 120'000;

enum ControlId : int
{
    Tab = 100,
    Enabled = 110,
    Telex = 111,
    Vni = 112,
    LaunchAtLogin = 113,
    ExcludedEdit = 114,
    ExcludedAdd = 115,
    ExcludedList = 116,
    ExcludedRemove = 117,
    ToggleHotKeyModifiers = 130,
    ToggleHotKeyVirtualKey = 131,
    SwitchHotKeyModifiers = 132,
    SwitchHotKeyVirtualKey = 133,
    ApplyHotKeys = 134,
    HotKeyStatus = 135,
    InputMethodStatus = 140,
    EnableInputMethod = 141,
    OpenLanguageSettings = 142,
    ExpansionShortcut = 120,
    ExpansionReplacement = 121,
    ExpansionAdd = 122,
    ExpansionList = 123,
    ExpansionRemove = 124,
    MenuTelex = 200,
    MenuVni = 201,
    MenuDisabled = 202,
    MenuSettings = 203,
    MenuExit = 204,
    MenuState = 205,
};

enum class Page : std::size_t
{
    General = 0,
    Expansions = 1,
    System = 2,
    About = 3,
};
constexpr std::size_t PageCount = 4;

enum class FontRole
{
    Message,
    Title,
};

// Logical layout at 96 DPI. Every value is scaled with the window DPI when applied.
constexpr int WindowMargin = 12;
constexpr int PagePadding = 12;
constexpr int ContentWidth = 540;
constexpr int TabWidth = ContentWidth + 2 * PagePadding + 4;
constexpr int TabHeight = 416;
constexpr int DropDownHeight = 200;
constexpr int ExpansionTriggerColumnWidth = 160;

struct ControlLayout
{
    HWND handle = nullptr;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    FontRole font = FontRole::Message;
};

struct AppState
{
    HWND window = nullptr;
    HWND tab = nullptr;
    UINT dpi = BaseDpi;
    HFONT message_font = nullptr;
    HFONT title_font = nullptr;
    std::array<std::vector<HWND>, PageCount> pages;
    std::vector<ControlLayout> layouts;
    COLORREF input_method_status_color = 0;
    bool enabling_input_method = false;
    bool tray_added = false;
    SettingsSnapshot settings;
    NOTIFYICONDATAW tray{};
};

constexpr int ToggleHotKeyId = 1;
constexpr int SwitchHotKeyId = 2;

[[nodiscard]] int Scale(const AppState &state, int value)
{
    return MulDiv(value, static_cast<int>(state.dpi), static_cast<int>(BaseDpi));
}

[[nodiscard]] std::wstring WindowText(HWND control)
{
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<std::size_t>(std::max(length, 0)) + 1, L'\0');
    if (length > 0)
    {
        const int copied = GetWindowTextW(control, value.data(), length + 1);
        value.resize(static_cast<std::size_t>(std::max(copied, 0)));
    }
    else
    {
        value.clear();
    }
    return value;
}

// The system message font (Segoe UI 9pt on Windows 10/11) at the requested DPI.
[[nodiscard]] HFONT CreateMessageFont(UINT dpi, int size_percent, int weight)
{
    LOGFONTW font{};
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi) !=
        FALSE)
    {
        font = metrics.lfMessageFont;
    }
    else
    {
        wcscpy_s(font.lfFaceName, L"Segoe UI");
        font.lfHeight = -MulDiv(9, static_cast<int>(dpi), 72);
        font.lfCharSet = DEFAULT_CHARSET;
    }
    font.lfHeight = MulDiv(font.lfHeight, size_percent, 100);
    if (weight != 0)
    {
        font.lfWeight = weight;
    }
    font.lfQuality = CLEARTYPE_QUALITY;
    HFONT created = CreateFontIndirectW(&font);
    return created != nullptr ? created : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

[[nodiscard]] HFONT FontFor(const AppState &state, FontRole role)
{
    return role == FontRole::Title ? state.title_font : state.message_font;
}

void ApplyFonts(AppState &state)
{
    SendMessageW(state.tab, WM_SETFONT, reinterpret_cast<WPARAM>(state.message_font), TRUE);
    for (const ControlLayout &layout : state.layouts)
    {
        SendMessageW(layout.handle, WM_SETFONT,
                     reinterpret_cast<WPARAM>(FontFor(state, layout.font)), TRUE);
    }
}

void RecreateFonts(AppState &state)
{
    HFONT previous_message = state.message_font;
    HFONT previous_title = state.title_font;
    state.message_font = CreateMessageFont(state.dpi, 100, 0);
    state.title_font = CreateMessageFont(state.dpi, 240, FW_SEMIBOLD);
    ApplyFonts(state);
    // Stock objects ignore DeleteObject, so the fallback font is safe here.
    if (previous_message != nullptr)
    {
        DeleteObject(previous_message);
    }
    if (previous_title != nullptr)
    {
        DeleteObject(previous_title);
    }
}

// Positions every control from the logical layout and sizes the frame to fit. `origin` is the
// new top-left corner (used for WM_DPICHANGED); null keeps the current position.
void ApplyLayout(AppState &state, const POINT *origin)
{
    const int margin = Scale(state, WindowMargin);
    const int tab_width = Scale(state, TabWidth);
    const int tab_height = Scale(state, TabHeight);
    SetWindowPos(state.tab, nullptr, margin, margin, tab_width, tab_height,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    RECT display{0, 0, tab_width, tab_height};
    TabCtrl_AdjustRect(state.tab, FALSE, &display);
    const int page_x = margin + display.left + Scale(state, PagePadding);
    const int page_y = margin + display.top + Scale(state, PagePadding);
    for (const ControlLayout &layout : state.layouts)
    {
        SetWindowPos(layout.handle, nullptr, page_x + Scale(state, layout.x),
                     page_y + Scale(state, layout.y), Scale(state, layout.width),
                     Scale(state, layout.height), SWP_NOZORDER | SWP_NOACTIVATE);
    }

    HWND expansions = GetDlgItem(state.window, ExpansionList);
    ListView_SetColumnWidth(expansions, 0, Scale(state, ExpansionTriggerColumnWidth));
    ListView_SetColumnWidth(expansions, 1, LVSCW_AUTOSIZE_USEHEADER);

    RECT frame{0, 0, 2 * margin + tab_width, 2 * margin + tab_height};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(state.window, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(state.window, GWL_EXSTYLE));
    AdjustWindowRectExForDpi(&frame, style, FALSE, ex_style, state.dpi);
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | (origin == nullptr ? SWP_NOMOVE : 0u);
    SetWindowPos(state.window, nullptr, origin == nullptr ? 0 : origin->x,
                 origin == nullptr ? 0 : origin->y, frame.right - frame.left,
                 frame.bottom - frame.top, flags);
}

HWND AddControl(AppState &state, Page page, const wchar_t *class_name, const wchar_t *text,
                DWORD style, DWORD ex_style, const ControlLayout &layout, int identifier)
{
    HWND control =
        CreateWindowExW(ex_style, class_name, text, WS_CHILD | WS_CLIPSIBLINGS | style, 0, 0, 0, 0,
                        state.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
                        GetModuleHandleW(nullptr), nullptr);
    if (control != nullptr)
    {
        ControlLayout stored = layout;
        stored.handle = control;
        state.layouts.push_back(stored);
        state.pages[static_cast<std::size_t>(page)].push_back(control);
    }
    return control;
}

void ShowPage(AppState &state, int selected)
{
    for (std::size_t page = 0; page < state.pages.size(); ++page)
    {
        for (HWND control : state.pages[page])
        {
            ShowWindow(control, static_cast<int>(page) == selected ? SW_SHOW : SW_HIDE);
        }
    }
}

void AddComboItem(HWND combo, const wchar_t *text, std::uint32_t value)
{
    const LRESULT index = ComboBox_AddString(combo, text);
    if (index != CB_ERR && index != CB_ERRSPACE)
    {
        ComboBox_SetItemData(combo, index, value);
    }
}

void PopulateHotKeyCombos(AppState &state)
{
    for (int identifier : {ToggleHotKeyModifiers, SwitchHotKeyModifiers})
    {
        HWND combo = GetDlgItem(state.window, identifier);
        AddComboItem(combo, L"Ctrl+Alt", MOD_CONTROL | MOD_ALT);
        AddComboItem(combo, L"Ctrl+Shift", MOD_CONTROL | MOD_SHIFT);
        AddComboItem(combo, L"Alt+Shift", MOD_ALT | MOD_SHIFT);
        AddComboItem(combo, L"Ctrl+Alt+Shift", MOD_CONTROL | MOD_ALT | MOD_SHIFT);
    }
    for (int identifier : {ToggleHotKeyVirtualKey, SwitchHotKeyVirtualKey})
    {
        HWND combo = GetDlgItem(state.window, identifier);
        for (wchar_t key = L'A'; key <= L'Z'; ++key)
        {
            const wchar_t label[]{key, L'\0'};
            AddComboItem(combo, label, static_cast<std::uint32_t>(key));
        }
        for (int index = 1; index <= 12; ++index)
        {
            const std::wstring label = L"F" + std::to_wstring(index);
            AddComboItem(combo, label.c_str(), static_cast<std::uint32_t>(VK_F1 + index - 1));
        }
    }
}

void SelectComboValue(HWND combo, std::uint32_t value)
{
    const LRESULT count = ComboBox_GetCount(combo);
    for (LRESULT index = 0; index < count; ++index)
    {
        if (static_cast<std::uint32_t>(ComboBox_GetItemData(combo, index)) == value)
        {
            ComboBox_SetCurSel(combo, index);
            return;
        }
    }
    ComboBox_SetCurSel(combo, 0);
}

[[nodiscard]] std::uint32_t SelectedComboValue(HWND combo)
{
    const LRESULT selected = ComboBox_GetCurSel(combo);
    if (selected == CB_ERR)
    {
        return 0;
    }
    const LRESULT value = ComboBox_GetItemData(combo, selected);
    return value == CB_ERR ? 0 : static_cast<std::uint32_t>(value);
}

[[nodiscard]] HotKey HotKeyFromControls(AppState &state, int modifiers_id, int key_id)
{
    return {
        .modifiers = SelectedComboValue(GetDlgItem(state.window, modifiers_id)),
        .virtual_key = SelectedComboValue(GetDlgItem(state.window, key_id)),
    };
}

bool RegisterConfiguredHotKeys(AppState &state)
{
    UnregisterHotKey(state.window, ToggleHotKeyId);
    UnregisterHotKey(state.window, SwitchHotKeyId);

    const bool toggle_registered =
        state.settings.toggle_input_hot_key.enabled() &&
        RegisterHotKey(state.window, ToggleHotKeyId,
                       state.settings.toggle_input_hot_key.modifiers | MOD_NOREPEAT,
                       state.settings.toggle_input_hot_key.virtual_key) != FALSE;
    const bool switch_registered =
        state.settings.switch_method_hot_key.enabled() &&
        RegisterHotKey(state.window, SwitchHotKeyId,
                       state.settings.switch_method_hot_key.modifiers | MOD_NOREPEAT,
                       state.settings.switch_method_hot_key.virtual_key) != FALSE;
    if (!toggle_registered || !switch_registered)
    {
        UnregisterHotKey(state.window, ToggleHotKeyId);
        UnregisterHotKey(state.window, SwitchHotKeyId);
        SetWindowTextW(GetDlgItem(state.window, HotKeyStatus),
                       L"Phím nóng đang xung đột với ứng dụng khác.");
        return false;
    }
    SetWindowTextW(GetDlgItem(state.window, HotKeyStatus), L"Phím nóng đang hoạt động.");
    return true;
}

// Reads the per-user keyboard list that Windows shows under Settings > Time & language.
[[nodiscard]] bool IsInputMethodInstalled()
{
    HKEY profiles = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, UserProfileKeyPath, 0, KEY_READ, &profiles) !=
        ERROR_SUCCESS)
    {
        return false;
    }
    bool installed = false;
    for (DWORD index = 0; !installed; ++index)
    {
        std::array<wchar_t, 256> language{};
        DWORD length = static_cast<DWORD>(language.size());
        if (RegEnumKeyExW(profiles, index, language.data(), &length, nullptr, nullptr, nullptr,
                          nullptr) != ERROR_SUCCESS)
        {
            break;
        }
        DWORD size = 0;
        installed = RegGetValueW(profiles, language.data(), InputMethodTip, RRF_RT_ANY, nullptr,
                                 nullptr, &size) == ERROR_SUCCESS;
    }
    RegCloseKey(profiles);
    return installed;
}

bool RefreshInputMethodStatus(AppState &state)
{
    const bool installed = IsInputMethodInstalled();
    HWND status = GetDlgItem(state.window, InputMethodStatus);
    SetWindowTextW(status, installed ? L"ViGo đã có trong danh sách bàn phím Windows. "
                                       L"Nhấn Win+Space để chuyển."
                                     : L"ViGo chưa được bật trong Windows. "
                                       L"Nhấn nút bên dưới để thêm ViGo vào danh sách bàn phím.");
    state.input_method_status_color = installed ? RGB(16, 124, 16) : RGB(196, 43, 28);
    EnableWindow(GetDlgItem(state.window, EnableInputMethod), installed ? FALSE : TRUE);
    InvalidateRect(status, nullptr, TRUE);
    return installed;
}

// Adds the ViGo keyboard to the user's language list through PowerShell (non-elevated, hidden
// console) and pumps messages while waiting so the window stays responsive.
void EnableInputMethodInWindows(AppState &state)
{
    if (state.enabling_input_method)
    {
        return;
    }
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.hwnd = state.window;
    info.lpVerb = L"open";
    info.lpFile = L"powershell.exe";
    info.lpParameters = EnableInputMethodArguments;
    info.nShow = SW_HIDE;
    if (ShellExecuteExW(&info) == FALSE || info.hProcess == nullptr)
    {
        MessageBoxW(state.window,
                    L"Không thể chạy PowerShell để bật ViGo. Hãy mở Cài đặt ngôn ngữ và thêm bàn "
                    L"phím ViGo thủ công.",
                    L"ViGo", MB_OK | MB_ICONERROR);
        return;
    }

    state.enabling_input_method = true;
    HWND status = GetDlgItem(state.window, InputMethodStatus);
    EnableWindow(GetDlgItem(state.window, EnableInputMethod), FALSE);
    SetWindowTextW(status, L"Đang bật ViGo trong Windows…");
    state.input_method_status_color = GetSysColor(COLOR_GRAYTEXT);
    InvalidateRect(status, nullptr, TRUE);

    const ULONGLONG deadline = GetTickCount64() + EnableInputMethodTimeoutMs;
    bool finished = false;
    bool quitting = false;
    while (!finished && !quitting)
    {
        const ULONGLONG now = GetTickCount64();
        const DWORD remaining = now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
        const DWORD wait =
            MsgWaitForMultipleObjects(1, &info.hProcess, FALSE, remaining, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0)
        {
            finished = true;
        }
        else if (wait == WAIT_OBJECT_0 + 1)
        {
            MSG message{};
            while (!quitting && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    PostQuitMessage(static_cast<int>(message.wParam));
                    quitting = true;
                }
                else
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
        }
        else
        {
            break; // timeout or wait failure
        }
    }

    DWORD exit_code = 1;
    if (finished)
    {
        GetExitCodeProcess(info.hProcess, &exit_code);
    }
    else
    {
        TerminateProcess(info.hProcess, 1);
    }
    CloseHandle(info.hProcess);
    state.enabling_input_method = false;
    if (quitting || IsWindow(state.window) == FALSE)
    {
        return;
    }
    const bool installed = RefreshInputMethodStatus(state);
    if (!finished || exit_code != 0 || !installed)
    {
        MessageBoxW(state.window,
                    L"Không thể bật ViGo trong Windows. Hãy mở Cài đặt ngôn ngữ, chọn Tiếng Việt "
                    L"và thêm bàn phím ViGo thủ công.",
                    L"ViGo", MB_OK | MB_ICONERROR);
    }
}

void RefreshExpansionList(AppState &state)
{
    HWND list = GetDlgItem(state.window, ExpansionList);
    ListView_DeleteAllItems(list);
    int row = 0;
    for (const auto &expansion : state.settings.expansions)
    {
        auto trigger = Utf8ToWide(expansion.trigger);
        auto replacement = Utf8ToWide(expansion.replacement);
        if (trigger.has_value() && replacement.has_value())
        {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = row;
            item.pszText = trigger->data();
            if (ListView_InsertItem(list, &item) != -1)
            {
                ListView_SetItemText(list, row, 1, replacement->data());
                ++row;
            }
        }
    }
}

void RefreshExcludedList(AppState &state)
{
    HWND list = GetDlgItem(state.window, ExcludedList);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto &executable : state.settings.excluded_executables)
    {
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(executable.c_str()));
    }
}

void RefreshGeneralControls(AppState &state)
{
    Button_SetCheck(GetDlgItem(state.window, Enabled),
                    state.settings.enabled ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(state.window, Telex),
                    state.settings.method == TypingMethod::Telex ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(state.window, Vni),
                    state.settings.method == TypingMethod::Vni ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(state.window, LaunchAtLogin),
                    IsLaunchAtLoginEnabled() ? BST_CHECKED : BST_UNCHECKED);
    SelectComboValue(GetDlgItem(state.window, ToggleHotKeyModifiers),
                     state.settings.toggle_input_hot_key.modifiers);
    SelectComboValue(GetDlgItem(state.window, ToggleHotKeyVirtualKey),
                     state.settings.toggle_input_hot_key.virtual_key);
    SelectComboValue(GetDlgItem(state.window, SwitchHotKeyModifiers),
                     state.settings.switch_method_hot_key.modifiers);
    SelectComboValue(GetDlgItem(state.window, SwitchHotKeyVirtualKey),
                     state.settings.switch_method_hot_key.virtual_key);
    RefreshExcludedList(state);
}

[[nodiscard]] const wchar_t *StateLabel(const SettingsSnapshot &settings)
{
    if (!settings.enabled)
    {
        return L"Đang tắt";
    }
    return settings.method == TypingMethod::Telex ? L"Telex" : L"VNI";
}

void UpdateTrayState(AppState &state)
{
    const std::wstring tip = std::wstring(L"ViGo – ") + StateLabel(state.settings);
    wcscpy_s(state.tray.szTip, tip.c_str());
    if (state.tray_added)
    {
        Shell_NotifyIconW(NIM_MODIFY, &state.tray);
    }
}

bool Persist(AppState &state, bool refresh_hot_keys = true)
{
    if (!SaveSettings(state.settings))
    {
        MessageBoxW(state.window, L"Không thể lưu cài đặt ViGo.", L"ViGo", MB_OK | MB_ICONERROR);
        return false;
    }
    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>(L"Software\\ViGo"), SMTO_ABORTIFHUNG, 100,
                        &ignored);
    UpdateTrayState(state);
    if (refresh_hot_keys)
    {
        static_cast<void>(RegisterConfiguredHotKeys(state));
    }
    return true;
}

void ShowSettings(AppState &state)
{
    state.settings = LoadSettings();
    RefreshGeneralControls(state);
    RefreshExpansionList(state);
    RegisterConfiguredHotKeys(state);
    UpdateTrayState(state);
    if (!state.enabling_input_method)
    {
        RefreshInputMethodStatus(state);
    }
    if (const UINT dpi = GetDpiForWindow(state.window); dpi != 0 && dpi != state.dpi)
    {
        state.dpi = dpi;
        RecreateFonts(state);
        ApplyLayout(state, nullptr);
    }
    ShowWindow(state.window, SW_SHOW);
    SetForegroundWindow(state.window);
}

void ShowTrayMenu(AppState &state)
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }
    state.settings = LoadSettings();
    UpdateTrayState(state);

    const std::wstring heading = std::wstring(L"ViGo · ") + StateLabel(state.settings);
    AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, MenuState, heading.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuTelex, L"Telex");
    AppendMenuW(menu, MF_STRING, MenuVni, L"VNI");
    AppendMenuW(menu, MF_STRING, MenuDisabled, L"Tắt");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuSettings, L"Tùy chọn…");
    AppendMenuW(menu, MF_STRING, MenuExit, L"Thoát");

    const UINT selected = !state.settings.enabled                        ? MenuDisabled
                          : state.settings.method == TypingMethod::Telex ? MenuTelex
                                                                         : MenuVni;
    CheckMenuRadioItem(menu, MenuTelex, MenuDisabled, selected, MF_BYCOMMAND);
    SetMenuDefaultItem(menu, MenuSettings, FALSE);

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(state.window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0,
                   state.window, nullptr);
    PostMessageW(state.window, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void AddExpansion(AppState &state)
{
    const std::wstring shortcut = WindowText(GetDlgItem(state.window, ExpansionShortcut));
    const std::wstring replacement = WindowText(GetDlgItem(state.window, ExpansionReplacement));
    const auto trigger_utf8 = WideToUtf8(shortcut);
    const auto replacement_utf8 = WideToUtf8(replacement);
    if (!trigger_utf8.has_value() || !replacement_utf8.has_value() || trigger_utf8->empty() ||
        replacement_utf8->empty() || shortcut.find_first_of(L"\t \r\n") != std::wstring::npos)
    {
        MessageBoxW(state.window, L"Phím tắt phải là một từ không chứa khoảng trắng.", L"ViGo",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    const auto duplicate =
        std::find_if(state.settings.expansions.begin(), state.settings.expansions.end(),
                     [&](const auto &item) { return item.trigger == *trigger_utf8; });
    if (duplicate != state.settings.expansions.end())
    {
        duplicate->replacement = *replacement_utf8;
    }
    else
    {
        state.settings.expansions.push_back(
            {.trigger = *trigger_utf8, .replacement = *replacement_utf8});
    }
    if (Persist(state))
    {
        SetWindowTextW(GetDlgItem(state.window, ExpansionShortcut), L"");
        SetWindowTextW(GetDlgItem(state.window, ExpansionReplacement), L"");
        RefreshExpansionList(state);
    }
}

void AddExcludedExecutable(AppState &state)
{
    std::wstring executable = WindowText(GetDlgItem(state.window, ExcludedEdit));
    if (executable.empty() || executable.find_first_of(L"\\/\t\r\n") != std::wstring::npos)
    {
        MessageBoxW(state.window, L"Nhập tên file thực thi, ví dụ game.exe.", L"ViGo",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    const auto duplicate = std::find_if(
        state.settings.excluded_executables.begin(), state.settings.excluded_executables.end(),
        [&](const auto &item) { return _wcsicmp(item.c_str(), executable.c_str()) == 0; });
    if (duplicate == state.settings.excluded_executables.end())
    {
        state.settings.excluded_executables.push_back(std::move(executable));
    }
    if (Persist(state))
    {
        SetWindowTextW(GetDlgItem(state.window, ExcludedEdit), L"");
        RefreshExcludedList(state);
    }
}

void ApplyHotKeySettings(AppState &state)
{
    const HotKey toggle = HotKeyFromControls(state, ToggleHotKeyModifiers, ToggleHotKeyVirtualKey);
    const HotKey switch_method =
        HotKeyFromControls(state, SwitchHotKeyModifiers, SwitchHotKeyVirtualKey);
    if (!toggle.enabled() || !switch_method.enabled() ||
        (toggle.modifiers == switch_method.modifiers &&
         toggle.virtual_key == switch_method.virtual_key))
    {
        MessageBoxW(state.window, L"Hai phím nóng phải hợp lệ và khác nhau.", L"ViGo",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    const HotKey previous_toggle = state.settings.toggle_input_hot_key;
    const HotKey previous_switch = state.settings.switch_method_hot_key;
    state.settings.toggle_input_hot_key = toggle;
    state.settings.switch_method_hot_key = switch_method;
    if (!RegisterConfiguredHotKeys(state))
    {
        state.settings.toggle_input_hot_key = previous_toggle;
        state.settings.switch_method_hot_key = previous_switch;
        static_cast<void>(RegisterConfiguredHotKeys(state));
        RefreshGeneralControls(state);
        return;
    }
    if (!Persist(state, false))
    {
        state.settings.toggle_input_hot_key = previous_toggle;
        state.settings.switch_method_hot_key = previous_switch;
        static_cast<void>(RegisterConfiguredHotKeys(state));
        RefreshGeneralControls(state);
    }
}

void RemoveSelected(AppState &state, int list_identifier)
{
    HWND list = GetDlgItem(state.window, list_identifier);
    if (list_identifier == ExpansionList)
    {
        const int selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
        if (selected >= 0 && static_cast<std::size_t>(selected) < state.settings.expansions.size())
        {
            state.settings.expansions.erase(state.settings.expansions.begin() + selected);
            if (Persist(state))
            {
                RefreshExpansionList(state);
            }
        }
        return;
    }
    const LRESULT selected = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (list_identifier == ExcludedList && selected != LB_ERR &&
        static_cast<std::size_t>(selected) < state.settings.excluded_executables.size())
    {
        state.settings.excluded_executables.erase(state.settings.excluded_executables.begin() +
                                                  static_cast<std::ptrdiff_t>(selected));
        if (Persist(state))
        {
            RefreshExcludedList(state);
        }
    }
}

void AddTabItem(HWND tab, int index, const wchar_t *text)
{
    TCITEMW item{.mask = TCIF_TEXT};
    item.pszText = const_cast<wchar_t *>(text);
    TabCtrl_InsertItem(tab, index, &item);
}

HWND AddLabel(AppState &state, Page page, const wchar_t *text, int x, int y, int width, int height,
              int identifier = -1, DWORD style = SS_LEFT)
{
    return AddControl(state, page, WC_STATICW, text, style, 0,
                      {.x = x, .y = y, .width = width, .height = height}, identifier);
}

HWND AddGroupBox(AppState &state, Page page, const wchar_t *text, int y, int height)
{
    return AddControl(state, page, WC_BUTTONW, text, BS_GROUPBOX | WS_GROUP, 0,
                      {.x = 0, .y = y, .width = ContentWidth, .height = height}, -1);
}

HWND AddButton(AppState &state, Page page, const wchar_t *text, DWORD style, int x, int y,
               int width, int height, int identifier)
{
    return AddControl(state, page, WC_BUTTONW, text, style | WS_TABSTOP, 0,
                      {.x = x, .y = y, .width = width, .height = height}, identifier);
}

HWND AddEdit(AppState &state, Page page, int x, int y, int width, int height, int identifier)
{
    return AddControl(state, page, WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                      {.x = x, .y = y, .width = width, .height = height}, identifier);
}

HWND AddCombo(AppState &state, Page page, int x, int y, int width, int identifier)
{
    return AddControl(state, page, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 0,
                      {.x = x, .y = y, .width = width, .height = DropDownHeight}, identifier);
}

void CreateGeneralPage(AppState &state)
{
    constexpr Page page = Page::General;
    constexpr int inset = 12;
    constexpr int button_height = 26;

    // Điều khiển
    AddGroupBox(state, page, L"Điều khiển", 0, 88);
    AddButton(state, page, L"Bật bộ gõ tiếng Việt", BS_AUTOCHECKBOX, inset, 26, 300, 20, Enabled);
    AddLabel(state, page, L"Kiểu gõ", inset, 56, 70, 20);
    AddButton(state, page, L"Telex", BS_AUTORADIOBUTTON | WS_GROUP, inset + 80, 54, 90, 20, Telex);
    AddButton(state, page, L"VNI", BS_AUTORADIOBUTTON, inset + 180, 54, 90, 20, Vni);

    // Phím nóng
    constexpr int hot_keys_top = 100;
    AddGroupBox(state, page, L"Phím nóng", hot_keys_top, 126);
    AddLabel(state, page, L"Bật/Tắt", inset, hot_keys_top + 29, 70, 20);
    AddCombo(state, page, inset + 80, hot_keys_top + 26, 150, ToggleHotKeyModifiers);
    AddCombo(state, page, inset + 240, hot_keys_top + 26, 72, ToggleHotKeyVirtualKey);
    AddLabel(state, page, L"Đổi kiểu", inset, hot_keys_top + 61, 70, 20);
    AddCombo(state, page, inset + 80, hot_keys_top + 58, 150, SwitchHotKeyModifiers);
    AddCombo(state, page, inset + 240, hot_keys_top + 58, 72, SwitchHotKeyVirtualKey);
    AddButton(state, page, L"Áp dụng", BS_PUSHBUTTON, ContentWidth - inset - 96, hot_keys_top + 56,
              96, button_height, ApplyHotKeys);
    AddLabel(state, page, L"", inset, hot_keys_top + 94, ContentWidth - 2 * inset, 20,
             HotKeyStatus);

    // Trạng thái Windows
    constexpr int status_top = 238;
    AddGroupBox(state, page, L"Trạng thái Windows", status_top, 112);
    AddLabel(state, page, L"", inset, status_top + 26, ContentWidth - 2 * inset, 36,
             InputMethodStatus);
    constexpr int open_width = 160;
    constexpr int enable_width = 190;
    AddButton(state, page, L"Bật ViGo trong Windows", BS_PUSHBUTTON,
              ContentWidth - inset - open_width - 8 - enable_width, status_top + 70, enable_width,
              button_height, EnableInputMethod);
    AddButton(state, page, L"Mở cài đặt ngôn ngữ", BS_PUSHBUTTON, ContentWidth - inset - open_width,
              status_top + 70, open_width, button_height, OpenLanguageSettings);
}

void CreateExpansionPage(AppState &state)
{
    constexpr Page page = Page::Expansions;
    constexpr int button_width = 84;
    constexpr int field_width = ContentWidth - button_width - 8;

    AddLabel(state, page, L"Phím tắt", 0, 0, 160, 18);
    AddLabel(state, page, L"Từ đầy đủ", 172, 0, 280, 18);
    AddEdit(state, page, 0, 20, 160, 24, ExpansionShortcut);
    AddEdit(state, page, 172, 20, field_width - 172, 24, ExpansionReplacement);
    AddButton(state, page, L"Thêm", BS_PUSHBUTTON, field_width + 8, 19, button_width, 26,
              ExpansionAdd);
    HWND list = AddControl(
        state, page, WC_LISTVIEWW, L"",
        LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_TABSTOP,
        WS_EX_CLIENTEDGE, {.x = 0, .y = 56, .width = field_width, .height = 282}, ExpansionList);
    if (list != nullptr)
    {
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;
        column.cx = ExpansionTriggerColumnWidth;
        column.pszText = const_cast<wchar_t *>(L"Phím tắt");
        ListView_InsertColumn(list, 0, &column);
        column.cx = field_width - ExpansionTriggerColumnWidth;
        column.pszText = const_cast<wchar_t *>(L"Từ đầy đủ");
        ListView_InsertColumn(list, 1, &column);
    }
    AddButton(state, page, L"Xóa", BS_PUSHBUTTON, field_width + 8, 56, button_width, 26,
              ExpansionRemove);
    AddLabel(state, page, L"Gõ tắt được thay thế khi nhấn dấu cách và chỉ lưu cục bộ trên máy.", 0,
             348, ContentWidth, 18);
}

void CreateSystemPage(AppState &state)
{
    constexpr Page page = Page::System;
    constexpr int inset = 12;
    constexpr int button_width = 84;
    constexpr int field_width = ContentWidth - 2 * inset - button_width - 8;

    AddGroupBox(state, page, L"Khởi động", 0, 60);
    AddButton(state, page, L"Tự động chạy khi đăng nhập", BS_AUTOCHECKBOX, inset, 26, 400, 20,
              LaunchAtLogin);

    constexpr int excluded_top = 72;
    AddGroupBox(state, page, L"Ứng dụng loại trừ", excluded_top, 284);
    AddLabel(state, page, L"ViGo tạm ngưng trong các ứng dụng này (tên file .exe, ví dụ game.exe).",
             inset, excluded_top + 26, ContentWidth - 2 * inset, 18);
    AddEdit(state, page, inset, excluded_top + 50, field_width, 24, ExcludedEdit);
    AddButton(state, page, L"Thêm", BS_PUSHBUTTON, inset + field_width + 8, excluded_top + 49,
              button_width, 26, ExcludedAdd);
    AddControl(state, page, WC_LISTBOXW, L"",
               WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE,
               {.x = inset, .y = excluded_top + 86, .width = field_width, .height = 182},
               ExcludedList);
    AddButton(state, page, L"Xóa", BS_PUSHBUTTON, inset + field_width + 8, excluded_top + 86,
              button_width, 26, ExcludedRemove);
}

void CreateAboutPage(AppState &state)
{
    constexpr Page page = Page::About;
    AddControl(state, page, WC_STATICW, L"ViGo", SS_CENTER, 0,
               {.x = 0, .y = 96, .width = ContentWidth, .height = 44, .font = FontRole::Title}, -1);
    AddLabel(state, page, L"Phiên bản 2.0 · Windows TSF", 0, 148, ContentWidth, 20, -1, SS_CENTER);
    AddLabel(state, page,
             L"Bộ gõ tiếng Việt riêng tư cho macOS, Windows và Linux.\r\n"
             L"GPL-3.0-or-later · Xử lý phím hoàn toàn trên thiết bị",
             0, 184, ContentWidth, 44, -1, SS_CENTER);
}

bool CreateControls(AppState &state)
{
    state.tab = CreateWindowExW(0, WC_TABCONTROLW, L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS, 0, 0, 0, 0,
                                state.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(Tab)),
                                GetModuleHandleW(nullptr), nullptr);
    if (state.tab == nullptr)
    {
        return false;
    }
    AddTabItem(state.tab, 0, L"Cơ bản");
    AddTabItem(state.tab, 1, L"Gõ tắt");
    AddTabItem(state.tab, 2, L"Hệ thống");
    AddTabItem(state.tab, 3, L"Giới thiệu");

    CreateGeneralPage(state);
    CreateExpansionPage(state);
    CreateSystemPage(state);
    CreateAboutPage(state);

    // Group boxes sit under their controls and the tab pane sits under everything, so the
    // WS_CLIPSIBLINGS painting never covers a control regardless of creation order.
    for (const ControlLayout &layout : state.layouts)
    {
        std::array<wchar_t, 16> class_name{};
        GetClassNameW(layout.handle, class_name.data(), static_cast<int>(class_name.size()));
        if (_wcsicmp(class_name.data(), WC_BUTTONW) == 0 &&
            (GetWindowLongPtrW(layout.handle, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX)
        {
            SetWindowPos(layout.handle, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
    SetWindowPos(state.tab, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    RecreateFonts(state);
    ApplyLayout(state, nullptr);
    PopulateHotKeyCombos(state);
    RefreshGeneralControls(state);
    RefreshExpansionList(state);
    RegisterConfiguredHotKeys(state);
    RefreshInputMethodStatus(state);
    ShowPage(state, 0);
    return true;
}

LRESULT HandleCommand(AppState &state, int identifier)
{
    switch (identifier)
    {
    case Enabled:
        state.settings.enabled = Button_GetCheck(GetDlgItem(state.window, Enabled)) == BST_CHECKED;
        Persist(state);
        break;
    case Telex:
    case MenuTelex:
        state.settings.enabled = true;
        state.settings.method = TypingMethod::Telex;
        Persist(state);
        RefreshGeneralControls(state);
        break;
    case Vni:
    case MenuVni:
        state.settings.enabled = true;
        state.settings.method = TypingMethod::Vni;
        Persist(state);
        RefreshGeneralControls(state);
        break;
    case MenuDisabled:
        state.settings.enabled = false;
        Persist(state);
        RefreshGeneralControls(state);
        break;
    case LaunchAtLogin: {
        std::array<wchar_t, 32768> executable{};
        const DWORD length =
            GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        const bool enabled =
            Button_GetCheck(GetDlgItem(state.window, LaunchAtLogin)) == BST_CHECKED;
        if (length == 0 || length >= executable.size() ||
            !SetLaunchAtLogin(enabled, std::wstring_view(executable.data(), length)))
        {
            MessageBoxW(state.window, L"Không thể thay đổi mục khởi động.", L"ViGo",
                        MB_OK | MB_ICONERROR);
            RefreshGeneralControls(state);
        }
        break;
    }
    case ExcludedAdd:
        AddExcludedExecutable(state);
        break;
    case ExcludedRemove:
        RemoveSelected(state, ExcludedList);
        break;
    case ExpansionAdd:
        AddExpansion(state);
        break;
    case ExpansionRemove:
        RemoveSelected(state, ExpansionList);
        break;
    case ApplyHotKeys:
        ApplyHotKeySettings(state);
        break;
    case EnableInputMethod:
        EnableInputMethodInWindows(state);
        break;
    case OpenLanguageSettings:
        ShellExecuteW(state.window, L"open", L"ms-settings:regionlanguage", nullptr, nullptr,
                      SW_SHOWNORMAL);
        break;
    case MenuSettings:
        ShowSettings(state);
        break;
    case IDCANCEL:
        ShowWindow(state.window, SW_HIDE);
        break;
    case MenuExit:
        DestroyWindow(state.window);
        break;
    default:
        break;
    }
    return 0;
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    auto *state = reinterpret_cast<AppState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(l_param);
        state = static_cast<AppState *>(create->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (state == nullptr)
    {
        return DefWindowProcW(window, message, w_param, l_param);
    }

    switch (message)
    {
    case WM_CREATE:
        if (const UINT dpi = GetDpiForWindow(window); dpi != 0)
        {
            state->dpi = dpi;
        }
        return CreateControls(*state) ? 0 : -1;
    case WM_COMMAND:
        return HandleCommand(*state, LOWORD(w_param));
    case WM_NOTIFY:
        if (reinterpret_cast<const NMHDR *>(l_param)->idFrom == Tab &&
            reinterpret_cast<const NMHDR *>(l_param)->code == TCN_SELCHANGE)
        {
            ShowPage(*state, TabCtrl_GetCurSel(state->tab));
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        // Labels, check boxes and group boxes sit on the white tab pane.
        HDC device = reinterpret_cast<HDC>(w_param);
        const bool status_label =
            reinterpret_cast<HWND>(l_param) == GetDlgItem(window, InputMethodStatus);
        SetBkColor(device, GetSysColor(COLOR_WINDOW));
        SetTextColor(device, status_label ? state->input_method_status_color
                                          : GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_DPICHANGED: {
        state->dpi = HIWORD(w_param);
        RecreateFonts(*state);
        const auto *suggested = reinterpret_cast<const RECT *>(l_param);
        const POINT origin{suggested->left, suggested->top};
        ApplyLayout(*state, &origin);
        return 0;
    }
    case WM_ACTIVATE:
        // The user may have just changed the keyboard list in Windows Settings.
        if (LOWORD(w_param) != WA_INACTIVE && !state->enabling_input_method)
        {
            RefreshInputMethodStatus(*state);
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;
    case TrayMessage:
        switch (LOWORD(l_param))
        {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu(*state);
            break;
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            ShowSettings(*state);
            break;
        default:
            break;
        }
        return 0;
    case ShowSettingsMessage:
        ShowSettings(*state);
        return 0;
    case WM_HOTKEY:
        state->settings = LoadSettings();
        if (w_param == ToggleHotKeyId)
        {
            state->settings.enabled = !state->settings.enabled;
        }
        else if (w_param == SwitchHotKeyId)
        {
            state->settings.enabled = true;
            state->settings.method = state->settings.method == TypingMethod::Telex
                                         ? TypingMethod::Vni
                                         : TypingMethod::Telex;
        }
        Persist(*state);
        RefreshGeneralControls(*state);
        return 0;
    case WM_DESTROY:
        UnregisterHotKey(window, ToggleHotKeyId);
        UnregisterHotKey(window, SwitchHotKeyId);
        Shell_NotifyIconW(NIM_DELETE, &state->tray);
        state->tray_added = false;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, w_param, l_param);
    }
}

} // namespace
} // namespace vigo::windows

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int)
{
    using namespace vigo::windows;

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\ViGoSettingsInstance");
    if (mutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (HWND existing = FindWindowW(WindowClassName, nullptr); existing != nullptr)
        {
            PostMessageW(existing, ShowSettingsMessage, 0, 0);
        }
        if (mutex != nullptr)
        {
            CloseHandle(mutex);
        }
        return 0;
    }

    INITCOMMONCONTROLSEX controls{.dwSize = sizeof(controls),
                                  .dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES |
                                           ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW window_class{.cbSize = sizeof(window_class)};
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(AppIconResourceId));
    if (window_class.hIcon == nullptr)
    {
        window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    window_class.lpszClassName = WindowClassName;
    window_class.hIconSm = static_cast<HICON>(
        LoadImageW(instance, MAKEINTRESOURCEW(AppIconResourceId), IMAGE_ICON,
                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (window_class.hIconSm == nullptr)
    {
        window_class.hIconSm = window_class.hIcon;
    }
    if (RegisterClassExW(&window_class) == 0)
    {
        CloseHandle(mutex);
        return 1;
    }

    AppState state;
    state.settings = LoadSettings();
    // The final size comes from the DPI-scaled layout in WM_CREATE.
    HWND window = CreateWindowExW(
        0, WindowClassName, L"ViGo", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 480, nullptr, nullptr, instance, &state);
    if (window == nullptr)
    {
        CloseHandle(mutex);
        return 1;
    }

    state.tray.cbSize = sizeof(state.tray);
    state.tray.hWnd = window;
    state.tray.uID = TrayId;
    state.tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    state.tray.uCallbackMessage = TrayMessage;
    state.tray.hIcon = window_class.hIconSm;
    UpdateTrayState(state);
    state.tray_added = Shell_NotifyIconW(NIM_ADD, &state.tray) != FALSE;
    if (!state.tray_added)
    {
        ShowSettings(state);
    }

    if (command_line != nullptr && wcsstr(command_line, L"--settings") != nullptr)
    {
        ShowSettings(state);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (IsWindowVisible(window) && IsDialogMessageW(window, &message))
        {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (state.message_font != nullptr)
    {
        DeleteObject(state.message_font);
    }
    if (state.title_font != nullptr)
    {
        DeleteObject(state.title_font);
    }
    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
