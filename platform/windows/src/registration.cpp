#include "registration.h"

#include "com_ptr.h"
#include "module_state.h"
#include "tsf_guids.h"

#include <msctf.h>

#include <array>
#include <iterator>
#include <string>

namespace nakl::windows
{
namespace
{

[[nodiscard]] HRESULT RegistryStatus(LSTATUS status)
{
    return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
}

[[nodiscard]] std::wstring GuidString(REFGUID guid)
{
    wchar_t value[40]{};
    const int length = StringFromGUID2(guid, value, static_cast<int>(std::size(value)));
    return length > 1 ? std::wstring(value, static_cast<std::size_t>(length - 1)) : std::wstring{};
}

[[nodiscard]] HRESULT SetRegistryString(HKEY key, const wchar_t *value_name,
                                        const std::wstring &value)
{
    return RegistryStatus(RegSetValueExW(key, value_name, 0, REG_SZ,
                                         reinterpret_cast<const BYTE *>(value.c_str()),
                                         static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))));
}

[[nodiscard]] HRESULT RegisterComServer()
{
    const std::wstring class_id = GuidString(kTextServiceClsid);
    const std::wstring module_path = ModulePath();
    if (class_id.empty() || module_path.empty())
    {
        return E_FAIL;
    }

    const std::wstring class_key_path = L"CLSID\\" + class_id;
    HKEY class_key = nullptr;
    LSTATUS status =
        RegCreateKeyExW(HKEY_CLASSES_ROOT, class_key_path.c_str(), 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &class_key, nullptr);
    if (status != ERROR_SUCCESS)
    {
        return RegistryStatus(status);
    }
    HRESULT result = SetRegistryString(class_key, nullptr, kTextServiceDescription);

    HKEY server_key = nullptr;
    if (SUCCEEDED(result))
    {
        status = RegCreateKeyExW(class_key, L"InProcServer32", 0, nullptr, REG_OPTION_NON_VOLATILE,
                                 KEY_WRITE, nullptr, &server_key, nullptr);
        result = RegistryStatus(status);
    }
    if (SUCCEEDED(result))
    {
        result = SetRegistryString(server_key, nullptr, module_path);
    }
    if (SUCCEEDED(result))
    {
        result = SetRegistryString(server_key, L"ThreadingModel", L"Apartment");
    }

    if (server_key != nullptr)
    {
        RegCloseKey(server_key);
    }
    RegCloseKey(class_key);
    return result;
}

void UnregisterComServer()
{
    const std::wstring class_id = GuidString(kTextServiceClsid);
    if (!class_id.empty())
    {
        const std::wstring path = L"CLSID\\" + class_id;
        RegDeleteTreeW(HKEY_CLASSES_ROOT, path.c_str());
    }
}

[[nodiscard]] HRESULT RegisterProfile()
{
    ComPtr<ITfInputProcessorProfileMgr> profile_manager;
    HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                      CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfileMgr,
                                      reinterpret_cast<void **>(profile_manager.Put()));
    if (FAILED(result))
    {
        return result;
    }

    const std::wstring module_path = ModulePath();
    return profile_manager->RegisterProfile(
        kTextServiceClsid, kVietnameseLanguageId, kLanguageProfileGuid, kTextServiceDescription,
        static_cast<ULONG>(std::size(kTextServiceDescription) - 1), module_path.c_str(),
        static_cast<ULONG>(module_path.size()), 0, nullptr, 0, TRUE, 0);
}

void UnregisterProfile()
{
    ComPtr<ITfInputProcessorProfileMgr> profile_manager;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfileMgr,
                                   reinterpret_cast<void **>(profile_manager.Put()))))
    {
        profile_manager->UnregisterProfile(kTextServiceClsid, kVietnameseLanguageId,
                                           kLanguageProfileGuid, 0);
    }
}

// Register only capabilities implemented by this slice. Input-mode and system
// tray categories are added with their corresponding TSF interfaces later.
const std::array<GUID, 4> kCategories{
    GUID_TFCAT_TIP_KEYBOARD,
    GUID_TFCAT_TIPCAP_SECUREMODE,
    GUID_TFCAT_TIPCAP_COMLESS,
    GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
};

[[nodiscard]] HRESULT RegisterCategories()
{
    ComPtr<ITfCategoryMgr> category_manager;
    HRESULT result =
        CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                         reinterpret_cast<void **>(category_manager.Put()));
    if (FAILED(result))
    {
        return result;
    }
    for (const GUID &category : kCategories)
    {
        result = category_manager->RegisterCategory(kTextServiceClsid, category, kTextServiceClsid);
        if (FAILED(result))
        {
            return result;
        }
    }
    return S_OK;
}

void UnregisterCategories()
{
    ComPtr<ITfCategoryMgr> category_manager;
    if (FAILED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ITfCategoryMgr,
                                reinterpret_cast<void **>(category_manager.Put()))))
    {
        return;
    }
    for (const GUID &category : kCategories)
    {
        category_manager->UnregisterCategory(kTextServiceClsid, category, kTextServiceClsid);
    }
}

} // namespace

HRESULT RegisterTextService()
{
    HRESULT result = RegisterComServer();
    if (FAILED(result))
    {
        return result;
    }
    result = RegisterProfile();
    if (FAILED(result))
    {
        UnregisterComServer();
        return result;
    }
    result = RegisterCategories();
    if (FAILED(result))
    {
        UnregisterCategories();
        UnregisterProfile();
        UnregisterComServer();
    }
    return result;
}

HRESULT UnregisterTextService()
{
    UnregisterCategories();
    UnregisterProfile();
    UnregisterComServer();
    return S_OK;
}

} // namespace nakl::windows
