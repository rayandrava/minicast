#include "theme.hpp"
#include <dwmapi.h>

namespace mini {
bool system_uses_light_theme() {
    DWORD value = 1;
    DWORD bytes = sizeof(value);
    const auto result = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &bytes);
    return result != ERROR_SUCCESS || value != 0;
}

void apply_window_theme(HWND hwnd, bool light) {
    if (!hwnd) return;
    const BOOL dark = light ? FALSE : TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE. A numeric identifier keeps compatibility
    // with older SDK headers; unsupported Windows versions ignore the request.
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

void apply_palette_frame(HWND hwnd) {
    if (!hwnd) return;
    // Documented Windows 11 attributes. Keeping the HWND's native shape lets
    // DWM provide a correctly rounded shadow; a region/layered fake shadow
    // would bypass the system's corner policy and require extra rendering.
    const int round = 2; // DWMWCP_ROUND (the larger of the native corner styles)
    const COLORREF noBorder = 0xFFFFFFFE; // DWMWA_COLOR_NONE
    DwmSetWindowAttribute(hwnd, 33, &round, sizeof(round));
    DwmSetWindowAttribute(hwnd, 34, &noBorder, sizeof(noBorder));
}
}
