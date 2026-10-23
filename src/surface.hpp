#pragma once
#include "platform.hpp"
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>

namespace mini {
// A transparent DirectComposition surface lets Windows' acrylic remain visible.
// The fallback uses an opaque HWND surface when composition is unavailable.
class NativeSurface {
    HWND window=nullptr;
    Com<ID3D11Device> gpu;
    Com<IDXGIDevice> dxgi;
    Com<ID2D1Device> device;
    Com<ID2D1DeviceContext> context;
    Com<IDXGISwapChain1> swap;
    Com<IDCompositionDevice> composition;
    Com<IDCompositionTarget> destination;
    Com<IDCompositionVisual> visual;
    Com<ID2D1Bitmap1> bitmap;
    Com<ID2D1HwndRenderTarget> fallback;
    bool bind();
    bool transparentInput=false;
    bool lightTheme=true;
    bool extend_frame();
    void use_opaque_backdrop();
public:
    // The OS accepted the Acrylic request, not proof that blur is currently
    // active: Windows may substitute a solid material for power/accessibility.
    bool acrylic=false;
    void set_transparent_input(bool enabled);
    bool create(HWND hwnd,ID2D1Factory* factory,Com<ID2D1RenderTarget>& out,bool light=true);
    void set_theme(HWND hwnd,bool light);
    void reset();
    void resize(UINT width,UINT height);
    HRESULT present();
};
}
