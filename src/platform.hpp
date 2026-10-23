#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <atomic>
#include "core.hpp"

namespace mini {
template<class T> struct Com {
    T* p=nullptr;
    Com()=default; Com(const Com&)=delete; Com& operator=(const Com&)=delete;
    Com(Com&& o)noexcept:p(o.p){o.p=nullptr;}
    Com& operator=(Com&& o)noexcept{if(this!=&o){reset();p=o.p;o.p=nullptr;}return *this;}
    ~Com(){if(p)p->Release();}
    T* operator->()const{return p;} explicit operator bool()const{return p!=nullptr;}
    T** put(){if(p){p->Release();p=nullptr;}return &p;}
    void reset(){if(p){p->Release();p=nullptr;}}
};
std::wstring widen(const std::string& s);
std::string narrow(const std::wstring& s);
std::filesystem::path data_dir();
std::wstring error_text(HRESULT code);
bool write_atomic(const std::filesystem::path& path,const std::vector<unsigned char>& data);
std::vector<unsigned char> read_bytes(const std::filesystem::path& path,size_t limit);
struct AppEntry { std::wstring name,target,search; bool packaged=false; int uses=0; };
std::vector<AppEntry> discover_apps(const std::atomic<bool>& exiting);
bool launch_app(HWND hwnd,const AppEntry& app);
bool copy_text(HWND hwnd,const std::wstring& text);
bool paste_to(HWND target);
struct HistoryItem { std::wstring text; uint64_t timestamp=0; };
std::vector<HistoryItem> load_history(const std::filesystem::path& path,int retentionDays=7,bool* pruned=nullptr);
bool save_history(const std::filesystem::path& path,const std::vector<HistoryItem>& items);
struct Rate { std::wstring from,to,value,date,error; };
Rate fetch_rate(const std::wstring& from,const std::wstring& to);
struct RateSnapshot {std::vector<Rate> rates;std::wstring error;};
RateSnapshot fetch_daily_rates();
std::optional<Conversion> convert_time(const std::wstring& query);
struct Binding { std::wstring id; UINT modifiers=0,key=0; };
std::wstring hotkey_name(const Binding& b);
}
