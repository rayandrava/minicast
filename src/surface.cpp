#include "surface.hpp"
#include "theme.hpp"
#include <dwmapi.h>
#include <dxgi1_3.h>
#include <algorithm>
namespace mini {
bool NativeSurface::extend_frame(){
    if(!window)return false;
    // Search uses system RichEdit's alpha-correct D2D path. One sheet of the
    // same material can now cover header, content and footer without a GDI
    // edit punching transparent pixels into the search text.
    MARGINS margins{-1,-1,-1,-1};
    return SUCCEEDED(DwmExtendFrameIntoClientArea(window,&margins));
}
void NativeSurface::set_transparent_input(bool enabled){
    if(transparentInput==enabled)return;
    transparentInput=enabled;
    if(window&&context)set_theme(window,lightTheme);
}
void NativeSurface::use_opaque_backdrop(){
    acrylic=false;
    if(!window)return;
    const int material=1; // DWMSBT_NONE; do not leave a stale system material.
    DwmSetWindowAttribute(window,38,&material,sizeof(material));
    MARGINS opaque{};
    DwmExtendFrameIntoClientArea(window,&opaque);
}
bool NativeSurface::bind(){
    Com<IDXGISurface> surface;
    if(FAILED(swap->GetBuffer(0,__uuidof(IDXGISurface),reinterpret_cast<void**>(surface.put()))))return false;
    D2D1_BITMAP_PROPERTIES1 props{};props.pixelFormat={DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED};props.dpiX=props.dpiY=96;props.bitmapOptions=D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    if(FAILED(context->CreateBitmapFromDxgiSurface(surface.p,&props,bitmap.put())))return false;
    context->SetTarget(bitmap.p);context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);return true;
}
void NativeSurface::reset(){
    if(context)context->SetTarget(nullptr);bitmap.reset();if(visual)visual->SetContent(static_cast<IUnknown*>(nullptr));if(composition)composition->Commit();visual.reset();destination.reset();composition.reset();swap.reset();context.reset();device.reset();dxgi.reset();gpu.reset();fallback.reset();acrylic=false;commitPending=false;pixelWidth=pixelHeight=0;
}
void NativeSurface::set_theme(HWND hwnd,bool light){
    lightTheme=light;
    apply_window_theme(hwnd,light);
    apply_palette_frame(hwnd);
    if(!context)return;
    // If text services cannot initialize, preserve readable native input with
    // one uniform opaque fallback instead of restoring a split glass header.
    if(!transparentInput){use_opaque_backdrop();return;}
    BOOL composed=FALSE;
    // DWMSBT_TRANSIENTWINDOW is the documented Desktop Acrylic material for a
    // transient launcher. DWM owns tint, live blur, noise and solid fallback;
    // leave Transparency Effects, Energy Saver and accessibility policy alone.
    // DWMWA_SYSTEMBACKDROP_TYPE is available from Windows 11 build 22621.
    const int material=3;
    acrylic=SUCCEEDED(DwmIsCompositionEnabled(&composed))&&composed&&
        SUCCEEDED(DwmSetWindowAttribute(hwnd,38,&material,sizeof(material)))&&
        extend_frame();
    if(!acrylic)use_opaque_backdrop();
}
bool NativeSurface::create(HWND hwnd,ID2D1Factory* factory,Com<ID2D1RenderTarget>& out,bool light){
    reset();window=hwnd;RECT rect;GetClientRect(hwnd,&rect);UINT width=std::max<LONG>(1,rect.right),height=std::max<LONG>(1,rect.bottom);
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,gpu.put(),nullptr,nullptr);
    Com<ID2D1Factory1> upgraded;Com<IDXGIAdapter> adapter;Com<IDXGIFactory2> dxgiFactory;
    if(SUCCEEDED(hr))hr=gpu->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(dxgi.put()));
    if(SUCCEEDED(hr)){Com<IDXGIDevice1> latency;if(SUCCEEDED(dxgi->QueryInterface(__uuidof(IDXGIDevice1),reinterpret_cast<void**>(latency.put()))))latency->SetMaximumFrameLatency(1);}
    if(SUCCEEDED(hr))hr=factory->QueryInterface(__uuidof(ID2D1Factory1),reinterpret_cast<void**>(upgraded.put()));
    if(SUCCEEDED(hr))hr=upgraded->CreateDevice(dxgi.p,device.put());
    if(SUCCEEDED(hr))hr=device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,context.put());
    if(SUCCEEDED(hr))hr=dxgi->GetAdapter(adapter.put());
    if(SUCCEEDED(hr))hr=adapter->GetParent(__uuidof(IDXGIFactory2),reinterpret_cast<void**>(dxgiFactory.put()));
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=width;desc.Height=height;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    if(SUCCEEDED(hr))hr=dxgiFactory->CreateSwapChainForComposition(gpu.p,&desc,nullptr,swap.put());
    if(SUCCEEDED(hr))hr=DCompositionCreateDevice(dxgi.p,__uuidof(IDCompositionDevice),reinterpret_cast<void**>(composition.put()));
    // The transparent input is painted by this tree, so it must sit above
    // child HWNDs. A below-children tree is clipped by the input's rectangle.
    // Keep native EDIT fallback above its opaque composition surface instead.
    if(SUCCEEDED(hr))hr=composition->CreateTargetForHwnd(hwnd,transparentInput?TRUE:FALSE,destination.put());
    if(SUCCEEDED(hr))hr=composition->CreateVisual(visual.put());
    if(SUCCEEDED(hr))hr=visual->SetContent(swap.p);
    if(SUCCEEDED(hr))hr=destination->SetRoot(visual.p);
    if(SUCCEEDED(hr)&&bind()){
        set_theme(hwnd,light);
        // Attach only after the first populated buffer is presented.
        commitPending=true;pixelWidth=width;pixelHeight=height;
        context->AddRef();out.p=context.p;return true;
    }
    reset();use_opaque_backdrop();auto properties=D2D1::RenderTargetProperties();properties.dpiX=properties.dpiY=96;
    if(FAILED(factory->CreateHwndRenderTarget(properties,D2D1::HwndRenderTargetProperties(hwnd,D2D1::SizeU(width,height)),fallback.put())))return false;
    pixelWidth=width;pixelHeight=height;
    apply_window_theme(hwnd,light);apply_palette_frame(hwnd);fallback->AddRef();out.p=fallback.p;return true;
}
bool NativeSurface::resize(UINT width,UINT height){
    if(!width||!height||(width==pixelWidth&&height==pixelHeight))return true;
    if(fallback){if(FAILED(fallback->Resize(D2D1::SizeU(width,height))))return false;}
    else if(swap&&context){
        if(acrylic&&!extend_frame())use_opaque_backdrop();
        context->SetTarget(nullptr);bitmap.reset();
        if(FAILED(swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0))||!bind())return false;
    }else return false;
    pixelWidth=width;pixelHeight=height;return true;
}
void NativeSurface::trim(){
    // Release driver-internal caches while retaining the bounded palette and
    // icon surfaces. Recreating the entire device after one second hidden made
    // hotkey latency depend on how recently the launcher had been used.
    Com<IDXGIDevice3> trimDevice;
    if(dxgi&&SUCCEEDED(dxgi->QueryInterface(__uuidof(IDXGIDevice3),reinterpret_cast<void**>(trimDevice.put()))))trimDevice->Trim();
}
HRESULT NativeSurface::present(){
    if(!swap)return S_OK;
    HRESULT hr=swap->Present(1,0);
    if(SUCCEEDED(hr)&&commitPending){
        hr=composition->Commit();
        // Only on creation/recovery: ensure the populated tree is processed
        // before show() reveals it. Normal frames never wait on a DComp commit.
        if(SUCCEEDED(hr))hr=composition->WaitForCommitCompletion();
        if(SUCCEEDED(hr))commitPending=false;
    }
    return hr;
}
}
