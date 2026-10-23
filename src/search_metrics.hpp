#pragma once
#include <windows.h>

namespace mini {
// Native edit messages and caret callbacks use physical pixels. Direct2D uses
// DIPs, and RichEdit character sizes use twips (1/1440 inch).
struct SearchInputScale {
    UINT dpi;
    explicit SearchInputScale(UINT value):dpi(value?value:96) {}
    float dips(float pixels) const {return pixels*96.f/dpi;}
    LONG font_twips(int physicalHeight) const {return MulDiv(physicalHeight,1440,dpi);}
};
}
