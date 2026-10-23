// Optional console diagnostic using the production palette. Its HWND stays
// hidden: no tray, hotkeys, startup changes, clipboard capture or camera access.
// Build with the normal app sources except main.cpp (included here), console
// subsystem, and the generated version/icon resources from build.ps1.
#include "../src/main.cpp"
#include <iostream>

int main(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 2;
    const HWND foreground=GetForegroundWindow();
    int failures=0,checks=0;
    auto check=[&](bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}};
    auto memory=[](const char* phase){PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters));std::cout<<phase<<"_private_bytes="<<counters.PrivateUsage<<'\n';};
    {
        App palette(GetModuleHandleW(nullptr),L"build/palette-render-profile");
        WNDCLASSW cls{};cls.hInstance=palette.instance;cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"MinicastHiddenRenderProbe";
        RegisterClassW(&cls);
        palette.hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"",WS_POPUP,0,0,750,475,nullptr,nullptr,palette.instance,nullptr);
        palette.edit=create_search_input(palette.hwnd,ID_QUERY,palette.instance);
        check(palette.hwnd&&palette.edit,"hidden window and search input");
        if(palette.hwnd&&palette.edit){
            palette.surface.set_transparent_input(search_input_is_composited(palette.edit));
            set_search_input_theme(palette.edit,palette.light);palette.layout();
            palette.set_mode(Mode::Home);
            auto start=std::chrono::steady_clock::now();
            check(palette.render_frame(),"first populated hidden frame");
            std::cout<<"cold_hidden_frame_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
            check(!palette.rows.empty(),"first frame has commands");
            check(palette.surface.acrylic,"composition/acrylic path active");
            memory("cold");
            auto* target=palette.target.p;
            double total=0,slowest=0;
            for(int i=0;i<20;++i){
                palette.set_mode(i%2?Mode::Home:Mode::Convert);
                start=std::chrono::steady_clock::now();
                check(palette.surface.resize(750,475),"unchanged size accepted");
                check(palette.render_frame(),"repeat hidden frame");
                const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();total+=elapsed;slowest=std::max(slowest,elapsed);
                check(palette.target.p==target,"repeat frame retains renderer");
            }
            std::cout<<"warm_hidden_frame_mean_ms="<<total/20<<" max_ms="<<slowest<<'\n';
            palette.surface.trim();
            memory("trimmed");
            check(palette.render_frame()&&palette.target.p==target,"idle trim retains usable renderer");
            check(palette.surface.resize(1125,713),"resize for display scale change");
            check(palette.render_frame(),"render after resize");
            check(palette.surface.resize(750,475),"restore size");
            palette.release_graphics();
            memory("released");
            check(palette.render_frame(),"recreate graphics after device reset");
            palette.light=!palette.light;palette.surface.set_theme(palette.hwnd,palette.light);set_search_input_theme(palette.edit,palette.light);
            check(palette.render_frame(),"render after theme change");
            check(!IsWindowVisible(palette.hwnd),"window stayed hidden");
            check(GetForegroundWindow()==foreground,"foreground window unchanged");
            palette.release_graphics();
        }
        if(palette.hwnd)DestroyWindow(palette.hwnd);
        palette.edit=palette.hwnd=nullptr;
        UnregisterClassW(cls.lpszClassName,palette.instance);
    }
    CoUninitialize();std::cout<<checks<<" hidden palette checks, "<<failures<<" failures\n";
    return failures?1:0;
}
