#pragma once
#include "platform.hpp"
#include <d2d1.h>

namespace mini {
// System RichEdit text services keep editing, selection, IME and accessibility
// native while painting into the launcher's existing transparent D2D surface.
// A native EDIT is returned if Windows cannot provide the D2D text engine.
HWND create_search_input(HWND parent, int id, HINSTANCE instance);
bool search_input_is_composited(HWND hwnd);
void set_search_input_theme(HWND hwnd, bool light);
// Call between BeginDraw/EndDraw. Bounds are read from the child HWND; the
// caller's DPI, transform and text antialiasing mode are restored on return.
void paint_search_input(HWND hwnd, ID2D1RenderTarget* target, const wchar_t* placeholder);
}
