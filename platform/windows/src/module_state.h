#pragma once

#include <windows.h>

#include <string>

namespace vigo::windows
{

void SetModuleInstance(HINSTANCE instance) noexcept;
[[nodiscard]] HINSTANCE ModuleInstance() noexcept;
[[nodiscard]] std::wstring ModulePath();

void AddModuleObject() noexcept;
void RemoveModuleObject() noexcept;
void LockModule() noexcept;
void UnlockModule() noexcept;
[[nodiscard]] bool CanUnloadModule() noexcept;

} // namespace vigo::windows
