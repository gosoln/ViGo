#pragma once

#include <windows.h>

namespace nakl::windows
{

// {027A6B19-4551-46F7-B8FD-FB09AE4135BA}
inline constexpr GUID kTextServiceClsid{
    0x027A6B19,
    0x4551,
    0x46F7,
    {0xB8, 0xFD, 0xFB, 0x09, 0xAE, 0x41, 0x35, 0xBA},
};

// {6DFD5E1A-DE07-49D4-B3AE-BD420B160656}
inline constexpr GUID kLanguageProfileGuid{
    0x6DFD5E1A,
    0xDE07,
    0x49D4,
    {0xB3, 0xAE, 0xBD, 0x42, 0x0B, 0x16, 0x06, 0x56},
};

inline constexpr LANGID kVietnameseLanguageId = 0x042A;
inline constexpr wchar_t kTextServiceDescription[] = L"NAKL Vietnamese";

} // namespace nakl::windows
