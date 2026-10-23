// Optional read-only console diagnostic. Uses the production icon worker but
// never creates a window, launches an application or registers a hotkey.
// Arguments: an indexed app target, followed by an output PNG path.
#include "app_icons.hpp"
#include <wincodec.h>
#include <iostream>

using namespace mini;

HRESULT save_png(const AppIcon& image,const wchar_t* path){
    Com<IWICImagingFactory> factory;Com<IWICBitmap> bitmap;
    Com<IWICFormatConverter> source;Com<IWICStream> stream;
    Com<IWICBitmapEncoder> encoder;Com<IWICBitmapFrameEncode> frame;
    HRESULT result=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
        IID_IWICImagingFactory,reinterpret_cast<void**>(factory.put()));
    if(SUCCEEDED(result))result=factory->CreateBitmapFromMemory(image.width,image.height,
        GUID_WICPixelFormat32bppPBGRA,image.width*4,static_cast<UINT>(image.pixels.size()),
        const_cast<BYTE*>(image.pixels.data()),bitmap.put());
    if(SUCCEEDED(result))result=factory->CreateFormatConverter(source.put());
    if(SUCCEEDED(result))result=source->Initialize(bitmap.p,GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    if(SUCCEEDED(result))result=factory->CreateStream(stream.put());
    if(SUCCEEDED(result))result=stream->InitializeFromFilename(path,GENERIC_WRITE);
    if(SUCCEEDED(result))result=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put());
    if(SUCCEEDED(result))result=encoder->Initialize(stream.p,WICBitmapEncoderNoCache);
    if(SUCCEEDED(result))result=encoder->CreateNewFrame(frame.put(),nullptr);
    if(SUCCEEDED(result))result=frame->Initialize(nullptr);
    if(SUCCEEDED(result))result=frame->SetSize(image.width,image.height);
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(SUCCEEDED(result))result=frame->SetPixelFormat(&format);
    if(SUCCEEDED(result))result=frame->WriteSource(source.p,nullptr);
    if(SUCCEEDED(result))result=frame->Commit();
    if(SUCCEEDED(result))result=encoder->Commit();
    return result;
}

int probe(const wchar_t* target,const wchar_t* output){
    AppIcons icons;icons.request(nullptr,WM_APP,{target});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    std::shared_ptr<const AppIcon> image;
    while(!(image=icons.get(target))&&std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if(!image){std::cerr<<"No icon returned within 15 seconds\n";return 1;}
    const HRESULT result=save_png(*image,output);
    if(FAILED(result)){std::cerr<<"PNG write failed: 0x"<<std::hex<<result<<'\n';return 1;}
    std::cout<<"Production icon worker returned "<<image->width<<'x'<<image->height
        <<" pixels; PNG saved without opening a window.\n";
    return 0;
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3){std::cerr<<"Usage: probe-app-icon <app-target> <output.png>\n";return 2;}
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized))return 1;
    const int result=probe(argv[1],argv[2]);
    CoUninitialize();return result;
}
