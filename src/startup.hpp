#pragma once
#include <string>

namespace mini {
// Whether this copy of Minicast is registered to open quietly at sign-in.
// Windows may independently disable registered apps in its Startup Apps settings.
bool startup_enabled();
// Whether Minicast's Run value registers any copy, including an older location.
// A missing value stays disabled; this does not inspect Startup Apps overrides.
bool startup_registered();
// Changes only Minicast's per-user Run value.
bool set_startup_enabled(bool enabled, std::wstring& error);
}
