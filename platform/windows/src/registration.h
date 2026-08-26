#pragma once

#include <windows.h>

namespace nakl::windows
{

[[nodiscard]] HRESULT RegisterTextService();
[[nodiscard]] HRESULT UnregisterTextService();

} // namespace nakl::windows
