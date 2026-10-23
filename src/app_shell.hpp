#pragma once
#include <string>

namespace mini {
// AppsFolder can return a bare child AppUserModelID even when asked for a
// desktop-absolute parsing name. Resolve it within that namespace, without
// changing the saved target used by hotkeys, usage ranking and icon caches.
inline std::wstring app_shell_parsing_name(const std::wstring& target) {
    if(target.empty()||target.find_first_of(L"\\/:")!=target.npos)return target;
    return L"shell:AppsFolder\\"+target;
}
}
