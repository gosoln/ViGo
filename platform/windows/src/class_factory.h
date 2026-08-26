#pragma once

#include <unknwn.h>

#include <atomic>

namespace nakl::windows
{

class ClassFactory final : public IClassFactory
{
  public:
    ClassFactory();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id, void **object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer, REFIID interface_id,
                                             void **object) override;
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override;

  private:
    ~ClassFactory();

    std::atomic<ULONG> reference_count_{1};
};

} // namespace nakl::windows
