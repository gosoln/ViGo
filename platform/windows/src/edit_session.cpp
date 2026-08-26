#include "edit_session.h"

#include "com_ptr.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace nakl::windows
{
namespace
{

[[nodiscard]] std::size_t Utf16ScalarCount(std::u16string_view text)
{
    std::size_t count = 0;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const char16_t unit = text[index];
        if (unit >= 0xD800 && unit <= 0xDBFF && index + 1 < text.size() &&
            text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF)
        {
            ++index;
        }
        ++count;
    }
    return count;
}

class ReplaceEditSession final : public ITfEditSession
{
  public:
    ReplaceEditSession(ITfContext *context, const EditAction &action)
        : context_(context), action_(action)
    {
        context_->AddRef();
    }

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

  private:
    [[nodiscard]] HRESULT Apply(TfEditCookie edit_cookie)
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
        BOOL empty = FALSE;
        result = selection_range->IsEmpty(edit_cookie, &empty);
        if (FAILED(result) || empty == FALSE)
        {
            return FAILED(result) ? result : S_FALSE;
        }
        if (action_.delete_scalars > static_cast<std::size_t>(std::numeric_limits<LONG>::max() / 2))
        {
            return E_INVALIDARG;
        }

        LONG delete_units = 0;
        if (action_.delete_scalars > 0)
        {
            ComPtr<ITfRange> probe;
            result = selection_range->Clone(probe.Put());
            if (FAILED(result))
            {
                return result;
            }
            result = probe->Collapse(edit_cookie, TF_ANCHOR_START);
            if (FAILED(result))
            {
                return result;
            }

            const LONG requested_window = static_cast<LONG>(action_.delete_scalars * 2);
            LONG shifted = 0;
            result = probe->ShiftStart(edit_cookie, -requested_window, &shifted, nullptr);
            if (FAILED(result))
            {
                return result;
            }
            if (shifted > 0)
            {
                return E_FAIL;
            }

            std::vector<wchar_t> preceding(static_cast<std::size_t>(-shifted));
            ULONG read = 0;
            if (!preceding.empty())
            {
                result = probe->GetText(edit_cookie, 0, preceding.data(),
                                        static_cast<ULONG>(preceding.size()), &read);
                if (FAILED(result))
                {
                    return result;
                }
            }
            const auto view =
                std::u16string_view(reinterpret_cast<const char16_t *>(preceding.data()), read);
            if (Utf16ScalarCount(view) < action_.delete_scalars)
            {
                return E_FAIL;
            }
            delete_units =
                static_cast<LONG>(Utf16UnitsForTrailingScalars(view, action_.delete_scalars));
            if (delete_units == 0)
            {
                return E_FAIL;
            }
        }

        ComPtr<ITfRange> replacement;
        result = selection_range->Clone(replacement.Put());
        if (FAILED(result))
        {
            return result;
        }
        result = replacement->Collapse(edit_cookie, TF_ANCHOR_START);
        if (FAILED(result))
        {
            return result;
        }
        if (delete_units > 0)
        {
            LONG shifted = 0;
            result = replacement->ShiftStart(edit_cookie, -delete_units, &shifted, nullptr);
            if (FAILED(result) || shifted != -delete_units)
            {
                return FAILED(result) ? result : E_FAIL;
            }
        }

        if (action_.insert.size() > static_cast<std::size_t>(std::numeric_limits<LONG>::max()))
        {
            return E_INVALIDARG;
        }
        result = replacement->SetText(edit_cookie, 0,
                                      reinterpret_cast<const wchar_t *>(action_.insert.data()),
                                      static_cast<LONG>(action_.insert.size()));
        if (FAILED(result))
        {
            return result;
        }
        result = replacement->Collapse(edit_cookie, TF_ANCHOR_END);
        if (FAILED(result))
        {
            return result;
        }

        TF_SELECTION updated_selection{};
        updated_selection.range = replacement.Get();
        updated_selection.style.ase = TF_AE_NONE;
        updated_selection.style.fInterimChar = FALSE;
        return context_->SetSelection(edit_cookie, 1, &updated_selection);
    }

    ~ReplaceEditSession()
    {
        context_->Release();
    }

    std::atomic<ULONG> reference_count_{1};
    ITfContext *context_;
    EditAction action_;
};

} // namespace

HRESULT ApplyEditAction(ITfContext *context, TfClientId client_id, const EditAction &action)
{
    if (context == nullptr || action.kind != EditAction::Kind::ReplaceBefore)
    {
        return E_INVALIDARG;
    }
    ReplaceEditSession *edit_session = nullptr;
    try
    {
        edit_session = new (std::nothrow) ReplaceEditSession(context, action);
    }
    catch (const std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
    if (edit_session == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    HRESULT session_result = E_FAIL;
    const HRESULT request_result = context->RequestEditSession(
        client_id, edit_session, TF_ES_SYNC | TF_ES_READWRITE, &session_result);
    edit_session->Release();
    return FAILED(request_result) ? request_result : session_result;
}

} // namespace nakl::windows
