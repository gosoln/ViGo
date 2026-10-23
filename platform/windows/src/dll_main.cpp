#include "class_factory.h"
#include "module_state.h"
#include "registration.h"
#include "tsf_guids.h"

#include <windows.h>

#include <new>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    static_cast<void>(reserved);
    if (reason == DLL_PROCESS_ATTACH)
    {
        vigo::windows::SetModuleInstance(module);
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllCanUnloadNow()
{
    return vigo::windows::CanUnloadModule() ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID class_id, REFIID interface_id,
                                               void **object)
{
    if (class_id != vigo::windows::kTextServiceClsid)
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    if (object == nullptr)
    {
        return E_POINTER;
    }
    auto *factory = new (std::nothrow) vigo::windows::ClassFactory();
    if (factory == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    const HRESULT result = factory->QueryInterface(interface_id, object);
    factory->Release();
    return result;
}

extern "C" HRESULT __stdcall DllRegisterServer()
{
    const HRESULT initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialize_result) && initialize_result != RPC_E_CHANGED_MODE)
    {
        return initialize_result;
    }
    const HRESULT result = vigo::windows::RegisterTextService();
    if (initialize_result != RPC_E_CHANGED_MODE)
    {
        CoUninitialize();
    }
    return result;
}

extern "C" HRESULT __stdcall DllUnregisterServer()
{
    const HRESULT initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialize_result) && initialize_result != RPC_E_CHANGED_MODE)
    {
        return initialize_result;
    }
    const HRESULT result = vigo::windows::UnregisterTextService();
    if (initialize_result != RPC_E_CHANGED_MODE)
    {
        CoUninitialize();
    }
    return result;
}
