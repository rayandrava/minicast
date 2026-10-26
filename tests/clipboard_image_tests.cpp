#include "platform.hpp"
#include "clipboard_image.hpp"
#include <iostream>
using namespace mini;

int main(){
    // A private window station has its own clipboard: never overwrite the user's.
    auto original=GetProcessWindowStation();auto originalDesktop=GetThreadDesktop(GetCurrentThreadId());
    auto station=CreateWindowStationW(nullptr,0,WINSTA_ALL_ACCESS,nullptr);
    if(!station||!SetProcessWindowStation(station))return 1;
    auto desktop=CreateDesktopW(L"ClipboardImageTests",nullptr,nullptr,0,GENERIC_ALL,nullptr);
    if(!desktop||!SetThreadDesktop(desktop))return 1;
    auto window=CreateWindowExW(0,L"STATIC",L"Clipboard test",0,0,0,1,1,nullptr,nullptr,nullptr,nullptr);
    int checks=0,failures=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<name<<'\n';}};
    auto png=read_bytes(std::filesystem::current_path()/L"assets"/L"brand"/L"double-crescent.png",history_max_image_bytes);
    auto expected=decode_clipboard_png(png.data(),png.size());
    check(window&&!expected.empty(),"isolated clipboard and PNG fixture ready");
    for(const auto* name:{L"PNG",L"image/png"}){
        auto memory=GlobalAlloc(GMEM_MOVEABLE,png.size());auto data=GlobalLock(memory);
        if(!data)return 1;memcpy(data,png.data(),png.size());GlobalUnlock(memory);
        if(!OpenClipboard(window))return 1;
        check(EmptyClipboard()&&SetClipboardData(RegisterClipboardFormatW(name),memory),"publish PNG-only clipboard");
        check(capture_clipboard_image()==expected,"capture PNG-only clipboard image");CloseClipboard();
    }
    check(copy_clipboard_image(window,expected),"copy stored image back to clipboard");
    if(!OpenClipboard(window))return 1;
    check(capture_clipboard_image()==expected,"recapture copied image without pixel changes");CloseClipboard();
    if(!expected.empty()){
        FILETIME now{};GetSystemTimeAsFileTime(&now);
        auto path=std::filesystem::current_path()/L"build"/L"clipboard-image-ui";
        std::filesystem::create_directories(path);
        check(save_history(path/L"clipboard.bin",{{L"Image",(uint64_t(now.dwHighDateTime)<<32)|now.dwLowDateTime,expected},{L"Clipboard text fixture",(uint64_t(now.dwHighDateTime)<<32)|now.dwLowDateTime-1}}),"save captured image and text for UI verification");
    }
    DestroyWindow(window);SetThreadDesktop(originalDesktop);CloseDesktop(desktop);
    SetProcessWindowStation(original);CloseWindowStation(station);
    std::cout<<checks<<" clipboard image checks, "<<failures<<" failures\n";return failures?1:0;
}
