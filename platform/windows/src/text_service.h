#pragma once

#include "composition_session.h"

#include <msctf.h>

#include <atomic>
#include <memory>
#include <optional>

namespace nakl::windows
{

class TextService final : public ITfTextInputProcessorEx, public ITfKeyEventSink
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

  private:
    TextService() noexcept;
    ~TextService();

    [[nodiscard]] bool IsSecureMode() const;
    [[nodiscard]] HRESULT EnsureContext(ITfContext *context);
    [[nodiscard]] bool ResetContext() noexcept;
    void ClearContext() noexcept;

    std::atomic<ULONG> reference_count_{1};
    ITfThreadMgr *thread_manager_ = nullptr;
    IUnknown *context_identity_ = nullptr;
    TfClientId client_id_ = TF_CLIENTID_NULL;
    std::unique_ptr<CompositionSession> session_;
};

} // namespace nakl::windows
