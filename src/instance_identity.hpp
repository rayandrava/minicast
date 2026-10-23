#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace mini {
// Input is an absolute, normalized, case-folded profile path. Keep the default
// class stable so newer builds still find the user's existing normal instance.
inline std::wstring instance_class(std::wstring_view profile) {
    if(profile.empty())return L"MinicastPalette";
    std::uint64_t hash=14695981039346656037ULL;
    for(wchar_t c:profile){hash^=static_cast<std::uint16_t>(c);hash*=1099511628211ULL;}
    constexpr wchar_t hex[]=L"0123456789abcdef";
    std::wstring result=L"MinicastPalette.Profile.";
    for(int shift=60;shift>=0;shift-=4)result+=hex[(hash>>shift)&15];
    return result;
}
}
