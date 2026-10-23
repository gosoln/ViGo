#include "class_factory.h"

#include "module_state.h"
#include "text_service.h"

#include <new>

namespace vigo::windows
{

ClassFactory::ClassFactory()
{
    AddModuleObject();
}

ClassFactory::~ClassFactory()
{
    RemoveModuleObject();
}

HRESULT ClassFactory::QueryInterface(REFIID interface_id, void **object)
{
    if (object == nullptr)
    {
        return E_POINTER;
    }
    *object = nullptr;
    if (interface_id == IID_IUnknown || interface_id == IID_IClassFactory)
    {
        *object = static_cast<IClassFactory *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG ClassFactory::AddRef()
{
    return reference_count_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG ClassFactory::Release()
{
    const ULONG remaining = reference_count_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (remaining == 0)
    {
        delete this;
    }
    return remaining;
}

HRESULT ClassFactory::CreateInstance(IUnknown *outer, REFIID interface_id, void **object)
{
    if (outer != nullptr)
    {
        return CLASS_E_NOAGGREGATION;
    }
    return TextService::CreateInstance(interface_id, object);
}

HRESULT ClassFactory::LockServer(BOOL lock)
{
    if (lock != FALSE)
    {
        LockModule();
    }
    else
    {
        UnlockModule();
    }
    return S_OK;
}

} // namespace vigo::windows
