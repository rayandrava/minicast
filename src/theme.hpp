#pragma once
#include <windows.h>

namespace mini {
// Read only when showing the palette or receiving Windows' theme notification.
// There is no polling thread, timer, or theme-specific resource cache.
bool system_uses_light_theme();
// One neutral surface color for the composited palette and native fallback.
// The brighter charcoal and near-white tint keep busy desktops subdued.
inline COLORREF palette_background(bool light) {
    return light?RGB(250,250,252):RGB(46,47,50);
}
void apply_window_theme(HWND hwnd, bool light);
// Preserve the native shadow and rounded silhouette while omitting its outline.
// Keep this separate from theming: regular Settings/dialog windows keep a frame.
void apply_palette_frame(HWND hwnd);
}
