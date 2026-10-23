#pragma once
#include "platform.hpp"
#include <condition_variable>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace mini {
struct AppIcon { UINT width=0,height=0;std::vector<unsigned char> pixels; };
// Extract only visible app icons, off the UI thread. Keep at most 64 images.
class AppIcons {
    struct Entry {
        std::shared_ptr<const AppIcon> image;
        uint64_t used=0;
        unsigned failures=0;
        std::chrono::steady_clock::time_point retryAfter{};
    };
    std::mutex mutex;
    std::condition_variable wake;
    std::map<std::wstring,Entry> cache;
    std::deque<std::wstring> pending;
    std::wstring inFlight;
    std::thread worker;
    bool stopping=false;
    uint64_t clock=0,generation=0,inFlightGeneration=0;
    void run(HWND receiver,UINT message);
public:
    ~AppIcons();
    void request(HWND receiver,UINT message,const std::vector<std::wstring>& visible);
    std::shared_ptr<const AppIcon> get(const std::wstring& target);
    void cancel_pending();
    // Discard cached and in-flight results when the app index is refreshed.
    void invalidate();
};
}
