#include "app_icons.hpp"
#include "app_shell.hpp"
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wincodec.h>
#include <algorithm>

namespace mini {
namespace {
constexpr UINT ICON_SIZE=64;
constexpr size_t CACHE_LIMIT=64,QUEUE_LIMIT=14;

struct BitmapHandle {
    HBITMAP value=nullptr;
    ~BitmapHandle(){if(value)DeleteObject(value);}
};
struct IconHandle {
    HICON value=nullptr;
    ~IconHandle(){if(value)DestroyIcon(value);}
};
struct ShellId {
    PIDLIST_ABSOLUTE value=nullptr;
    ~ShellId(){if(value)CoTaskMemFree(value);}
};

bool visible_pixels(const AppIcon& image){
    for(size_t i=3;i<image.pixels.size();i+=4)if(image.pixels[i])return true;
    return false;
}
bool colored_pixels(const AppIcon& image){
    for(size_t i=0;i+3<image.pixels.size();i+=4)
        if(image.pixels[i]||image.pixels[i+1]||image.pixels[i+2])return true;
    return false;
}

std::shared_ptr<const AppIcon> read_pixels(IWICImagingFactory* wic,IWICBitmapSource* source){
    UINT width=0,height=0;
    if(FAILED(source->GetSize(&width,&height))||!width||!height||width>4096||height>4096)return {};
    Com<IWICBitmapScaler> scaler;
    // Shell providers may return their next larger native size. Keep the cache
    // bounded, but retain that icon instead of treating it as a failed lookup.
    if(width>ICON_SIZE||height>ICON_SIZE){
        const UINT longest=std::max(width,height);
        width=std::max(1u,static_cast<UINT>((uint64_t(width)*ICON_SIZE+longest/2)/longest));
        height=std::max(1u,static_cast<UINT>((uint64_t(height)*ICON_SIZE+longest/2)/longest));
        if(FAILED(wic->CreateBitmapScaler(scaler.put()))||
            FAILED(scaler->Initialize(source,width,height,WICBitmapInterpolationModeFant)))return {};
        source=scaler.p;
    }
    Com<IWICFormatConverter> converter;
    if(FAILED(wic->CreateFormatConverter(converter.put()))||
        FAILED(converter->Initialize(source,GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
    auto image=std::make_shared<AppIcon>();
    image->width=width;image->height=height;
    image->pixels.resize(size_t(width)*height*4);
    if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(image->pixels.size()),image->pixels.data())))return {};
    return image;
}

std::shared_ptr<const AppIcon> shell_icon(IWICImagingFactory* wic,PCIDLIST_ABSOLUTE id){
    SHFILEINFOW info{};
    const auto found=SHGetFileInfoW(reinterpret_cast<LPCWSTR>(id),0,&info,sizeof(info),
        SHGFI_PIDL|SHGFI_ICON|SHGFI_LARGEICON);
    IconHandle icon;icon.value=info.hIcon;
    if(!found||!icon.value)return {};
    // HICON retains the legacy transparency mask that an HBITMAP can lose.
    Com<IWICBitmap> source;
    if(FAILED(wic->CreateBitmapFromHICON(icon.value,source.put())))return {};
    auto image=read_pixels(wic,source.p);
    return image&&visible_pixels(*image)?image:nullptr;
}

std::shared_ptr<const AppIcon> extract_icon(IWICImagingFactory* wic,const std::wstring& target){
    ShellId id;
    const auto parsingName=app_shell_parsing_name(target);
    if(FAILED(SHParseDisplayName(parsingName.c_str(),nullptr,&id.value,0,nullptr))||!id.value)return {};
    Com<IShellItemImageFactory> imageFactory;
    BitmapHandle bitmap;
    std::shared_ptr<const AppIcon> opaqueFallback;
    if(SUCCEEDED(SHCreateItemFromIDList(id.value,IID_IShellItemImageFactory,
        reinterpret_cast<void**>(imageFactory.put())))&&
        SUCCEEDED(imageFactory->GetImage({ICON_SIZE,ICON_SIZE},
            static_cast<SIIGBF>(SIIGBF_ICONONLY|SIIGBF_BIGGERSIZEOK),&bitmap.value))&&bitmap.value){
        Com<IWICBitmap> source;
        if(SUCCEEDED(wic->CreateBitmapFromHBITMAP(bitmap.value,nullptr,WICBitmapUsePremultipliedAlpha,source.put()))){
            auto image=read_pixels(wic,source.p);
            if(image&&visible_pixels(*image))return image;
        }
        // Some legacy providers return RGB with every alpha byte zero. Prefer
        // the mask-aware HICON below; retain opaque RGB only as a last resort.
        source.reset();
        if(SUCCEEDED(wic->CreateBitmapFromHBITMAP(bitmap.value,nullptr,WICBitmapIgnoreAlpha,source.put()))){
            auto image=read_pixels(wic,source.p);
            if(image&&colored_pixels(*image))opaqueFallback=std::move(image);
        }
    }
    if(auto image=shell_icon(wic,id.value))return image;
    return opaqueFallback;
}
}

AppIcons::~AppIcons(){
    {std::lock_guard lock(mutex);stopping=true;pending.clear();}
    wake.notify_one();if(worker.joinable())worker.join();
}
void AppIcons::request(HWND receiver,UINT message,const std::vector<std::wstring>& visible){
    std::lock_guard lock(mutex);
    if(stopping)return;
    pending.clear();
    const auto now=std::chrono::steady_clock::now();
    for(const auto& target:visible){
        if(pending.size()>=QUEUE_LIMIT)break;
        if(target.empty()||(target==inFlight&&inFlightGeneration==generation))continue;
        auto found=cache.find(target);
        if(found!=cache.end()&&(found->second.image||now<found->second.retryAfter))continue;
        if(std::find(pending.begin(),pending.end(),target)==pending.end())pending.push_back(target);
    }
    if(!pending.empty()){
        if(!worker.joinable())worker=std::thread([this,receiver,message]{run(receiver,message);});
        wake.notify_one();
    }
}
std::shared_ptr<const AppIcon> AppIcons::get(const std::wstring& target){
    std::lock_guard lock(mutex);auto found=cache.find(target);if(found==cache.end())return {};
    found->second.used=++clock;return found->second.image;
}
void AppIcons::cancel_pending(){std::lock_guard lock(mutex);pending.clear();}
void AppIcons::invalidate(){std::lock_guard lock(mutex);++generation;cache.clear();pending.clear();}
void AppIcons::run(HWND receiver,UINT message){
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    {
        // Reuse WIC on its owning worker rather than activating it per row.
        Com<IWICImagingFactory> wic;
        for(;;){
            std::wstring target;uint64_t requestedGeneration=0;
            {std::unique_lock lock(mutex);wake.wait(lock,[&]{return stopping||!pending.empty();});if(stopping)break;
                target=std::move(pending.front());pending.pop_front();inFlight=target;
                requestedGeneration=inFlightGeneration=generation;
            }
            if(SUCCEEDED(initialized)&&!wic)
                CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,reinterpret_cast<void**>(wic.put()));
            auto image=wic?extract_icon(wic.p,target):nullptr;
            {std::lock_guard lock(mutex);inFlight.clear();if(stopping)break;
                if(requestedGeneration!=generation)continue;
                auto found=cache.find(target);
                const unsigned failures=image?0:std::min(5u,found==cache.end()?1u:found->second.failures+1);
                if(found==cache.end()&&cache.size()>=CACHE_LIMIT){
                    auto oldest=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});cache.erase(oldest);
                }
                Entry entry;entry.image=std::move(image);entry.used=++clock;entry.failures=failures;
                if(failures)entry.retryAfter=std::chrono::steady_clock::now()+std::chrono::seconds(std::min(60u,5u<<(failures-1)));
                cache[target]=std::move(entry);
            }
            PostMessageW(receiver,message,0,0);
        }
    }
    if(SUCCEEDED(initialized))CoUninitialize();
}
}
