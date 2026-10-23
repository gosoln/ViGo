#include "module_state.h"

#include <atomic>
#include <vector>

namespace vigo::windows
{
namespace
{

HINSTANCE module_instance = nullptr;
std::atomic<long> object_count = 0;
std::atomic<long> lock_count = 0;

} // namespace

void SetModuleInstance(HINSTANCE instance) noexcept
{
    module_instance = instance;
}

HINSTANCE ModuleInstance() noexcept
{
    return module_instance;
}

std::wstring ModulePath()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true)
    {
        const DWORD length =
            GetModuleFileNameW(module_instance, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }
        if (length < buffer.size() - 1)
        {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

void AddModuleObject() noexcept
{
    object_count.fetch_add(1, std::memory_order_relaxed);
}

void RemoveModuleObject() noexcept
{
    object_count.fetch_sub(1, std::memory_order_relaxed);
}

void LockModule() noexcept
{
    lock_count.fetch_add(1, std::memory_order_relaxed);
}

void UnlockModule() noexcept
{
    lock_count.fetch_sub(1, std::memory_order_relaxed);
}

bool CanUnloadModule() noexcept
{
    return object_count.load(std::memory_order_relaxed) == 0 &&
           lock_count.load(std::memory_order_relaxed) == 0;
}

} // namespace vigo::windows
