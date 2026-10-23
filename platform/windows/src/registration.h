#pragma once

#include <windows.h>

namespace vigo::windows
{

[[nodiscard]] HRESULT RegisterTextService();
[[nodiscard]] HRESULT UnregisterTextService();

} // namespace vigo::windows
