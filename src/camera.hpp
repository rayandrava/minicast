#pragma once
#include "platform.hpp"
#include <mutex>
#include <thread>
#include <memory>
namespace mini {
struct CameraFrame { UINT width=0,height=0; std::vector<unsigned char> pixels; uint64_t sequence=0; };
class Camera {
    struct State;
    std::shared_ptr<State> state;
    std::thread worker;
public:
    Camera(); ~Camera();
    bool start(HWND receiver,UINT notification,int device=0);
    void stop();
    bool running() const;
    std::shared_ptr<const CameraFrame> latest_frame() const;
    CameraFrame frame(bool mirror) const;
    std::wstring status() const;
    std::vector<std::wstring> devices() const;
};
bool save_png(const std::filesystem::path& path,const CameraFrame& frame);
bool copy_frame(HWND owner,const CameraFrame& frame);
}
