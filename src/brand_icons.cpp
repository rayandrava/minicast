#include "brand_icons.hpp"
#include "resource.h"

namespace mini {
namespace {
HICON load_icon(HINSTANCE instance,int id,int width,int height) {
    // No LR_SHARED: it can return the first cached size for a resource name.
    return static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(id),IMAGE_ICON,width,height,LR_DEFAULTCOLOR));
}
bool taskbar_is_light() {
    // The Windows shell can be dark while application windows are light.
    DWORD value=0,bytes=sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&bytes);
    return value!=0;
}
}
BrandIcons::BrandIcons(HINSTANCE instance):instance_(instance) {
    large_=load_icon(instance_,IDI_MINICAST,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON));
    small_=load_icon(instance_,IDI_MINICAST,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON));
}
BrandIcons::~BrandIcons() {
    if(large_)DestroyIcon(large_);
    if(small_)DestroyIcon(small_);
    if(tray_)DestroyIcon(tray_);
}
bool BrandIcons::refresh_tray() {
    bool light=taskbar_is_light();
    HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);
    UINT dpi=taskbar?GetDpiForWindow(taskbar):GetDpiForSystem();
    int size=GetSystemMetricsForDpi(SM_CXSMICON,dpi?dpi:96);
    if(tray_&&trayLight_==light&&traySize_==size)return false;
    auto next=load_icon(instance_,light?IDI_MINICAST_TRAY_LIGHT:IDI_MINICAST_TRAY_DARK,size,size);
    if(!next)return false; // Retain a working icon if resource decoding fails.
    if(tray_)DestroyIcon(tray_);
    tray_=next;trayLight_=light;traySize_=size;return true;
}
}
