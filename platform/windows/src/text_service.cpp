#include "text_service.h"

#include "com_ptr.h"
#include "edit_session.h"
#include "module_state.h"
#include "settings.h"

#include <array>
#include <iterator>
#include <new>

namespace nakl::windows
{
namespace
{

enum class NormalizedKind
{
    Text,
    Backspace,
    Boundary,
};

struct NormalizedKey
{
    NormalizedKind kind = NormalizedKind::Boundary;
    char32_t scalar = U'\0';
};

[[nodiscard]] bool IsPressed(const std::array<BYTE, 256> &state, int key)
{
    return (state[static_cast<std::size_t>(key)] & 0x80) != 0;
}

[[nodiscard]] NormalizedKey NormalizeKey(WPARAM w_param, LPARAM l_param)
{
    if (w_param == VK_BACK)
    {
        return {.kind = NormalizedKind::Backspace};
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

[[nodiscard]] InputEvent CoreEvent(NormalizedKey key)
{
    switch (key.kind)
    {
    case NormalizedKind::Text:
        return {.kind = EventKind::Text, .scalar = key.scalar};
    case NormalizedKind::Backspace:
        return {.kind = EventKind::Backspace};
    case NormalizedKind::Boundary:
        return {.kind = EventKind::Boundary};
    }
    return {.kind = EventKind::Boundary};
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
    if (FAILED(edit_record->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, nullptr, 0,
                                                      ranges.Put())) ||
        ranges.Get() == nullptr)
    {
        return false;
    }
    ComPtr<ITfRange> range;
    ULONG fetched = 0;
    return ranges->Next(1, range.Put(), &fetched) == S_OK && fetched == 1;
}

} // namespace

TextService::TextService() noexcept
    : session_(new (std::nothrow) CompositionSession(TypingMethod::Telex))
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
        ClearContext();
        return S_OK;
    }
    HRESULT result = EnsureContext(context);
    if (FAILED(result))
    {
        return result;
    }
    if (!processing_enabled_)
    {
        return S_OK;
    }
    if (ignore_next_edit_)
    {
        // The client did not report a committed edit for the previous
        // pass-through key. Its text is therefore not safe to retain in the
        // composition history.
        ignore_next_edit_ = false;
        session_->Reset();
    }

    const NormalizedKey key = NormalizeKey(w_param, l_param);
    if (key.kind != NormalizedKind::Text)
    {
        // Request OnKeyDown even for pass-through boundaries so the live
        // Composition Session cannot leak across cursor movement or Backspace.
        *eaten = TRUE;
        return S_OK;
    }
    std::optional<EditAction> action;
    try
    {
        action = session_->Preview(CoreEvent(key));
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
    // TSF only delivers OnKeyDown after the test callback expresses interest.
    // Pass-through letters must therefore also request the callback: OnKeyDown
    // advances the live session and then returns FALSE so the client inserts
    // the original key normally.
    *eaten = TRUE;
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
    HRESULT result = EnsureContext(context);
    if (FAILED(result))
    {
        return result;
    }
    if (!processing_enabled_)
    {
        return S_OK;
    }

    const NormalizedKey key = NormalizeKey(w_param, l_param);
    if (key.kind == NormalizedKind::Boundary)
    {
        session_->Reset();
        return S_OK;
    }
    std::optional<EditAction> action;
    try
    {
        action = session_->Process(CoreEvent(key));
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
    if (action->kind == EditAction::Kind::PassThrough)
    {
        ignore_next_edit_ = true;
        return S_OK;
    }

    applying_edit_ = true;
    ignore_next_edit_ = true;
    result = ApplyEditAction(context, client_id_, *action);
    applying_edit_ = false;
    if (result == S_OK)
    {
        *eaten = TRUE;
    }
    else
    {
        ClearContext();
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
    static_cast<void>(context);
    static_cast<void>(edit_cookie);
    if (applying_edit_)
    {
        ignore_next_edit_ = false;
        return S_OK;
    }
    if (ignore_next_edit_)
    {
        ignore_next_edit_ = false;
        if (EditChangedText(edit_record))
        {
            return S_OK;
        }
    }
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
    ignore_next_edit_ = false;
    processing_enabled_ = false;
    session_.reset();
    if (context_identity_ == nullptr)
    {
        return;
    }
    context_identity_->Release();
    context_identity_ = nullptr;
}

} // namespace nakl::windows
