#include "text_service.h"

#include "com_ptr.h"
#include "edit_session.h"
#include "module_state.h"
#include "settings.h"

#include <array>
#include <iterator>
#include <new>
#include <string>

#ifdef VIGO_DIAGNOSTIC_TRACE
#include <cstdarg>
#include <cstdio>
#endif

namespace vigo::windows
{
namespace
{

#ifdef VIGO_DIAGNOSTIC_TRACE
// Opt-in build-time diagnostics for real-client certification. Records event
// kinds and HRESULTs only; never Keystroke Content.
void Trace(const char *format, ...)
{
    wchar_t directory[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, directory) == 0)
    {
        return;
    }
    const std::wstring path = std::wstring(directory) + L"vigo-tsf-trace.log";
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"a") != 0 || file == nullptr)
    {
        return;
    }
    std::fprintf(file, "[pid %lu] ", GetCurrentProcessId());
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(file, format, arguments);
    va_end(arguments);
    std::fputc('\n', file);
    std::fclose(file);
}
#define VIGO_TRACE(...) Trace(__VA_ARGS__)
#else
#define VIGO_TRACE(...) static_cast<void>(0)
#endif

enum class NormalizedKind
{
    Text,
    Backspace,
    Boundary,
    // Modifier and lock keys: neither text nor a reason to end the word.
    Ignored,
};

struct NormalizedKey
{
    NormalizedKind kind = NormalizedKind::Boundary;
    char32_t scalar = U'\0';
};

// Words longer than this are not Vietnamese syllables; committing them keeps
// the composition range and the session history from drifting apart once the
// history is trimmed.
constexpr std::size_t MaxCompositionUnits = 200;

[[nodiscard]] bool IsPressed(const std::array<BYTE, 256> &state, int key)
{
    return (state[static_cast<std::size_t>(key)] & 0x80) != 0;
}

[[nodiscard]] bool IsModifierKey(WPARAM w_param)
{
    switch (w_param)
    {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_CAPITAL:
    case VK_NUMLOCK:
    case VK_SCROLL:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] NormalizedKey NormalizeKey(WPARAM w_param, LPARAM l_param)
{
    if (w_param == VK_BACK)
    {
        return {.kind = NormalizedKind::Backspace};
    }
    if (IsModifierKey(w_param))
    {
        return {.kind = NormalizedKind::Ignored};
    }

    std::array<BYTE, 256> keyboard_state{};
    if (GetKeyboardState(keyboard_state.data()) == FALSE)
    {
        return {};
    }
    if (w_param < keyboard_state.size())
    {
        keyboard_state[static_cast<std::size_t>(w_param)] |= 0x80;
    }
    if (IsPressed(keyboard_state, VK_CONTROL) || IsPressed(keyboard_state, VK_MENU) ||
        IsPressed(keyboard_state, VK_LWIN) || IsPressed(keyboard_state, VK_RWIN))
    {
        return {};
    }

    const UINT scan_code = static_cast<UINT>((l_param >> 16) & 0xFF);
    wchar_t characters[4]{};
    // Bit 2 keeps ToUnicodeEx from mutating the kernel keyboard buffer. Test-key
    // and key-down both normalize the same event, so dead-key state must remain
    // untouched while TSF asks whether the service wants the real callback.
    constexpr UINT DoNotChangeKeyboardState = 0x4;
    const int count = ToUnicodeEx(static_cast<UINT>(w_param), scan_code, keyboard_state.data(),
                                  characters, static_cast<int>(std::size(characters)),
                                  DoNotChangeKeyboardState, GetKeyboardLayout(0));
    if (count == 1)
    {
        const char32_t scalar = static_cast<char32_t>(characters[0]);
        if (scalar >= U' ' && scalar != 0x7F)
        {
            return {.kind = NormalizedKind::Text, .scalar = scalar};
        }
        return {};
    }
    if (count == 2 && characters[0] >= 0xD800 && characters[0] <= 0xDBFF &&
        characters[1] >= 0xDC00 && characters[1] <= 0xDFFF)
    {
        const char32_t scalar = 0x10000 + ((static_cast<char32_t>(characters[0]) - 0xD800) << 10) +
                                (static_cast<char32_t>(characters[1]) - 0xDC00);
        return {.kind = NormalizedKind::Text, .scalar = scalar};
    }
    return {};
}

// Letters and digits continue the current word; anything else (space,
// punctuation, symbols) ends it, matching the core's word-clearing rule while
// keeping VNI digit modifiers inside the composition.
[[nodiscard]] bool ContinuesWord(char32_t scalar)
{
    if (scalar > 0xFFFF)
    {
        return true;
    }
    return IsCharAlphaNumericW(static_cast<wchar_t>(scalar)) != FALSE;
}

[[nodiscard]] bool IsCompartmentEnabled(ITfContext *context, REFGUID compartment_id)
{
    if (context == nullptr)
    {
        return true;
    }
    ComPtr<ITfCompartmentMgr> manager;
    if (FAILED(context->QueryInterface(IID_ITfCompartmentMgr,
                                       reinterpret_cast<void **>(manager.Put()))))
    {
        return true;
    }
    ComPtr<ITfCompartment> compartment;
    if (FAILED(manager->GetCompartment(compartment_id, compartment.Put())))
    {
        return true;
    }

    VARIANT value;
    VariantInit(&value);
    const HRESULT result = compartment->GetValue(&value);
    if (result == S_FALSE || value.vt == VT_EMPTY)
    {
        VariantClear(&value);
        return false;
    }
    if (FAILED(result))
    {
        VariantClear(&value);
        return true;
    }
    const bool enabled = (value.vt == VT_I4 && value.lVal != 0) ||
                         (value.vt == VT_UI4 && value.ulVal != 0) ||
                         (value.vt == VT_BOOL && value.boolVal != VARIANT_FALSE);
    VariantClear(&value);
    return enabled;
}

[[nodiscard]] bool IsContextDisabled(ITfContext *context)
{
    return IsCompartmentEnabled(context, GUID_COMPARTMENT_KEYBOARD_DISABLED) ||
           IsCompartmentEnabled(context, GUID_COMPARTMENT_EMPTYCONTEXT);
}

[[nodiscard]] bool EditChangedText(ITfEditRecord *edit_record)
{
    if (edit_record == nullptr)
    {
        return false;
    }
    ComPtr<IEnumTfRanges> ranges;
    if (FAILED(
            edit_record->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, nullptr, 0, ranges.Put())) ||
        ranges.Get() == nullptr)
    {
        return false;
    }
    ComPtr<ITfRange> range;
    ULONG fetched = 0;
    return ranges->Next(1, range.Put(), &fetched) == S_OK && fetched == 1;
}

[[nodiscard]] bool EditMovedSelection(ITfEditRecord *edit_record)
{
    if (edit_record == nullptr)
    {
        return false;
    }
    BOOL changed = FALSE;
    return SUCCEEDED(edit_record->GetSelectionStatus(&changed)) && changed != FALSE;
}

} // namespace

TextService::TextService() noexcept
    : session_(new(std::nothrow) CompositionSession(TypingMethod::Telex))
{
    AddModuleObject();
}

TextService::~TextService()
{
    Deactivate();
    RemoveModuleObject();
}

HRESULT TextService::CreateInstance(REFIID interface_id, void **object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    auto *service = new (std::nothrow) TextService();
    if (service == nullptr || service->session_ == nullptr || !service->session_->valid())
    {
        delete service;
        return E_OUTOFMEMORY;
    }
    const HRESULT result = service->QueryInterface(interface_id, object);
    service->Release();
    return result;
}

HRESULT TextService::QueryInterface(REFIID interface_id, void **object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    if (interface_id == IID_IUnknown || interface_id == IID_ITfTextInputProcessor ||
        interface_id == IID_ITfTextInputProcessorEx)
    {
        *object = static_cast<ITfTextInputProcessorEx *>(this);
    }
    else if (interface_id == IID_ITfKeyEventSink)
    {
        *object = static_cast<ITfKeyEventSink *>(this);
    }
    else if (interface_id == IID_ITfTextEditSink)
    {
        *object = static_cast<ITfTextEditSink *>(this);
    }
    else if (interface_id == IID_ITfCompositionSink)
    {
        *object = static_cast<ITfCompositionSink *>(this);
    }
    else
    {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG TextService::AddRef()
{
    return reference_count_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG TextService::Release()
{
    const ULONG remaining = reference_count_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (remaining == 0)
    {
        delete this;
    }
    return remaining;
}

HRESULT TextService::Activate(ITfThreadMgr *thread_manager, TfClientId client_id)
{
    return ActivateEx(thread_manager, client_id, 0);
}

HRESULT TextService::ActivateEx(ITfThreadMgr *thread_manager, TfClientId client_id, DWORD flags)
{
    static_cast<void>(flags);
    if (thread_manager == nullptr || thread_manager_ != nullptr)
    {
        return E_INVALIDARG;
    }

    ComPtr<ITfKeystrokeMgr> keystroke_manager;
    HRESULT result = thread_manager->QueryInterface(
        IID_ITfKeystrokeMgr, reinterpret_cast<void **>(keystroke_manager.Put()));
    if (FAILED(result))
    {
        return result;
    }
    result = keystroke_manager->AdviseKeyEventSink(client_id, this, TRUE);
    if (FAILED(result))
    {
        return result;
    }

    thread_manager_ = thread_manager;
    thread_manager_->AddRef();
    client_id_ = client_id;
    if (!ResetContext())
    {
        Deactivate();
        return E_OUTOFMEMORY;
    }
    VIGO_TRACE("ActivateEx ok client=%lu processing=%d", client_id, processing_enabled_ ? 1 : 0);
    return S_OK;
}

HRESULT TextService::Deactivate()
{
    if (thread_manager_ == nullptr)
    {
        return S_OK;
    }
    ComPtr<ITfKeystrokeMgr> keystroke_manager;
    if (SUCCEEDED(thread_manager_->QueryInterface(
            IID_ITfKeystrokeMgr, reinterpret_cast<void **>(keystroke_manager.Put()))))
    {
        keystroke_manager->UnadviseKeyEventSink(client_id_);
    }
    ClearContext();
    thread_manager_->Release();
    thread_manager_ = nullptr;
    client_id_ = TF_CLIENTID_NULL;
    return S_OK;
}

HRESULT TextService::OnSetFocus(BOOL foreground)
{
    if (foreground == FALSE)
    {
        ClearContext();
    }
    return S_OK;
}

HRESULT TextService::OnTestKeyDown(ITfContext *context, WPARAM w_param, LPARAM l_param, BOOL *eaten)
{
    if (eaten == nullptr)
    {
        return E_POINTER;
    }
    *eaten = FALSE;
    if (context == nullptr || IsSecureMode() || IsContextDisabled(context))
    {
        VIGO_TRACE("TestKeyDown skipped: ctx=%p", static_cast<void *>(context));
        ClearContext();
        return S_OK;
    }
    const HRESULT result = EnsureContext(context);
    if (FAILED(result))
    {
        VIGO_TRACE("TestKeyDown EnsureContext failed 0x%08lX", result);
        return result;
    }
    if (!processing_enabled_)
    {
        return S_OK;
    }

    const NormalizedKey key = NormalizeKey(w_param, l_param);
    VIGO_TRACE("TestKeyDown kind=%d", static_cast<int>(key.kind));
    switch (key.kind)
    {
    case NormalizedKind::Ignored:
        return S_OK;
    case NormalizedKind::Boundary:
        // Claim the key only while composing so TSF still calls OnKeyDown, where
        // a write lock is available. OnKeyDown finalizes the word synchronously
        // and then reports eaten=FALSE, so the client receives the real key
        // (Enter, arrow, Tab) against already-committed text and the caret does
        // not jump. Editing here, in the read-only test callback, would be
        // deferred and race the client.
        *eaten = composition_.Get() != nullptr ? TRUE : FALSE;
        return S_OK;
    case NormalizedKind::Backspace:
        // Only claim Backspace while a word is being composed; otherwise the
        // client handles selections and earlier text itself.
        *eaten = composition_.Get() != nullptr && !session_->Text().empty() ? TRUE : FALSE;
        return S_OK;
    case NormalizedKind::Text:
        // TSF never hands a key the test callback claimed back to the client,
        // so every text key is placed by OnKeyDown itself.
        *eaten = TRUE;
        return S_OK;
    }
    return S_OK;
}

HRESULT TextService::OnTestKeyUp(ITfContext *context, WPARAM w_param, LPARAM l_param, BOOL *eaten)
{
    static_cast<void>(context);
    static_cast<void>(w_param);
    static_cast<void>(l_param);
    if (eaten == nullptr)
    {
        return E_POINTER;
    }
    *eaten = FALSE;
    return S_OK;
}

HRESULT TextService::OnKeyDown(ITfContext *context, WPARAM w_param, LPARAM l_param, BOOL *eaten)
{
    if (eaten == nullptr)
    {
        return E_POINTER;
    }
    *eaten = FALSE;
    if (context == nullptr || IsSecureMode() || IsContextDisabled(context))
    {
        ClearContext();
        return S_OK;
    }
    const HRESULT result = EnsureContext(context);
    if (FAILED(result))
    {
        return result;
    }
    if (!processing_enabled_)
    {
        return S_OK;
    }

    const NormalizedKey key = NormalizeKey(w_param, l_param);
    VIGO_TRACE("KeyDown kind=%d", static_cast<int>(key.kind));
    if (key.kind == NormalizedKind::Ignored)
    {
        return S_OK;
    }
    if (key.kind == NormalizedKind::Boundary)
    {
        FinishComposition(context);
        return S_OK;
    }
    if (key.kind == NormalizedKind::Backspace &&
        (composition_.Get() == nullptr || session_->Text().empty()))
    {
        FinishComposition(context);
        return S_OK;
    }

    const std::u16string expected(session_->Text());
    const InputEvent event = key.kind == NormalizedKind::Backspace
                                 ? InputEvent{.kind = EventKind::Backspace}
                                 : InputEvent{.kind = EventKind::Text, .scalar = key.scalar};
    std::optional<EditAction> action;
    try
    {
        action = session_->Process(event);
    }
    catch (const std::bad_alloc &)
    {
        ClearContext();
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        ClearContext();
        return E_UNEXPECTED;
    }
    if (!action.has_value())
    {
        ClearContext();
        return S_OK;
    }

    const bool end_after = key.kind == NormalizedKind::Text && !ContinuesWord(key.scalar);
    const std::optional<char32_t> retry_scalar =
        key.kind == NormalizedKind::Text ? std::optional<char32_t>(key.scalar) : std::nullopt;
    if (ApplySessionText(context, expected, end_after, retry_scalar))
    {
        *eaten = TRUE;
    }
    return S_OK;
}

HRESULT TextService::OnKeyUp(ITfContext *context, WPARAM w_param, LPARAM l_param, BOOL *eaten)
{
    return OnTestKeyUp(context, w_param, l_param, eaten);
}

HRESULT TextService::OnPreservedKey(ITfContext *context, REFGUID guid, BOOL *eaten)
{
    static_cast<void>(context);
    static_cast<void>(guid);
    if (eaten == nullptr)
    {
        return E_POINTER;
    }
    *eaten = FALSE;
    return S_OK;
}

HRESULT TextService::OnEndEdit(ITfContext *context, TfEditCookie edit_cookie,
                               ITfEditRecord *edit_record)
{
    static_cast<void>(edit_cookie);
    if (applying_edit_)
    {
        return S_OK;
    }
    if (composition_.Get() == nullptr)
    {
        if (session_ != nullptr && !session_->Text().empty())
        {
            session_->Reset();
        }
        return S_OK;
    }
    // The client moved the caret or edited text behind the service's back:
    // commit the word and start over rather than guess what happened.
    if (EditMovedSelection(edit_record) || EditChangedText(edit_record))
    {
        VIGO_TRACE("OnEndEdit external change ends composition");
        FinishComposition(context);
    }
    return S_OK;
}

HRESULT TextService::OnCompositionTerminated(TfEditCookie edit_cookie, ITfComposition *composition)
{
    static_cast<void>(edit_cookie);
    static_cast<void>(composition);
    VIGO_TRACE("OnCompositionTerminated");
    composition_.Reset();
    if (session_ != nullptr)
    {
        session_->Reset();
    }
    return S_OK;
}

bool TextService::IsSecureMode() const
{
    if (thread_manager_ == nullptr)
    {
        return true;
    }
    ComPtr<ITfThreadMgrEx> extended_manager;
    if (FAILED(thread_manager_->QueryInterface(IID_ITfThreadMgrEx,
                                               reinterpret_cast<void **>(extended_manager.Put()))))
    {
        return true;
    }
    DWORD flags = 0;
    return FAILED(extended_manager->GetActiveFlags(&flags)) || (flags & TF_TMF_SECUREMODE) != 0;
}

HRESULT TextService::EnsureContext(ITfContext *context)
{
    ComPtr<IUnknown> identity;
    const HRESULT result =
        context->QueryInterface(IID_IUnknown, reinterpret_cast<void **>(identity.Put()));
    if (FAILED(result))
    {
        return result;
    }
    if (identity.Get() != context_identity_)
    {
        if (!ResetContext())
        {
            return E_OUTOFMEMORY;
        }
        context_identity_ = identity.Get();
        context_identity_->AddRef();
        const HRESULT advise_result = AdviseContext(context);
        if (FAILED(advise_result))
        {
            ClearContext();
            return advise_result;
        }
    }
    if (processing_enabled_ && (session_ == nullptr || !session_->valid()))
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

HRESULT TextService::AdviseContext(ITfContext *context)
{
    ComPtr<ITfSource> source;
    HRESULT result =
        context->QueryInterface(IID_ITfSource, reinterpret_cast<void **>(source.Put()));
    if (FAILED(result))
    {
        return result;
    }
    result = source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink *>(this),
                                &edit_sink_cookie_);
    if (FAILED(result))
    {
        edit_sink_cookie_ = TF_INVALID_COOKIE;
        return result;
    }
    advised_context_ = context;
    advised_context_->AddRef();
    return S_OK;
}

bool TextService::ResetContext() noexcept
{
    ClearContext();
    try
    {
        const SettingsSnapshot settings = LoadSettings();
        processing_enabled_ = settings.enabled && !IsCurrentProcessExcluded(settings);
        if (!processing_enabled_)
        {
            return true;
        }
        session_.reset(new (std::nothrow) CompositionSession(settings.method, settings.expansions));
        return session_ != nullptr && session_->valid();
    }
    catch (...)
    {
        processing_enabled_ = false;
        return false;
    }
}

void TextService::ClearContext() noexcept
{
    if (composition_.Get() != nullptr)
    {
        static_cast<void>(EndComposition(advised_context_, client_id_, composition_));
        composition_.Reset();
    }
    if (advised_context_ != nullptr)
    {
        ComPtr<ITfSource> source;
        if (edit_sink_cookie_ != TF_INVALID_COOKIE &&
            SUCCEEDED(advised_context_->QueryInterface(IID_ITfSource,
                                                       reinterpret_cast<void **>(source.Put()))))
        {
            source->UnadviseSink(edit_sink_cookie_);
        }
        advised_context_->Release();
        advised_context_ = nullptr;
        edit_sink_cookie_ = TF_INVALID_COOKIE;
    }
    applying_edit_ = false;
    processing_enabled_ = false;
    session_.reset();
    if (context_identity_ == nullptr)
    {
        return;
    }
    context_identity_->Release();
    context_identity_ = nullptr;
}

bool TextService::ApplySessionText(ITfContext *context, std::u16string_view expected_text,
                                   bool end_after, std::optional<char32_t> retry_scalar)
{
    CompositionEditResult outcome = CompositionEditResult::Composing;
    applying_edit_ = true;
    HRESULT result =
        SetCompositionText(context, client_id_, static_cast<ITfCompositionSink *>(this),
                           composition_, expected_text, session_->Text(), end_after, &outcome);
    applying_edit_ = false;
    VIGO_TRACE("ApplySessionText units=%zu end=%d hr=0x%08lX outcome=%d", session_->Text().size(),
               end_after ? 1 : 0, result, static_cast<int>(outcome));

    if (result == S_FALSE)
    {
        // The composition no longer holds the text the session placed. Drop
        // it and place only the key just typed as a fresh word.
        static_cast<void>(EndComposition(context, client_id_, composition_));
        session_->Reset();
        if (!retry_scalar.has_value())
        {
            return true;
        }
        static_cast<void>(
            session_->Process(InputEvent{.kind = EventKind::Text, .scalar = *retry_scalar}));
        applying_edit_ = true;
        result = SetCompositionText(context, client_id_, static_cast<ITfCompositionSink *>(this),
                                    composition_, {}, session_->Text(), end_after, &outcome);
        applying_edit_ = false;
    }
    if (FAILED(result))
    {
        ClearContext();
        return false;
    }
    if (outcome != CompositionEditResult::Composing || end_after ||
        session_->Text().size() > MaxCompositionUnits)
    {
        FinishComposition(context);
    }
    return true;
}

void TextService::FinishComposition(ITfContext *context) noexcept
{
    if (composition_.Get() != nullptr)
    {
        const HRESULT result = EndComposition(context, client_id_, composition_);
        VIGO_TRACE("FinishComposition hr=0x%08lX", result);
        static_cast<void>(result);
        composition_.Reset();
    }
    if (session_ != nullptr)
    {
        session_->Reset();
    }
}

} // namespace vigo::windows
