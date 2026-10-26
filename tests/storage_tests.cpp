#include "platform.hpp"
#include "clipboard_image.hpp"
#include "history_retention.hpp"
#include "pending_save.hpp"
#include <iostream>

using namespace mini;
int checks=0,failures=0;
void check(bool passed,const char* name){++checks;if(!passed){++failures;std::cerr<<"FAIL: "<<name<<'\n';}}

int main(){
    // Synthetic data only. Never use the user's profile or clipboard, and keep
    // every test artifact under this checkout's build directory.
    const auto directory=std::filesystem::current_path()/L"build"/L"storage-tests"/
        (std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    std::error_code error;std::filesystem::create_directories(directory,error);
    if(error){std::cerr<<"Cannot create isolated storage test directory\n";return 1;}
    const auto path=directory/L"history.bin";
    FILETIME clock{};GetSystemTimeAsFileTime(&clock);
    const uint64_t now=(uint64_t(clock.dwHighDateTime)<<32)|clock.dwLowDateTime;

    std::vector<HistoryItem> original{{L"Synthetic earlier entry",now-100},
        {std::wstring(30000,L'\u6F22'),now-200},
        {L"Emoji \U0001F680 \U0001F9EA and accents \u00E9\u00E8",now-300}};
    check(narrow(original[1].text).size()>65536,"fixture exceeds former UTF-8 length limit");
    check(save_history(path,original),"encrypt and save multilingual history");
    auto restored=load_history(path,7);
    check(restored.size()==original.size(),"large CJK entry does not discard entire saved history");
    bool equal=restored.size()==original.size();
    for(size_t i=0;equal&&i<restored.size();++i)
        equal=restored[i].text==original[i].text&&restored[i].timestamp==original[i].timestamp;
    check(equal,"Unicode text and timestamps round-trip through DPAPI");

    BITMAPINFOHEADER header{};header.biSize=40;header.biWidth=2;header.biHeight=2;header.biPlanes=1;header.biBitCount=24;
    std::vector<unsigned char> raw(56);memcpy(raw.data(),&header,40);raw[40]=11;raw[48]=22;
    auto png=read_bytes(std::filesystem::current_path()/L"assets"/L"brand"/L"double-crescent.png",history_max_image_bytes);
    auto pngDib=decode_clipboard_png(png.data(),png.size());
    check(!pngDib.empty()&&normalize_clipboard_dib(pngDib.data(),pngDib.size())==pngDib,"PNG clipboard bytes decode to valid stored bitmap");
    check(decode_clipboard_png(raw.data(),raw.size()).empty(),"invalid PNG clipboard data rejected");
    auto dib=normalize_clipboard_dib(raw.data(),raw.size());
    check(dib.size()==56&&dib[40]==22&&dib[48]==11&&dib[43]==255,"bottom-up padded 24-bit image converts to opaque top-down BGRA");
    check(normalize_clipboard_dib(raw.data(),55).empty(),"truncated image is rejected");
    header.biWidth=LONG_MAX;memcpy(raw.data(),&header,40);
    check(normalize_clipboard_dib(raw.data(),raw.size()).empty(),"oversized image dimensions are rejected before allocation");
    original.push_back({L"Image",now-400,dib});
    check(save_history(path,original),"save mixed text and image history encrypted");
    restored=load_history(path,7);
    check(restored.size()==original.size()&&restored.back().dib==dib,"image pixels round-trip through encrypted storage");
    auto expired=original;expired.back().timestamp=now-8*history_ticks_per_day;
    check(prune_history(expired,now,7)&&expired.size()==original.size()-1,"images follow retention policy");
    auto invalid=original;invalid.back().dib.pop_back();
    check(!save_history(path,invalid),"malformed image does not replace stored history");

    std::vector<HistoryItem> maximum{{std::wstring(history_max_item_characters,L'\u6F22'),now}};
    check(narrow(maximum[0].text).size()==history_max_item_utf8_bytes,"worst-case UTF-8 bound matches UTF-16 input bound");
    check(save_history(path,maximum),"maximum captured UTF-16 entry can be saved");
    restored=load_history(path,7);
    check(restored.size()==1&&restored[0].text==maximum[0].text,"maximum captured entry reloads intact");
    auto oversized=maximum;oversized[0].text.push_back(L'x');
    check(!save_history(path,oversized),"oversized individual entry rejected before replacing disk data");
    restored=load_history(path,7);
    check(restored.size()==1&&restored[0].text==maximum[0].text,"rejected oversized save preserves previous data");
    check(!save_history(path,std::vector<HistoryItem>(history_max_items+1,{L"x",now})),"oversized entry count rejected");
    check(!save_history(path,std::vector<HistoryItem>(history_max_bytes/(maximum[0].text.size()*sizeof(wchar_t))+1,maximum[0])),"oversized total history budget rejected");

    check(save_history(path,original),"prepare saved data for clear failure");
    PendingSave<std::vector<HistoryItem>> pending;
    check(!pending.take(),"empty save queue has no work");
    const auto clearRevision=pending.set({});
    auto clear=pending.take();
    check(clear&&clear->revision==clearRevision&&clear->value.empty(),"clear queues an empty revision");
    check(!pending.take(),"in-flight revision does not repeatedly dequeue itself");

    // Deny replacement of this synthetic file to exercise a real atomic-write
    // failure. The original encrypted history must remain readable until retry.
    HANDLE blocked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(blocked!=INVALID_HANDLE_VALUE,"hold synthetic file against replacement");
    if(blocked!=INVALID_HANDLE_VALUE&&clear){
        bool saved=save_history(path,clear->value);
        check(!saved,"clear fails when atomic replacement is blocked");
        if(!saved)pending.retry(*clear);
        restored=load_history(path,7);
        check(restored.size()==original.size(),"failed clear leaves previous disk data rather than pretending success");
        CloseHandle(blocked);
        auto retry=pending.take();
        check(retry&&retry->revision==clearRevision&&retry->value.empty(),"failed clear remains available for next event or shutdown");
        bool retried=retry&&save_history(path,retry->value);
        check(retried,"clear succeeds after transient replacement block ends");
        check(load_history(path,7).empty(),"acknowledged clear persists across reload");
        check(!pending.take(),"successful retry has no further work");
    }else if(blocked!=INVALID_HANDLE_VALUE)CloseHandle(blocked);

    const auto first=pending.set(original);auto inFlight=pending.take();
    std::vector<HistoryItem> newer{{L"Newest synthetic snapshot",now}};
    const auto latest=pending.set(newer);
    check(inFlight&&inFlight->revision==first&&latest>first,"new snapshot receives a later revision");
    if(inFlight)pending.retry(*inFlight);
    auto next=pending.take();
    check(next&&next->revision==latest&&next->value.size()==1&&next->value[0].text==newer[0].text,
        "failure of an older in-flight save cannot replace the newest snapshot");
    pending.set(original);inFlight=pending.take();pending.set(newer);
    next=pending.take();
    check(next&&next->value.size()==1&&next->value[0].text==newer[0].text,
        "success of an older in-flight save cannot consume a newer queued snapshot");

    // No recursive cleanup: only the two explicitly created synthetic files.
    std::filesystem::remove(path,error);auto temporary=path;temporary+=L".tmp";
    std::filesystem::remove(temporary,error);std::filesystem::remove(directory,error);
    std::cout<<checks<<" storage checks, "<<failures<<" failures\n";
    return failures?1:0;
}
