#include "edit_session.h"

#include <atomic>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace vigo::windows
{
namespace
{

class EditSessionBase : public ITfEditSession
{
  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id, void **object) override
    {
        if (object == nullptr)
        {
            return E_POINTER;
        }
        *object = nullptr;
        if (interface_id == IID_IUnknown || interface_id == IID_ITfEditSession)
        {
            *object = static_cast<ITfEditSession *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return reference_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = reference_count_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0)
        {
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie edit_cookie) override
    {
        try
        {
            return Apply(edit_cookie);
        }
        catch (const std::bad_alloc &)
        {
            return E_OUTOFMEMORY;
        }
        catch (...)
        {
            return E_UNEXPECTED;
        }
    }

  protected:
    EditSessionBase() = default;
    virtual ~EditSessionBase() = default;
    [[nodiscard]] virtual HRESULT Apply(TfEditCookie edit_cookie) = 0;

  private:
    std::atomic<ULONG> reference_count_{1};
};

[[nodiscard]] HRESULT PlaceCaretAfter(ITfContext *context, TfEditCookie edit_cookie,
                                      ITfRange *range)
{
    ComPtr<ITfRange> caret;
    HRESULT result = range->Clone(caret.Put());
    if (FAILED(result))
    {
        return result;
    }
    result = caret->Collapse(edit_cookie, TF_ANCHOR_END);
    if (FAILED(result))
    {
        return result;
    }
    TF_SELECTION selection{};
    selection.range = caret.Get();
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    return context->SetSelection(edit_cookie, 1, &selection);
}

[[nodiscard]] HRESULT ReplaceRangeText(TfEditCookie edit_cookie, ITfRange *range,
                                       std::u16string_view text)
{
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<LONG>::max()))
    {
        return E_INVALIDARG;
    }
    return range->SetText(edit_cookie, 0, reinterpret_cast<const wchar_t *>(text.data()),
                          static_cast<LONG>(text.size()));
}

class CompositionEditSession final : public EditSessionBase
{
  public:
    CompositionEditSession(ITfContext *context, ITfCompositionSink *sink,
                           ComPtr<ITfComposition> &composition, std::u16string_view expected_text,
                           std::u16string_view text, bool end_after, CompositionEditResult *outcome)
        : context_(context), sink_(sink), composition_(composition), expected_text_(expected_text),
          text_(text), end_after_(end_after), outcome_(outcome)
    {
        context_->AddRef();
    }

  private:
    [[nodiscard]] HRESULT Apply(TfEditCookie edit_cookie) override
    {
        *outcome_ = CompositionEditResult::Composing;
        if (composition_.Get() == nullptr)
        {
            return Start(edit_cookie);
        }

        ComPtr<ITfRange> range;
        const HRESULT result = composition_->GetRange(range.Put());
        if (FAILED(result) || range.Get() == nullptr)
        {
            return FAILED(result) ? result : E_FAIL;
        }
        if (!RangeHoldsExpectedText(edit_cookie, range.Get()))
        {
            return S_FALSE;
        }
        return Replace(edit_cookie, range.Get());
    }

    [[nodiscard]] HRESULT Start(TfEditCookie edit_cookie)
    {
        TF_SELECTION selection{};
        ULONG fetched = 0;
        HRESULT result =
            context_->GetSelection(edit_cookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (FAILED(result) || fetched != 1 || selection.range == nullptr)
        {
            return FAILED(result) ? result : E_FAIL;
        }
        ComPtr<ITfRange> selection_range;
        selection_range.Reset(selection.range);

        ComPtr<ITfContextComposition> context_composition;
        result = context_->QueryInterface(IID_ITfContextComposition,
                                          reinterpret_cast<void **>(context_composition.Put()));
        ITfComposition *started = nullptr;
        if (SUCCEEDED(result))
        {
            result = context_composition->StartComposition(edit_cookie, selection_range.Get(),
                                                           sink_, &started);
        }
        if (FAILED(result) || started == nullptr)
        {
            // The client does not support compositions here. Insert the text
            // as a plain committed edit so no keystroke is lost.
            result = ReplaceRangeText(edit_cookie, selection_range.Get(), text_);
            if (FAILED(result))
            {
                return result;
            }
            *outcome_ = CompositionEditResult::Committed;
            return PlaceCaretAfter(context_, edit_cookie, selection_range.Get());
        }
        composition_.Reset(started);

        ComPtr<ITfRange> range;
        result = composition_->GetRange(range.Put());
        if (FAILED(result) || range.Get() == nullptr)
        {
            return FAILED(result) ? result : E_FAIL;
        }
        return Replace(edit_cookie, range.Get());
    }

    [[nodiscard]] bool RangeHoldsExpectedText(TfEditCookie edit_cookie, ITfRange *range)
    {
        std::vector<wchar_t> buffer(expected_text_.size() + 1);
        ULONG read = 0;
        if (FAILED(range->GetText(edit_cookie, 0, buffer.data(), static_cast<ULONG>(buffer.size()),
                                  &read)))
        {
            return false;
        }
        return std::u16string_view(reinterpret_cast<const char16_t *>(buffer.data()), read) ==
               expected_text_;
    }

    [[nodiscard]] HRESULT Replace(TfEditCookie edit_cookie, ITfRange *range)
    {
        HRESULT result = ReplaceRangeText(edit_cookie, range, text_);
        if (FAILED(result))
        {
            return result;
        }
        result = PlaceCaretAfter(context_, edit_cookie, range);
        if (FAILED(result))
        {
            return result;
        }
        if (end_after_ || text_.empty())
        {
            result = composition_->EndComposition(edit_cookie);
            composition_.Reset();
            *outcome_ = CompositionEditResult::Ended;
        }
        return result;
    }

    ~CompositionEditSession() override
    {
        context_->Release();
    }

    ITfContext *context_;
    ITfCompositionSink *sink_;
    ComPtr<ITfComposition> &composition_;
    std::u16string expected_text_;
    std::u16string text_;
    bool end_after_;
    CompositionEditResult *outcome_;
};

// Owns its own composition reference so it can run after the caller forgot it.
class EndCompositionSession final : public EditSessionBase
{
  public:
    explicit EndCompositionSession(ITfComposition *composition) : composition_(composition)
    {
        composition_->AddRef();
    }

  private:
    [[nodiscard]] HRESULT Apply(TfEditCookie edit_cookie) override
    {
        // Finalize the text in place without touching the selection: the client
        // owns the caret once the composition ends.
        return composition_->EndComposition(edit_cookie);
    }

    ~EndCompositionSession() override
    {
        composition_->Release();
    }

    ITfComposition *composition_;
};

[[nodiscard]] HRESULT RunEditSession(ITfContext *context, TfClientId client_id,
                                     EditSessionBase *session, DWORD flags)
{
    HRESULT session_result = E_FAIL;
    const HRESULT request_result =
        context->RequestEditSession(client_id, session, flags, &session_result);
    if (FAILED(request_result))
    {
        return request_result;
    }
    return (flags & TF_ES_SYNC) != 0 ? session_result : request_result;
}

} // namespace

HRESULT SetCompositionText(ITfContext *context, TfClientId client_id, ITfCompositionSink *sink,
                           ComPtr<ITfComposition> &composition, std::u16string_view expected_text,
                           std::u16string_view text, bool end_after, CompositionEditResult *outcome)
{
    if (context == nullptr || sink == nullptr || outcome == nullptr)
    {
        return E_INVALIDARG;
    }
    *outcome = CompositionEditResult::Composing;
    if (composition.Get() == nullptr && text.empty())
    {
        *outcome = CompositionEditResult::Ended;
        return S_OK;
    }
    CompositionEditSession *session = nullptr;
    try
    {
        session = new (std::nothrow) CompositionEditSession(
            context, sink, composition, expected_text, text, end_after, outcome);
    }
    catch (const std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
    if (session == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    const HRESULT result =
        RunEditSession(context, client_id, session, TF_ES_SYNC | TF_ES_READWRITE);
    session->Release();
    return result;
}

HRESULT EndComposition(ITfContext *context, TfClientId client_id,
                       ComPtr<ITfComposition> &composition)
{
    if (composition.Get() == nullptr)
    {
        return S_OK;
    }
    if (context == nullptr)
    {
        composition.Reset();
        return E_INVALIDARG;
    }
    auto *session = new (std::nothrow) EndCompositionSession(composition.Get());
    composition.Reset();
    if (session == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    HRESULT result = RunEditSession(context, client_id, session, TF_ES_SYNC | TF_ES_READWRITE);
    if (FAILED(result))
    {
        const HRESULT async_result =
            RunEditSession(context, client_id, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE);
        // Report the synchronous failure so callers can tell the end is pending.
        result = FAILED(async_result) ? async_result : result;
    }
    session->Release();
    return result;
}

} // namespace vigo::windows
