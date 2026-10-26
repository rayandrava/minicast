#pragma once
#include <windows.h>
#include <vector>
#include <cstring>
#include <wincodec.h>
#include "platform.hpp"
#include "history_retention.hpp"

namespace mini {
// Normalize bounded Windows DIBs to top-down, opaque BGRA for storage and drawing.
inline std::vector<unsigned char> normalize_clipboard_dib(const void* data, size_t size) {
    if (!data || size < sizeof(BITMAPINFOHEADER)) return {};
    BITMAPINFOHEADER h{}; memcpy(&h,data,sizeof(h));
    if (h.biSize < sizeof(h) || h.biSize > size || h.biWidth <= 0 || !h.biHeight ||
        h.biHeight == LONG_MIN || h.biPlanes != 1 || (h.biBitCount != 24 && h.biBitCount != 32) ||
        h.biClrUsed || (h.biCompression != BI_RGB && h.biCompression != BI_BITFIELDS)) return {};
    size_t offset=h.biSize;
    if(h.biCompression==BI_BITFIELDS){
        if(h.biBitCount!=32 || (h.biSize!=40 && h.biSize<52))return {};
        if(size<52)return {};
        DWORD masks[3];memcpy(masks,static_cast<const unsigned char*>(data)+40,12);
        if(masks[0]!=0xff0000 || masks[1]!=0xff00 || masks[2]!=0xff)return {};
        if(h.biSize==40)offset+=12;
    }
    const size_t width=h.biWidth,height=h.biHeight<0?-int64_t(h.biHeight):h.biHeight;
    if(width>(history_max_image_bytes-40)/4/height)return {};
    const size_t stride=((width*h.biBitCount+31)/32)*4;
    if(offset>size || height>(size-offset)/stride)return {};
    std::vector<unsigned char> result(40+width*height*4);
    BITMAPINFOHEADER out{};out.biSize=40;out.biWidth=LONG(width);out.biHeight=-LONG(height);
    out.biPlanes=1;out.biBitCount=32;out.biSizeImage=DWORD(width*height*4);
    memcpy(result.data(),&out,40);
    for(size_t y=0;y<height;++y){
        auto source=static_cast<const unsigned char*>(data)+offset+(h.biHeight<0?y:height-1-y)*stride;
        auto dest=result.data()+40+y*width*4;
        for(size_t x=0;x<width;++x){memcpy(dest+x*4,source+x*(h.biBitCount/8),3);dest[x*4+3]=255;}
    }
    return result;
}
inline std::vector<unsigned char> decode_clipboard_png(const void* data,size_t size){
    if(!data||!size||size>history_max_image_bytes)return {};
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Cleanup {HRESULT result;~Cleanup(){if(SUCCEEDED(result))CoUninitialize();}} cleanup{initialized};
    Com<IWICImagingFactory> factory;Com<IWICStream> stream;Com<IWICBitmapDecoder> decoder;
    Com<IWICBitmapFrameDecode> frame;Com<IWICFormatConverter> converter;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.put())))||
       FAILED(factory->CreateStream(stream.put()))||
       FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(const_cast<void*>(data)),DWORD(size)))||
       FAILED(factory->CreateDecoderFromStream(stream.p,nullptr,WICDecodeMetadataCacheOnDemand,decoder.put()))||
       FAILED(decoder->GetFrame(0,frame.put())))return {};
    UINT width=0,height=0;
    if(FAILED(frame->GetSize(&width,&height))||!width||!height||width>(history_max_image_bytes-40)/4/height)return {};
    std::vector<unsigned char> dib(40+size_t(width)*height*4);
    BITMAPINFOHEADER header{};header.biSize=40;header.biWidth=LONG(width);header.biHeight=-LONG(height);
    header.biPlanes=1;header.biBitCount=32;header.biSizeImage=DWORD(dib.size()-40);memcpy(dib.data(),&header,40);
    if(FAILED(factory->CreateFormatConverter(converter.put()))||
       FAILED(converter->Initialize(frame.p,GUID_WICPixelFormat32bppBGR,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))||
       FAILED(converter->CopyPixels(nullptr,width*4,header.biSizeImage,dib.data()+40)))return {};
    for(size_t i=43;i<dib.size();i+=4)dib[i]=255;
    return dib;
}
// Caller holds the clipboard open. Browser and design apps may expose PNG only.
inline std::vector<unsigned char> capture_clipboard_image(){
    const UINT png=RegisterClipboardFormatW(L"PNG");
    const UINT mimePng=RegisterClipboardFormatW(L"image/png");
    for(UINT format:{UINT(CF_DIB),UINT(CF_DIBV5),png,mimePng}){
        if(!IsClipboardFormatAvailable(format))continue;
        auto handle=GetClipboardData(format);if(!handle)continue;
        const auto size=GlobalSize(handle);if(!size||size>history_max_image_bytes+4096)continue;
        auto data=GlobalLock(handle);if(!data)continue;
        auto result=(format==png||format==mimePng)?decode_clipboard_png(data,size):normalize_clipboard_dib(data,size);
        GlobalUnlock(handle);if(!result.empty())return result;
    }
    return {};
}
inline bool copy_clipboard_image(HWND owner,const std::vector<unsigned char>& dib){
    if(dib.empty())return false;
    auto memory=GlobalAlloc(GMEM_MOVEABLE,dib.size());if(!memory)return false;
    auto data=GlobalLock(memory);if(!data){GlobalFree(memory);return false;}
    memcpy(data,dib.data(),dib.size());GlobalUnlock(memory);
    if(!OpenClipboard(owner)){GlobalFree(memory);return false;}
    bool ok=EmptyClipboard()&&SetClipboardData(CF_DIB,memory);CloseClipboard();
    if(!ok)GlobalFree(memory);return ok;
}
}
