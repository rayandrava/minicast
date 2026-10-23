#include "camera.hpp"
#include <wincodec.h>
#include <iostream>

using namespace mini;
int checks=0,failures=0;
void check(bool passed,const char* name){++checks;if(!passed){++failures;std::cerr<<"FAIL: "<<name<<'\n';}}

int main(){
    // Image encoding only: never instantiate Camera or access any device or
    // clipboard. RGB32 capture frames have an unused fourth byte, not alpha.
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)){std::cerr<<"Cannot initialize WIC test apartment\n";return 1;}
    const auto directory=std::filesystem::current_path()/L"build"/L"camera-image-tests"/
        (std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    std::error_code error;std::filesystem::create_directories(directory,error);
    if(error){CoUninitialize();std::cerr<<"Cannot create isolated image test directory\n";return 1;}
    const auto path=directory/L"snapshot.png";
    CameraFrame frame;frame.width=frame.height=2;
    frame.pixels={0,0,255,0, 0,255,0,77, 255,0,0,255, 255,255,255,19};
    const auto original=frame.pixels;
    {
        check(save_png(path,frame),"synthetic RGB32 snapshot saves as PNG");
        check(frame.pixels==original,"saving does not modify captured pixels");
        Com<IWICImagingFactory> factory;Com<IWICBitmapDecoder> decoder;
        Com<IWICBitmapFrameDecode> decoded;Com<IWICFormatConverter> converted;
        HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,reinterpret_cast<void**>(factory.put()));
        check(SUCCEEDED(hr),"WIC decoder factory available");
        if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,decoder.put());
        check(SUCCEEDED(hr),"saved snapshot is a readable encoded image");
        GUID container{};if(SUCCEEDED(hr))hr=decoder->GetContainerFormat(&container);
        check(SUCCEEDED(hr)&&container==GUID_ContainerFormatPng,"snapshot uses PNG container");
        if(SUCCEEDED(hr))hr=decoder->GetFrame(0,decoded.put());
        UINT width=0,height=0;if(SUCCEEDED(hr))hr=decoded->GetSize(&width,&height);
        check(SUCCEEDED(hr)&&width==2&&height==2,"snapshot dimensions round-trip");
        if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(converted.put());
        if(SUCCEEDED(hr))hr=converted->Initialize(decoded.p,GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        unsigned char pixels[16]{};
        if(SUCCEEDED(hr))hr=converted->CopyPixels(nullptr,8,sizeof(pixels),pixels);
        check(SUCCEEDED(hr),"snapshot pixels decode to BGRA");
        bool colors=SUCCEEDED(hr),opaque=SUCCEEDED(hr);
        for(size_t pixel=0;pixel<4;++pixel){
            for(size_t channel=0;channel<3;++channel)colors=colors&&pixels[pixel*4+channel]==original[pixel*4+channel];
            opaque=opaque&&pixels[pixel*4+3]==255;
        }
        check(colors,"red green blue white colors and row order round-trip");
        check(opaque,"unused RGB32 byte is not exported as transparency");
    }
    auto invalid=frame;invalid.pixels.pop_back();
    check(!save_png(path,invalid),"truncated frame rejected before encoder reads pixels");
    invalid=frame;invalid.width=0;
    check(!save_png(path,invalid),"zero-width frame rejected");
    std::filesystem::remove(path,error);std::filesystem::remove(directory,error);
    CoUninitialize();std::cout<<checks<<" camera image checks, "<<failures<<" failures\n";
    return failures?1:0;
}
