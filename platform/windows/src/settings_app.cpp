#include "settings.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cwchar>
#include <string>
#include <string_view>
#include <vector>

namespace nakl::windows
{
namespace
{

constexpr wchar_t WindowClassName[] = L"NAKLSettingsWindow";
constexpr UINT TrayMessage = WM_APP + 1;
constexpr UINT ShowSettingsMessage = WM_APP + 2;
constexpr UINT TrayId = 1;

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
};

struct AppState
{
    HWND window = nullptr;
    HWND tab = nullptr;
    std::array<std::vector<HWND>, 3> pages;
    SettingsSnapshot settings;
    NOTIFYICONDATAW tray{};
};

constexpr int ToggleHotKeyId = 1;
constexpr int SwitchHotKeyId = 2;

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

void UseDefaultFont(HWND control)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
}

HWND AddControl(AppState &state, std::size_t page, const wchar_t *class_name, const wchar_t *text,
                DWORD style, int x, int y, int width, int height, int identifier)
{
    HWND control =
        CreateWindowExW(0, class_name, text, WS_CHILD | style, x, y, width, height, state.window,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
                        GetModuleHandleW(nullptr), nullptr);
    if (control != nullptr)
    {
        UseDefaultFont(control);
        state.pages[page].push_back(control);
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

void RegisterConfiguredHotKeys(AppState &state)
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
        return;
    }
    SetWindowTextW(GetDlgItem(state.window, HotKeyStatus), L"Phím nóng đang hoạt động.");
}

void RefreshExpansionList(AppState &state)
{
    HWND list = GetDlgItem(state.window, ExpansionList);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto &expansion : state.settings.expansions)
    {
        const auto trigger = Utf8ToWide(expansion.trigger);
        const auto replacement = Utf8ToWide(expansion.replacement);
        if (trigger.has_value() && replacement.has_value())
        {
            const std::wstring row = *trigger + L"  →  " + *replacement;
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
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

bool Persist(AppState &state)
{
    if (!SaveSettings(state.settings))
    {
        MessageBoxW(state.window, L"Không thể lưu cài đặt NAKL.", L"NAKL", MB_OK | MB_ICONERROR);
        return false;
    }
    DWORD_PTR ignored = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>(L"Software\\NAKL"), SMTO_ABORTIFHUNG, 100,
                        &ignored);
    RegisterConfiguredHotKeys(state);
    return true;
}

void ShowSettings(AppState &state)
{
    state.settings = LoadSettings();
    RefreshGeneralControls(state);
    RefreshExpansionList(state);
    RegisterConfiguredHotKeys(state);
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
    AppendMenuW(menu,
                MF_STRING | (state.settings.enabled && state.settings.method == TypingMethod::Telex
                                 ? MF_CHECKED
                                 : 0),
                MenuTelex, L"Telex");
    AppendMenuW(
        menu,
        MF_STRING |
            (state.settings.enabled && state.settings.method == TypingMethod::Vni ? MF_CHECKED : 0),
        MenuVni, L"VNI");
    AppendMenuW(menu, MF_STRING | (!state.settings.enabled ? MF_CHECKED : 0), MenuDisabled, L"Tắt");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuSettings, L"Tùy chọn…");
    AppendMenuW(menu, MF_STRING, MenuExit, L"Thoát");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(state.window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0,
                   state.window, nullptr);
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
        MessageBoxW(state.window, L"Phím tắt phải là một từ không chứa khoảng trắng.", L"NAKL",
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
        MessageBoxW(state.window, L"Nhập tên file thực thi, ví dụ game.exe.", L"NAKL",
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
        MessageBoxW(state.window, L"Hai phím nóng phải hợp lệ và khác nhau.", L"NAKL",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    state.settings.toggle_input_hot_key = toggle;
    state.settings.switch_method_hot_key = switch_method;
    Persist(state);
}

void RemoveSelected(AppState &state, int list_identifier)
{
    HWND list = GetDlgItem(state.window, list_identifier);
    const LRESULT selected = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (selected == LB_ERR)
    {
        return;
    }
    if (list_identifier == ExpansionList &&
        static_cast<std::size_t>(selected) < state.settings.expansions.size())
    {
        state.settings.expansions.erase(state.settings.expansions.begin() +
                                        static_cast<std::ptrdiff_t>(selected));
        if (Persist(state))
        {
            RefreshExpansionList(state);
        }
    }
    else if (list_identifier == ExcludedList &&
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

bool CreateControls(AppState &state)
{
    state.tab = CreateWindowExW(
        0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 16, 12, 600, 370, state.window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(Tab)), GetModuleHandleW(nullptr), nullptr);
    if (state.tab == nullptr)
    {
        return false;
    }
    UseDefaultFont(state.tab);
    TCITEMW item{.mask = TCIF_TEXT};
    item.pszText = const_cast<wchar_t *>(L"Tùy chọn chung");
    TabCtrl_InsertItem(state.tab, 0, &item);
    item.pszText = const_cast<wchar_t *>(L"Gõ tắt");
    TabCtrl_InsertItem(state.tab, 1, &item);
    item.pszText = const_cast<wchar_t *>(L"Giới thiệu");
    TabCtrl_InsertItem(state.tab, 2, &item);

    AddControl(state, 0, WC_BUTTONW, L"Bật bộ gõ tiếng Việt", BS_AUTOCHECKBOX | WS_TABSTOP, 42, 62,
               240, 24, Enabled);
    AddControl(state, 0, WC_STATICW, L"Kiểu gõ", SS_LEFT, 42, 102, 100, 22, -1);
    AddControl(state, 0, WC_BUTTONW, L"Telex", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 150, 100,
               90, 24, Telex);
    AddControl(state, 0, WC_BUTTONW, L"VNI", BS_AUTORADIOBUTTON | WS_TABSTOP, 245, 100, 90, 24,
               Vni);
    AddControl(state, 0, WC_BUTTONW, L"Tự động chạy khi đăng nhập", BS_AUTOCHECKBOX | WS_TABSTOP,
               42, 140, 280, 24, LaunchAtLogin);
    AddControl(state, 0, WC_STATICW, L"Phím nóng", SS_LEFT, 42, 174, 100, 22, -1);
    AddControl(state, 0, WC_STATICW, L"Bật/Tắt", SS_LEFT, 42, 200, 80, 22, -1);
    AddControl(state, 0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 122, 196, 145, 200,
               ToggleHotKeyModifiers);
    AddControl(state, 0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 275, 196, 75, 200,
               ToggleHotKeyVirtualKey);
    AddControl(state, 0, WC_STATICW, L"Đổi kiểu", SS_LEFT, 42, 229, 80, 22, -1);
    AddControl(state, 0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 122, 225, 145, 200,
               SwitchHotKeyModifiers);
    AddControl(state, 0, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, 275, 225, 75, 200,
               SwitchHotKeyVirtualKey);
    AddControl(state, 0, WC_BUTTONW, L"Áp dụng", BS_PUSHBUTTON | WS_TABSTOP, 362, 210, 85, 28,
               ApplyHotKeys);
    AddControl(state, 0, WC_STATICW, L"", SS_LEFT, 455, 205, 120, 42, HotKeyStatus);
    AddControl(state, 0, WC_STATICW, L"Ứng dụng loại trừ (tên .exe)", SS_LEFT, 42, 258, 250, 22,
               -1);
    AddControl(state, 0, WC_EDITW, L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 42, 280, 350, 25,
               ExcludedEdit);
    AddControl(state, 0, WC_BUTTONW, L"Thêm", BS_PUSHBUTTON | WS_TABSTOP, 400, 279, 80, 27,
               ExcludedAdd);
    AddControl(state, 0, WC_LISTBOXW, L"", WS_BORDER | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL, 42,
               313, 438, 42, ExcludedList);
    AddControl(state, 0, WC_BUTTONW, L"Xóa", BS_PUSHBUTTON | WS_TABSTOP, 490, 313, 80, 27,
               ExcludedRemove);

    AddControl(state, 1, WC_STATICW, L"Phím tắt", SS_LEFT, 42, 64, 150, 22, -1);
    AddControl(state, 1, WC_STATICW, L"Từ đầy đủ", SS_LEFT, 210, 64, 280, 22, -1);
    AddControl(state, 1, WC_EDITW, L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 42, 88, 150, 25,
               ExpansionShortcut);
    AddControl(state, 1, WC_EDITW, L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 210, 88, 280, 25,
               ExpansionReplacement);
    AddControl(state, 1, WC_BUTTONW, L"Thêm", BS_PUSHBUTTON | WS_TABSTOP, 500, 87, 70, 27,
               ExpansionAdd);
    AddControl(state, 1, WC_LISTBOXW, L"", WS_BORDER | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL, 42,
               128, 448, 196, ExpansionList);
    AddControl(state, 1, WC_BUTTONW, L"Xóa", BS_PUSHBUTTON | WS_TABSTOP, 500, 128, 70, 27,
               ExpansionRemove);
    AddControl(state, 1, WC_STATICW,
               L"Gõ tắt được thay thế khi nhấn dấu cách và chỉ lưu cục bộ trên máy.", SS_LEFT, 42,
               336, 520, 22, -1);

    AddControl(state, 2, WC_STATICW, L"NAKL", SS_CENTER, 170, 98, 300, 36, -1);
    AddControl(state, 2, WC_STATICW, L"Phiên bản 2.0 · Windows TSF", SS_CENTER, 120, 145, 400, 24,
               -1);
    AddControl(state, 2, WC_STATICW,
               L"Bộ gõ tiếng Việt riêng tư cho macOS, Windows và Linux.\r\n"
               L"GPL-3.0-or-later · Xử lý phím hoàn toàn trên thiết bị",
               SS_CENTER, 85, 188, 470, 62, -1);

    PopulateHotKeyCombos(state);
    RefreshGeneralControls(state);
    RefreshExpansionList(state);
    RegisterConfiguredHotKeys(state);
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
            MessageBoxW(state.window, L"Không thể thay đổi mục khởi động.", L"NAKL",
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
    case MenuSettings:
        ShowSettings(state);
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
    case WM_CLOSE:
        ShowWindow(window, SW_HIDE);
        return 0;
    case TrayMessage:
        if (LOWORD(l_param) == WM_RBUTTONUP)
        {
            ShowTrayMenu(*state);
        }
        else if (LOWORD(l_param) == WM_LBUTTONDBLCLK)
        {
            ShowSettings(*state);
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
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, w_param, l_param);
    }
}

} // namespace
} // namespace nakl::windows

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int)
{
    using namespace nakl::windows;

    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\NAKLSettingsInstance");
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

    INITCOMMONCONTROLSEX controls{.dwSize = sizeof(controls), .dwICC = ICC_TAB_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW window_class{.cbSize = sizeof(window_class)};
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    window_class.lpszClassName = WindowClassName;
    window_class.hIconSm = window_class.hIcon;
    if (RegisterClassExW(&window_class) == 0)
    {
        CloseHandle(mutex);
        return 1;
    }

    AppState state;
    state.settings = LoadSettings();
    HWND window = CreateWindowExW(
        0, WindowClassName, L"NAKL", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 440, nullptr, nullptr, instance, &state);
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
    state.tray.hIcon = window_class.hIcon;
    wcscpy_s(state.tray.szTip, L"NAKL");
    if (Shell_NotifyIconW(NIM_ADD, &state.tray) == FALSE)
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
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
