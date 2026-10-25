#pragma once

#include "com_ptr.h"
#include "composition_session.h"

#include <msctf.h>

#include <atomic>
#include <memory>
#include <optional>

namespace vigo::windows
{

// One TSF text input processor. The word being typed lives in a TSF
// composition whose text always mirrors CompositionSession::Text(); the
// composition ends on separators, boundary keys, client edits, and focus loss,
// which commits its text as ordinary client text.
class TextService final : public ITfTextInputProcessorEx,
                          public ITfKeyEventSink,
                          public ITfTextEditSink,
                          public ITfCompositionSink
{
  public:
    static HRESULT CreateInstance(REFIID interface_id, void **object);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id, void **object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr *thread_manager, TfClientId client_id) override;
    HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr *thread_manager, TfClientId client_id,
                                         DWORD flags) override;
    HRESULT STDMETHODCALLTYPE Deactivate() override;

    HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override;
    HRESULT STDMETHODCALLTYPE OnTestKeyDown(ITfContext *context, WPARAM w_param, LPARAM l_param,
                                            BOOL *eaten) override;
    HRESULT STDMETHODCALLTYPE OnTestKeyUp(ITfContext *context, WPARAM w_param, LPARAM l_param,
                                          BOOL *eaten) override;
    HRESULT STDMETHODCALLTYPE OnKeyDown(ITfContext *context, WPARAM w_param, LPARAM l_param,
                                        BOOL *eaten) override;
    HRESULT STDMETHODCALLTYPE OnKeyUp(ITfContext *context, WPARAM w_param, LPARAM l_param,
                                      BOOL *eaten) override;
    HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext *context, REFGUID guid,
                                             BOOL *eaten) override;
    HRESULT STDMETHODCALLTYPE OnEndEdit(ITfContext *context, TfEditCookie edit_cookie,
                                        ITfEditRecord *edit_record) override;
    HRESULT STDMETHODCALLTYPE OnCompositionTerminated(TfEditCookie edit_cookie,
                                                      ITfComposition *composition) override;

  private:
    TextService() noexcept;
    ~TextService();

    [[nodiscard]] bool IsSecureMode() const;
    [[nodiscard]] HRESULT EnsureContext(ITfContext *context);
    [[nodiscard]] HRESULT AdviseContext(ITfContext *context);
    [[nodiscard]] bool ResetContext() noexcept;
    void ClearContext() noexcept;

    // Mirrors the session text into the composition. Returns false when the
    // key could not be placed and the context was cleared.
    [[nodiscard]] bool ApplySessionText(ITfContext *context, std::u16string_view expected_text,
                                        bool end_after, std::optional<char32_t> retry_scalar);
    void FinishComposition(ITfContext *context) noexcept;

    std::atomic<ULONG> reference_count_{1};
    ITfThreadMgr *thread_manager_ = nullptr;
    IUnknown *context_identity_ = nullptr;
    ITfContext *advised_context_ = nullptr;
    DWORD edit_sink_cookie_ = TF_INVALID_COOKIE;
    TfClientId client_id_ = TF_CLIENTID_NULL;
    bool processing_enabled_ = true;
    bool applying_edit_ = false;
    ComPtr<ITfComposition> composition_;
    std::unique_ptr<CompositionSession> session_;
};

} // namespace vigo::windows
