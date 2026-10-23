#pragma once
#include <windows.h>

namespace mini {
// Owned icon handles loaded from the executable, never external files.
// Only the tray handle changes, on shell theme or DPI notifications.
class BrandIcons {
    HINSTANCE instance_;
    HICON large_=nullptr,small_=nullptr,tray_=nullptr;
    int traySize_=0;
    bool trayLight_=false;
public:
    explicit BrandIcons(HINSTANCE instance);
    ~BrandIcons();
    BrandIcons(const BrandIcons&)=delete;
    BrandIcons& operator=(const BrandIcons&)=delete;
    HICON large() const {return large_;}
    HICON small() const {return small_;}
    HICON tray() const {return tray_;}
    bool refresh_tray();
};
}
