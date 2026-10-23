#include "platform.hpp"
#include "camera.hpp"
#include "lucide.hpp"
#include "surface.hpp"
#include "app_icons.hpp"
#include "brand_icons.hpp"
#include "theme.hpp"
#include "app_ranking.hpp"
#include "local_currency.hpp"
#include "startup.hpp"
#include "search_input.hpp"
#include "shortcut_capture.hpp"
#include "history_retention.hpp"
#include "pending_save.hpp"
#include "instance_identity.hpp"
#include "version.h"
#include <uxtheme.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wtsapi32.h>
#include <psapi.h>
#include <windowsx.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <map>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <array>

using namespace mini;
namespace {
constexpr UINT MSG_TRAY=WM_APP+1,MSG_APPS=WM_APP+2,MSG_CLIP=WM_APP+3,MSG_RATE=WM_APP+4,MSG_CAMERA=WM_APP+5,MSG_SHOW=WM_APP+6,MSG_STATUS=WM_APP+7,MSG_QUERY=WM_APP+8,MSG_ICONS=WM_APP+9;
constexpr int ID_QUERY=100,TIMER_PASTE=2,TIMER_TOAST=3,TIMER_SMOKE=4,TIMER_TRIM_GRAPHICS=5,TIMER_HISTORY_EXPIRY=6;
constexpr int PALETTE_WIDTH=750,PALETTE_HEIGHT=475,ROW_TOP=92,ROW_HEIGHT=38,VISIBLE_ROWS=9,FOOTER_TOP=434;
constexpr int SETTINGS_COMMAND=201,SETTINGS_HOTKEY=202,SETTINGS_REMOVE=205,SETTINGS_HISTORY=206,SETTINGS_CLEAR=207,SETTINGS_REFRESH=208;
enum class Mode { Home,Apps,Convert,Emoji,Clipboard,Camera };
enum class Action { App,Copy,Mode,Settings,EnableHistory,ClearHistory,RefreshRates,RefreshApps,Query,Updates,About };
struct Row {std::wstring title,detail,glyph;Action action;int index=0;std::wstring value;int score=0;uint64_t identity=0;std::wstring group;};
struct GridTile {int row;D2D1_RECT_F bounds;};
struct GridSection {std::wstring name;int count;float y;};
struct PaletteAction {std::wstring name,shortcut;int command;};
struct ListPosition {int row;float y;bool header;};
struct ListLayout {std::array<ListPosition,VISIBLE_ROWS> items{};int count=0;};
struct CapturedText {DWORD sequence;uint64_t generation;std::wstring text;};
const std::pair<const wchar_t*,const wchar_t*> commands[]={
    {L"launcher.open",L"Open launcher"},{L"apps.open",L"Search apps"},{L"converter.units",L"Unit converter"},
    {L"converter.currency",L"Currency converter"},{L"converter.time",L"Time converter"},{L"emoji.open",L"Emoji picker"},
    {L"clipboard.open",L"Clipboard history"},{L"camera.toggle",L"Webcam mirror"},{L"clipboard.pause",L"Pause / resume clipboard"}
};
LRESULT CALLBACK window_proc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK settings_proc(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK edit_proc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);

struct App {
    HINSTANCE instance;HWND hwnd=nullptr,edit=nullptr,settings=nullptr,previous=nullptr,pasteTarget=nullptr;
    BrandIcons brandIcons;
    UINT dpi=96;float scale=1;HFONT inputFont=nullptr,settingsFont=nullptr;HBRUSH editBrush=nullptr;bool light=system_uses_light_theme();
    Com<ID2D1Factory> factory;NativeSurface surface;Lucide lucide;Com<ID2D1RenderTarget> target;Com<ID2D1SolidColorBrush> brush;Com<IDWriteFactory> textFactory;
    Com<IDWriteTextFormat> body,small,heading,icon,previewText;Com<ID2D1Bitmap> cameraBitmap;
    struct IconBitmap {Com<ID2D1Bitmap> bitmap;uint64_t used=0;};
    AppIcons appIcons;std::map<std::wstring,IconBitmap> iconBitmaps;uint64_t iconClock=0;
    Mode mode=Mode::Home,resultMode=Mode::Home;std::vector<Row> rows;std::vector<AppEntry> apps;int selected=0,scroll=0,hover=-1;std::wstring query,status=L"Ready when you are",toast,converterHint;
    bool queryPending=false,composing=false,positionSaved=false;POINT savedPosition{};SIZE savedSize{PALETTE_WIDTH,PALETTE_HEIGHT};
    std::wstring localCurrency=local_currency_code();
    int editLayoutWidth=0;UINT editLayoutDpi=0;
    std::vector<GridTile> grid;std::vector<GridSection> gridSections;float gridOffset=0,gridHeight=0;int emojiCategory=-1;
    bool conversionCard=false;int popup=0,popupSelected=0;std::vector<PaletteAction> popupActions;
    std::vector<Binding> bindings;std::map<int,std::wstring> hotkeys;int nextHotkey=1;std::vector<std::pair<std::wstring,std::wstring>> settingsCommands;
    ShortcutChord registeredLauncher{};
    bool keepRunning=true,startupSetupDone=false,isolatedData=false;
    std::vector<HistoryItem> history;int historyRetentionDays=7;std::atomic<bool> historyEnabled{false},exiting{false};DWORD ownSequence=0,lastSequence=0;
    HANDLE quitEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr),captureEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr),saveEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    std::thread indexWorker,clipWorker,rateWorker;std::mutex saveMutex;PendingSave<std::vector<HistoryItem>> historySave;std::atomic<uint64_t> historyGeneration{0};bool historyLoaded=false;
    uint64_t historyNoticeRevision=0;std::wstring historyNoticeSuccess,historyNoticeFailure;
    std::map<std::wstring,int> appUsage;PendingSave<std::vector<unsigned char>> usageSave;
    std::filesystem::path dir;std::map<std::wstring,Rate> rates;std::wstring rateFetchedOn,rateError;bool rateBusy=false,indexing=false;
    Camera camera;bool mirrored=true,cameraPending=false;int cameraDevice=0;uint64_t cameraSequence=0;int pasteAttempts=0;bool smoke=false;int smokeStage=0;
    std::chrono::steady_clock::time_point launchTime=std::chrono::steady_clock::now();double readyMs=0;
    App(HINSTANCE h,const std::filesystem::path& overrideDir):instance(h),brandIcons(h),dir(overrideDir.empty()?data_dir():overrideDir){isolatedData=!overrideDir.empty();std::error_code ec;std::filesystem::create_directories(dir,ec);editBrush=CreateSolidBrush(palette_background(light));}
    ~App(){exiting=true;SetEvent(quitEvent);camera.stop();if(indexWorker.joinable())indexWorker.join();if(clipWorker.joinable())clipWorker.join();if(rateWorker.joinable())rateWorker.join();CloseHandle(quitEvent);CloseHandle(captureEvent);CloseHandle(saveEvent);if(inputFont)DeleteObject(inputFont);if(settingsFont)DeleteObject(settingsFont);DeleteObject(editBrush);}
    void invalidate(){if(hwnd)InvalidateRect(hwnd,nullptr,FALSE);}
    void notify(std::wstring s){toast=std::move(s);SetTimer(hwnd,TIMER_TOAST,4000,nullptr);invalidate();}
    HWND control(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){auto c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(settingsFont),TRUE);return c;}
    void load_settings(){
        auto bytes=read_bytes(dir/L"settings.txt",1024*1024);std::istringstream in(std::string(bytes.begin(),bytes.end()));std::string line;
        while(std::getline(in,line)){std::istringstream row(line);std::string type;row>>type;if(type=="history"){int v=0;row>>v;historyEnabled=v==1;}else if(type=="history_retention_days"){int days=7;row>>days;historyRetentionDays=normalize_history_retention_days(days);}else if(type=="close_to_tray"){int v=1;row>>v;keepRunning=v!=0;}else if(type=="startup_setup"){int v=0;row>>v;startupSetupDone=v==1;}else if(type=="position"){LONG x,y,w,h;if(row>>x>>y&&x>=-1000000&&x<=1000000&&y>=-1000000&&y<=1000000){savedPosition={x,y};positionSaved=true;if(row>>w>>h&&w>0&&h>0&&w<=16384&&h<=16384)savedSize={w,h};}}else if(type=="bind"){UINT mods,key;std::string id;if(row>>mods>>key){std::getline(row,id);if(!id.empty()&&id[0]==' ')id.erase(id.begin());auto wide=widen(id);if(!wide.empty()&&key>0&&key<256&&(mods&~15u)==0&&bindings.size()<200)bindings.push_back({wide,mods,key});}}}
        if(bytes.empty()&&!isolatedData)bindings.push_back({L"launcher.open",MOD_CONTROL|MOD_ALT,VK_SPACE});
        for(auto& b:bindings){
            int id=nextHotkey++;
            if(RegisterHotKey(hwnd,id,b.modifiers|MOD_NOREPEAT,b.key)){
                hotkeys[id]=b.id;
                // Keep the actual working chord, even if a manually edited
                // settings file contains more than one launcher binding.
                if(b.id==L"launcher.open"&&!registeredLauncher.key)registeredLauncher={b.modifiers,b.key};
            }else status=L"A shortcut is unavailable. Open Settings to reassign it.";
        }
    }
    bool save_settings(const std::vector<Binding>* proposedBindings=nullptr,std::optional<bool> proposedHistory=std::nullopt){
        // Persist proposed preferences before publishing them to the hotkey
        // dispatcher or clipboard worker. Existing callers save current state.
        const auto& savedBindings=proposedBindings?*proposedBindings:bindings;
        std::ostringstream out;
        out<<"version 1\nhistory "<<(proposedHistory.value_or(historyEnabled.load())?1:0)
            <<"\nhistory_retention_days "<<historyRetentionDays<<"\nclose_to_tray "<<(keepRunning?1:0)
            <<"\nstartup_setup "<<(startupSetupDone?1:0)<<"\n";
        if(positionSaved)out<<"position "<<savedPosition.x<<" "<<savedPosition.y<<" "<<savedSize.cx<<" "<<savedSize.cy<<"\n";
        for(const auto& b:savedBindings)out<<"bind "<<b.modifiers<<" "<<b.key<<" "<<narrow(b.id)<<"\n";
        auto s=out.str();
        if(!write_atomic(dir/L"settings.txt",{s.begin(),s.end()})){notify(L"Couldn't save settings");return false;}
        return true;
    }
    void setup_startup(){
        // This release enables sign-in launch once, as requested. Later user or
        // Windows changes are respected; dev profiles never register startup.
        if(isolatedData)return;
        if(startupSetupDone&&!startup_registered())return;
        std::wstring error;bool ok=set_startup_enabled(true,error);
        if(!startupSetupDone){startupSetupDone=true;save_settings();}
        if(!ok)notify(error.empty()?L"Couldn't enable startup":error);
    }
    void queue_history(std::wstring success={},std::wstring failure={}){
        uint64_t revision;{std::lock_guard lock(saveMutex);revision=historySave.set(history);}
        if(!success.empty()){historyNoticeRevision=revision;historyNoticeSuccess=std::move(success);historyNoticeFailure=std::move(failure);}
        SetEvent(saveEvent);
    }
    static uint64_t history_now(){FILETIME ft;GetSystemTimeAsFileTime(&ft);return (uint64_t(ft.dwHighDateTime)<<32)|ft.dwLowDateTime;}
    void schedule_history_expiry(){
        KillTimer(hwnd,TIMER_HISTORY_EXPIRY);
        if(history.empty())return;
        uint64_t oldest=history.front().timestamp;
        for(const auto& item:history)oldest=std::min(oldest,item.timestamp);
        uint64_t expires=oldest+uint64_t(historyRetentionDays)*history_ticks_per_day;
        uint64_t now=history_now(),delay=expires>now?(expires-now)/10000+1:1;
        // Wake at the next expiry, not on a periodic clipboard polling loop.
        SetTimer(hwnd,TIMER_HISTORY_EXPIRY,UINT(std::clamp<uint64_t>(delay,USER_TIMER_MINIMUM,USER_TIMER_MAXIMUM)),nullptr);
    }
    bool expire_history(){
        bool changed=prune_history(history,history_now(),historyRetentionDays);
        if(changed)queue_history();
        schedule_history_expiry();return changed;
    }
    bool set_history_retention(int days){
        days=normalize_history_retention_days(days);
        if(days==historyRetentionDays)return true;
        int previousDays=historyRetentionDays;historyRetentionDays=days;
        if(!save_settings()){historyRetentionDays=previousDays;return false;}
        // Apply the previous policy before extending it: already-expired data
        // must not reappear just because capture was paused at startup.
        bool changed=false;
        if(!historyLoaded){history=load_history(dir/L"clipboard.bin",previousDays,&changed);historyLoaded=true;}
        uint64_t now=history_now();
        changed|=prune_history(history,now,previousDays);
        changed|=prune_history(history,now,historyRetentionDays);
        if(changed)queue_history();
        schedule_history_expiry();
        if(resultMode==Mode::Clipboard)rebuild();
        return true;
    }
    void load_usage(){
        auto bytes=read_bytes(dir/L"usage.cache",1024*1024);std::istringstream in(std::string(bytes.begin(),bytes.end()));std::string line;
        while(std::getline(in,line)&&appUsage.size()<5000){auto tab=line.find('\t');if(tab==line.npos)continue;std::istringstream number(line.substr(0,tab));int count=0;if(number>>count&&count>0&&count<=1000000){auto key=widen(line.substr(tab+1));if(!key.empty())appUsage[key]=count;}}
    }
    void apply_usage(){for(auto& item:apps){auto it=appUsage.find(item.target);item.uses=it==appUsage.end()?0:it->second;}}
    void record_launch(const std::wstring& target){
        const int uses=std::min(appUsage[target]+1,1000000);appUsage[target]=uses;
        // Shell activation can dispatch messages and replace the app index.
        // Refind by stable identity instead of retaining a vector reference.
        for(auto& item:apps)if(item.target==target)item.uses=uses;
        queue_usage();
    }
    void record_emoji(const std::wstring& glyph){auto& count=appUsage[L"emoji:"+glyph];count=std::min(count+1,1000000);queue_usage();}
    void queue_usage(){
        std::ostringstream out;size_t count=0;for(auto& [target,uses]:appUsage){if(++count>5000)break;out<<uses<<'\t'<<narrow(target)<<'\n';}
        auto bytes=out.str();{std::lock_guard lock(saveMutex);usageSave.set({bytes.begin(),bytes.end()});}SetEvent(saveEvent);
    }
    void flush_storage(){
        std::optional<PendingSave<std::vector<HistoryItem>>::Job> historyJob;
        std::optional<PendingSave<std::vector<unsigned char>>::Job> usageJob;
        {std::lock_guard lock(saveMutex);historyJob=historySave.take();usageJob=usageSave.take();}
        if(historyJob){
            bool saved=save_history(dir/L"clipboard.bin",historyJob->value);
            if(!saved){std::lock_guard lock(saveMutex);historySave.retry(*historyJob);}
            PostMessageW(hwnd,MSG_STATUS,saved?3:1,static_cast<LPARAM>(historyJob->revision));
        }
        if(usageJob&&!write_atomic(dir/L"usage.cache",usageJob->value)){
            {std::lock_guard lock(saveMutex);usageSave.retry(*usageJob);}
            PostMessageW(hwnd,MSG_STATUS,2,0);
        }
    }
    void storage_status(WPARAM status,uint64_t revision){
        if(status==3){
            if(historyNoticeRevision&&revision>=historyNoticeRevision){
                notify(historyNoticeSuccess);if(settings)settings_feedback(historyNoticeSuccess);
                historyNoticeRevision=0;historyNoticeSuccess.clear();historyNoticeFailure.clear();
            }
            return;
        }
        auto message=status==2?L"Could not save application ranking.":L"Could not save clipboard history.";
        if(status==1&&historyNoticeRevision&&revision>=historyNoticeRevision)message=historyNoticeFailure.c_str();
        notify(message);if(settings)settings_feedback(message,true);
    }
    bool enable_history(bool enabled){
        if(!save_settings(nullptr,enabled)){settings_sync_options();return false;}
        historyEnabled=enabled;historyGeneration++;
        if(enabled&&!historyLoaded){bool pruned=false;history=load_history(dir/L"clipboard.bin",historyRetentionDays,&pruned);historyLoaded=true;if(pruned)queue_history();}
        if(historyLoaded)expire_history();
        rebuild();settings_sync_options();return true;
    }
    void start_clipboard(){
        if(historyEnabled){bool pruned=false;history=load_history(dir/L"clipboard.bin",historyRetentionDays,&pruned);historyLoaded=true;if(pruned)queue_history();schedule_history_expiry();}
        clipWorker=std::thread([this]{
            HANDLE events[]={quitEvent,captureEvent,saveEvent};
            while(!exiting){DWORD signaled=WaitForMultipleObjects(3,events,FALSE,INFINITE);if(signaled==WAIT_OBJECT_0)break;
                if(signaled==WAIT_OBJECT_0+2){flush_storage();continue;}
                if(signaled!=WAIT_OBJECT_0+1||!historyEnabled)continue;
                auto generation=historyGeneration.load();
                bool opened=false;for(int retry=0;retry<5&&!exiting;retry++){if(OpenClipboard(nullptr)){opened=true;break;}if(WaitForSingleObject(quitEvent,15)==WAIT_OBJECT_0)break;}
                if(!opened)continue;
                bool excluded=IsClipboardFormatAvailable(RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitor"));
                auto allowedFormat=RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");if(IsClipboardFormatAvailable(allowedFormat)){auto handle=GetClipboardData(allowedFormat);if(handle&&GlobalSize(handle)>=sizeof(DWORD)){auto flag=static_cast<DWORD*>(GlobalLock(handle));if(flag){excluded|=*flag==0;GlobalUnlock(handle);}}}
                std::wstring text;
                if(!excluded&&IsClipboardFormatAvailable(CF_UNICODETEXT)){auto data=GetClipboardData(CF_UNICODETEXT);SIZE_T bytes=data?GlobalSize(data):0;if(bytes>0&&bytes<=history_max_item_bytes){auto p=static_cast<const wchar_t*>(GlobalLock(data));if(p){size_t max=bytes/sizeof(wchar_t),length=0;while(length<max&&p[length])length++;if(length<max)text.assign(p,length);GlobalUnlock(data);}}}
                DWORD sequence=GetClipboardSequenceNumber();CloseClipboard();
                if(!text.empty()&&historyEnabled){auto* item=new CapturedText{sequence,generation,std::move(text)};if(!PostMessageW(hwnd,MSG_CLIP,0,reinterpret_cast<LPARAM>(item)))delete item;}
            }
            flush_storage();
        });
    }
    void load_apps(){auto bytes=read_bytes(dir/L"apps.cache",4*1024*1024);std::istringstream in(std::string(bytes.begin(),bytes.end()));std::string line;while(std::getline(in,line)&&apps.size()<5000){auto a=line.find('\t'),b=a==line.npos?a:line.find('\t',a+1);if(a==line.npos||b==line.npos)continue;auto name=widen(line.substr(0,a)),path=widen(line.substr(b+1));if(!name.empty()&&!path.empty())apps.push_back({name,path,lower(name),line.substr(a+1,b-a-1)=="1"});}}
    void scan_apps(){if(indexing)return;if(indexWorker.joinable())indexWorker.join();appIcons.invalidate();iconBitmaps.clear();indexing=true;indexWorker=std::thread([this]{auto found=discover_apps(exiting);if(exiting)return;std::ostringstream out;for(auto& a:found)out<<narrow(a.name)<<'\t'<<(a.packaged?1:0)<<'\t'<<narrow(a.target)<<'\n';auto s=out.str();write_atomic(dir/L"apps.cache",{s.begin(),s.end()});auto* result=new std::vector<AppEntry>(std::move(found));if(!PostMessageW(hwnd,MSG_APPS,0,reinterpret_cast<LPARAM>(result)))delete result;});}
    void load_rates(){
        auto bytes=read_bytes(dir/L"rates.cache",128*1024);std::istringstream in(std::string(bytes.begin(),bytes.end()));std::string line;
        while(std::getline(in,line)){std::istringstream row(line);std::string a,b,c,d;if(!(row>>a>>b))continue;if(a=="fetched"){rateFetchedOn=widen(b);continue;}if(!(row>>c>>d)||a!="USD"||b.size()!=3||c.size()>32||d.size()!=10)continue;Rate r{widen(a),widen(b),widen(c),widen(d),{}};if(currency_result(L"1",r.value,6))rates[r.to]=r;}
    }
    void save_rates(){std::ostringstream out;out<<"fetched "<<narrow(rateFetchedOn)<<'\n';size_t count=0;for(auto& [key,r]:rates){if(++count>512)break;out<<narrow(r.from)<<' '<<narrow(r.to)<<' '<<narrow(r.value)<<' '<<narrow(r.date)<<'\n';}auto s=out.str();write_atomic(dir/L"rates.cache",{s.begin(),s.end()});}
    std::wstring today(){SYSTEMTIME st;GetLocalTime(&st);wchar_t b[16];swprintf(b,16,L"%04u-%02u-%02u",st.wYear,st.wMonth,st.wDay);return b;}
    void refresh_rates(bool force=false){
        if(rateBusy||(!force&&!rates.empty()&&rateFetchedOn==today()))return;
        if(rateWorker.joinable())rateWorker.join();rateBusy=true;rateError.clear();
        rateWorker=std::thread([this]{auto* snapshot=new RateSnapshot(fetch_daily_rates());if(exiting||!PostMessageW(hwnd,MSG_RATE,0,reinterpret_cast<LPARAM>(snapshot)))delete snapshot;});
    }
    void text(const std::wstring& s,float x,float y,float w,float h,IDWriteTextFormat* font,D2D1_COLOR_F color){brush->SetColor(color);target->DrawText(s.c_str(),static_cast<UINT32>(s.size()),font,D2D1::RectF(x,y,x+w,y+h),brush.p,D2D1_DRAW_TEXT_OPTIONS_CLIP|D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);}
    static D2D1_COLOR_F color(int r,int g,int b,float a=1){return D2D1::ColorF(r/255.f,g/255.f,b/255.f,a);}
    void rect(float x,float y,float w,float h,D2D1_COLOR_F c,float radius=0){brush->SetColor(c);if(radius)target->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+h),radius,radius),brush.p);else target->FillRectangle(D2D1::RectF(x,y,x+w,y+h),brush.p);}
    bool init_graphics(){
        if(!factory&&FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,IID_ID2D1Factory,nullptr,reinterpret_cast<void**>(factory.put()))))return false;
        if(!textFactory){if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(textFactory.put()))))return false;
            textFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,14,L"en-us",body.put());
            textFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,12,L"en-us",small.put());
            textFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,24,L"en-us",heading.put());
            textFactory->CreateTextFormat(L"Segoe UI Emoji",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,42,L"en-us",icon.put());
            textFactory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,14,L"en-us",previewText.put());
            if(previewText)previewText->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            for(auto* f:{body.p,small.p,heading.p,icon.p})if(f){f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};Com<IDWriteInlineObject> ellipsis;textFactory->CreateEllipsisTrimmingSign(f,ellipsis.put());f->SetTrimming(&trimming,ellipsis.p);}
        }
        if(!target){if(!surface.create(hwnd,factory.p,target,light))return false;target->CreateSolidColorBrush(color(255,255,255),brush.put());}
        return bool(brush)&&bool(body)&&bool(small)&&bool(heading)&&bool(icon);
    }
    void draw_icon(const std::string& name,float x,float y,float size,D2D1_COLOR_F tint){brush->SetColor(tint);lucide.draw(factory.p,target.p,brush.p,name,x,y,size);}
    void release_graphics(){iconBitmaps.clear();cameraBitmap.reset();brush.reset();target.reset();surface.reset();cameraSequence=0;}
    bool draw_app_icon(const std::wstring& key,float x,float y){
        auto found=iconBitmaps.find(key);
        if(found==iconBitmaps.end()){
            auto image=appIcons.get(key);if(!image||image->pixels.empty())return false;
            IconBitmap entry;auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
            if(FAILED(target->CreateBitmap(D2D1::SizeU(image->width,image->height),image->pixels.data(),image->width*4,properties,entry.bitmap.put())))return false;
            if(iconBitmaps.size()>=64){auto oldest=std::min_element(iconBitmaps.begin(),iconBitmaps.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});iconBitmaps.erase(oldest);}
            found=iconBitmaps.emplace(key,std::move(entry)).first;
        }
        found->second.used=++iconClock;
        auto size=found->second.bitmap->GetSize();float ratio=22.f/std::max(size.width,size.height);
        float width=size.width*ratio,height=size.height*ratio;
        target->DrawBitmap(found->second.bitmap.p,D2D1::RectF(x+(22-width)/2,y+(22-height)/2,x+(22+width)/2,y+(22+height)/2));return true;
    }
#include "palette.inl"
    void hint(){InvalidateRect(edit,nullptr,TRUE);}
    void set_mode(Mode next){popup=0;gridOffset=0;cameraPending=false;if(mode==Mode::Camera&&next!=Mode::Camera){camera.stop();cameraBitmap.reset();cameraSequence=0;}mode=next;query.clear();SetWindowTextW(edit,L"");selected=scroll=0;hint();if(next==Mode::Camera)cameraPending=!camera.start(hwnd,MSG_CAMERA,cameraDevice);rebuild(false);if(IsWindowVisible(hwnd))SetFocus(edit);}
    void refresh_region(){auto current=local_currency_code();if(current!=localCurrency){localCurrency=std::move(current);rebuild();}}
    void place_palette(){
        POINT point=savedPosition;if(!positionSaved)GetCursorPos(&point);
        RECT savedBounds{point.x,point.y,point.x+savedSize.cx,point.y+savedSize.cy};
        auto destination=positionSaved?MonitorFromRect(&savedBounds,MONITOR_DEFAULTTONEAREST):MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitor{sizeof(monitor)};if(!GetMonitorInfoW(destination,&monitor))return;
        auto work=monitor.rcWork;
        // Move while hidden first: Windows delivers the destination monitor's DPI
        // before the final size and position are calculated.
        SetWindowPos(hwnd,nullptr,work.left,work.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        int width=MulDiv(PALETTE_WIDTH,GetDpiForWindow(hwnd),96),height=MulDiv(PALETTE_HEIGHT,GetDpiForWindow(hwnd),96);
        LONG x=positionSaved?savedPosition.x:work.left+(work.right-work.left-width)/2;
        LONG y=positionSaved?savedPosition.y:work.top+std::max<LONG>(10,(work.bottom-work.top-height)/3);
        if(positionSaved&&MonitorFromRect(&savedBounds,MONITOR_DEFAULTTONULL)){
            // Preserve deliberate straddling positions; keep a draggable part
            // of the header reachable if work areas or DPI changed.
            LONG grip=MulDiv(80,GetDpiForWindow(hwnd),96);
            x=std::clamp(x,work.left-width+grip,std::max(work.left-width+grip,work.right-grip));
            y=std::clamp(y,work.top,std::max(work.top,work.bottom-MulDiv(65,GetDpiForWindow(hwnd),96)));
        }else{
            x=std::clamp(x,work.left,std::max(work.left,work.right-width));
            y=std::clamp(y,work.top,std::max(work.top,work.bottom-height));
        }
        SetWindowPos(hwnd,HWND_TOPMOST,x,y,width,height,SWP_NOACTIVATE);
    }
    void remember_position(){
        RECT bounds{};if(!GetWindowRect(hwnd,&bounds))return;
        SIZE size{bounds.right-bounds.left,bounds.bottom-bounds.top};
        if(positionSaved&&savedPosition.x==bounds.left&&savedPosition.y==bounds.top&&savedSize.cx==size.cx&&savedSize.cy==size.cy)return;
        savedPosition={bounds.left,bounds.top};savedSize=size;positionSaved=true;save_settings();
    }
    void begin_drag(HWND source,LPARAM point){
        POINT screen{GET_X_LPARAM(point),GET_Y_LPARAM(point)};ClientToScreen(source,&screen);
        ReleaseCapture();SendMessageW(hwnd,WM_NCLBUTTONDOWN,HTCAPTION,MAKELPARAM(screen.x,screen.y));
    }
    void show(Mode next,bool toggle=true){
        if(IsWindowVisible(hwnd)&&mode==next&&toggle){hide();return;}
        KillTimer(hwnd,TIMER_TRIM_GRAPHICS);
        refresh_theme();refresh_region();
        const bool opening=!IsWindowVisible(hwnd);
        if(opening){previous=GetForegroundWindow();place_palette();}
        set_mode(next);
        if(opening){
            // Populate and present the new mode while hidden. Showing an empty
            // composition surface before WM_PAINT caused an intermittent flash.
            // Retry once if the graphics device was lost while in the tray.
            if(!render_frame())render_frame();
            ShowWindow(hwnd,SW_SHOW);
        }
        SetForegroundWindow(hwnd);SetFocus(edit);
        // Also covers the opaque HWND fallback, whose hidden draw may be clipped.
        RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
    }
    void hide(bool restoreFocus=true){popup=0;appIcons.cancel_pending();cameraPending=false;camera.stop();cameraBitmap.reset();cameraSequence=0;if(!IsWindowVisible(hwnd))return;ShowWindow(hwnd,SW_HIDE);SetTimer(hwnd,TIMER_TRIM_GRAPHICS,60000,nullptr);if(restoreFocus&&previous&&IsWindow(previous))SetForegroundWindow(previous);}
    void update_query(){
        int length=GetWindowTextLengthW(edit);std::wstring latest(length+1,L'\0');
        GetWindowTextW(edit,latest.data(),length+1);latest.resize(length);
        if(latest==query)return;
        query=std::move(latest);selected=scroll=0;gridOffset=0;popup=0;toast.clear();rebuild(false);
    }
#include "results.inl"
    void move(int delta){update_query();if(rows.empty())return;selected=std::clamp(selected+delta,0,int(rows.size())-1);ensure_selection_visible();hover=-1;invalidate();}
    void activate(bool copyOnly=false){
        update_query();
        if(mode==Mode::Camera){snapshot(copyOnly);return;}
        if(rows.empty()||selected>=int(rows.size()))return;auto row=rows[selected];
        switch(row.action){
            case Action::Mode:set_mode(static_cast<Mode>(row.index));break;
            case Action::Query:SetWindowTextW(edit,row.title.c_str());update_query();SetFocus(edit);break;
            case Action::RefreshRates:refresh_rates(true);rebuild();break;
            case Action::RefreshApps:scan_apps();rebuild();break;
            case Action::Settings:open_settings();break;
            case Action::Updates:open_releases();break;
            case Action::About:notify(L"Minicast " MINICAST_VERSION_WSTRING);break;
            case Action::EnableHistory:enable_history(true);break;
            case Action::ClearHistory:clear_history();break;
            case Action::App:if(row.index>=0&&row.index<int(apps.size())){auto item=apps[row.index];hide();if(launch_app(hwnd,item))record_launch(item.target);else{show(Mode::Apps,false);notify(L"Couldn't open app. Try Refresh Apps.");}}break;
            case Action::Copy:
                if(resultMode==Mode::Clipboard && row.identity){
                    auto entry=std::find_if(history.begin(),history.end(),[&](const auto& item){return item.timestamp==row.identity;});
                    if(entry==history.end())return;
                    row.value=entry->text;
                }
                if(row.value.empty()){if(mode==Mode::Convert&&query.empty()){SetWindowTextW(edit,row.title.c_str());update_query();}return;}
                if(!copy_text(hwnd,row.value)){notify(L"Clipboard is busy. Try again.");return;}ownSequence=GetClipboardSequenceNumber();
                if(resultMode==Mode::Emoji)record_emoji(row.value);
                if((resultMode==Mode::Emoji||resultMode==Mode::Clipboard)&&!copyOnly){pasteTarget=previous;hide();pasteAttempts=0;SetTimer(hwnd,TIMER_PASTE,25,nullptr);}else notify(L"Copied");break;
        }
    }
    void open_releases(){
        constexpr auto url=MINICAST_RELEASES_URL_WSTRING;
        if(!*url){notify(L"Release page isn't configured for this build.");settings_feedback(L"Release page isn't configured for this build.",true);return;}
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd,L"open",url,nullptr,nullptr,SW_SHOWNORMAL))<=32)
            notify(L"Couldn't open the release page.");
        else hide(false);
    }
    void command(const std::wstring& id){
        if(id.starts_with(L"app:")){auto target=id.substr(4);auto it=std::find_if(apps.begin(),apps.end(),[&](auto& a){return a.target==target;});if(it!=apps.end()){auto item=*it;if(launch_app(hwnd,item))record_launch(target);}else{show(Mode::Apps,false);notify(L"This shortcut's app is no longer indexed.");}return;}
        if(id==L"clipboard.pause"){enable_history(!historyEnabled);return;}
        Mode next=Mode::Home;if(id==L"apps.open")next=Mode::Apps;if(id.starts_with(L"converter."))next=Mode::Convert;if(id==L"emoji.open")next=Mode::Emoji;if(id==L"clipboard.open")next=Mode::Clipboard;if(id==L"camera.toggle")next=Mode::Camera;
        converterHint=id==L"converter.currency"?L"100 usd to idr":id==L"converter.time"?L"time Jakarta · 9am Jakarta to London tomorrow":L"25 c to f · 5 ft 10 in to cm · 2 gb to mib";show(next);
    }
    void clear_history(){history.clear();historyLoaded=true;historyGeneration++;queue_history(L"History cleared.",L"Couldn't clear saved history. Try Clear again.");schedule_history_expiry();rebuild();notify(L"Clearing history…");}
    void accept_clip(DWORD sequence,std::wstring text){
        if(!historyEnabled||sequence==ownSequence||sequence==lastSequence)return;lastSequence=sequence;
        history.erase(std::remove_if(history.begin(),history.end(),[&](auto& h){return h.text==text;}),history.end());uint64_t now=history_now();
        history.insert(history.begin(),{std::move(text),now});prune_history(history,now,historyRetentionDays);queue_history();schedule_history_expiry();if(resultMode==Mode::Clipboard)rebuild();
    }
    void snapshot(bool copy){auto frame=camera.frame(mirrored);if(frame.pixels.empty()){notify(L"Wait for the camera's first frame.");return;}if(copy){if(copy_frame(hwnd,frame)){ownSequence=GetClipboardSequenceNumber();notify(L"Snapshot copied");}else notify(L"Could not copy snapshot");return;}
        Com<IFileSaveDialog> dialog;if(FAILED(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_IFileSaveDialog,reinterpret_cast<void**>(dialog.put())))){notify(L"Cannot open Save dialog");return;}COMDLG_FILTERSPEC types[]={{L"PNG image",L"*.png"}};dialog->SetFileTypes(1,types);dialog->SetDefaultExtension(L"png");dialog->SetFileName(L"Minicast snapshot.png");if(SUCCEEDED(dialog->Show(hwnd))){Com<IShellItem> item;if(SUCCEEDED(dialog->GetResult(item.put()))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){bool ok=save_png(path,frame);CoTaskMemFree(path);notify(ok?L"Snapshot saved":L"Could not save snapshot");}}}}
    void camera_click(int index){if(index==0)snapshot(false);if(index==1)snapshot(true);if(index==2){mirrored=!mirrored;invalidate();}if(index==3){auto devices=camera.devices();if(devices.size()<2){notify(L"Only one camera is available");return;}cameraDevice=(cameraDevice+1)%devices.size();cameraPending=true;camera.stop();cameraBitmap.reset();cameraSequence=0;if(!camera.running())cameraPending=!camera.start(hwnd,MSG_CAMERA,cameraDevice);notify(L"Switching camera…");}}
#include "settings.inl"
    void tray(bool update=false){
        NOTIFYICONDATAW data{sizeof(data)};data.hWnd=hwnd;data.uID=1;data.uFlags=NIF_TIP;
        std::wstring tip=L"Minicast";
        if(registeredLauncher.key)tip+=L" · "+hotkey_name({L"launcher.open",registeredLauncher.modifiers,registeredLauncher.key});
        else if(std::any_of(bindings.begin(),bindings.end(),[](const Binding& binding){return binding.id==L"launcher.open"&&binding.key;}))
            tip+=L" · Shortcut unavailable";
        wcsncpy_s(data.szTip,std::size(data.szTip),tip.c_str(),_TRUNCATE);
        bool iconChanged=brandIcons.refresh_tray();
        if(!update||iconChanged){data.uFlags|=NIF_ICON;data.hIcon=brandIcons.tray()?brandIcons.tray():brandIcons.small();}
        if(!update){data.uFlags|=NIF_MESSAGE;data.uCallbackMessage=MSG_TRAY;}
        // Updates do not remove/re-add the icon. If Explorer has dropped it,
        // attempt one add; TaskbarCreated also restores the current label.
        if(!Shell_NotifyIconW(update?NIM_MODIFY:NIM_ADD,&data)&&update)tray(false);
    }
    void tray_menu(){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Open Minicast");AppendMenuW(menu,MF_STRING,2,L"Settings");AppendMenuW(menu,MF_STRING,3,historyEnabled?L"Pause clipboard history":L"Enable clipboard history");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,4,L"Exit");POINT p;GetCursorPos(&p);SetForegroundWindow(hwnd);int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);DestroyMenu(menu);if(result==1)show(Mode::Home,false);if(result==2)open_settings();if(result==3){enable_history(!historyEnabled);}if(result==4)DestroyWindow(hwnd);}
    void smoke_sample(const char* phase){PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));DWORD handles=0;GetProcessHandleCount(GetCurrentProcess(),&handles);std::ofstream out(dir/L"smoke-report.txt",std::ios::app);out<<phase<<" private_bytes="<<memory.PrivateUsage<<" working_set="<<memory.WorkingSetSize<<" handles="<<handles<<" apps="<<apps.size()<<" ready_ms="<<readyMs<<"\n";}
};
App* app=nullptr;
LRESULT CALLBACK settings_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    return app?app->settings_message(hwnd,msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CALLBACK edit_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if((msg==WM_LBUTTONDOWN||msg==WM_LBUTTONDBLCLK)&&(GetKeyState(VK_MENU)&0x8000)){app->begin_drag(hwnd,lp);return 0;}
    if(msg==WM_IME_STARTCOMPOSITION)app->composing=true;
    if(msg==WM_IME_ENDCOMPOSITION)app->composing=false;
    // Candidate selection belongs to the IME until the composition is committed.
    if((msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN)&&!app->composing){
        bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        bool alt=(GetKeyState(VK_MENU)&0x8000)!=0;
        if(wp==VK_ESCAPE||(alt&&wp==VK_LEFT)){app->go_back();return 0;}
        if(ctrl&&wp=='K'){app->open_actions();return 0;}
        if(app->popup){
            if(wp==VK_UP||wp==VK_DOWN||wp==VK_TAB){int delta=wp==VK_UP||((GetKeyState(VK_SHIFT)&0x8000)&&wp==VK_TAB)?-1:1;app->popupSelected=std::clamp(app->popupSelected+delta,0,int(app->popupActions.size())-1);app->invalidate();return 0;}
            if(wp==VK_RETURN){app->run_popup_action();return 0;}
            if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_NEXT||wp==VK_PRIOR||wp==VK_HOME||wp==VK_END)return 0;
        }
        if(wp==VK_DOWN){if(app->emoji_view())app->move_grid_vertical(1);else app->move(1);return 0;}
        if(wp==VK_UP){if(app->emoji_view())app->move_grid_vertical(-1);else app->move(-1);return 0;}
        if(app->emoji_view()&&!ctrl&&(wp==VK_LEFT||wp==VK_RIGHT)){app->move(wp==VK_LEFT?-1:1);return 0;}
        if(wp==VK_NEXT){app->move(app->emoji_view()?24:app->list_capacity());return 0;}
        if(wp==VK_PRIOR){app->move(app->emoji_view()?-24:-app->list_capacity());return 0;}
        if(ctrl&&wp==VK_HOME){app->move(-10000);return 0;}
        if(ctrl&&wp==VK_END){app->move(10000);return 0;}
        if(wp==VK_RETURN){app->activate(ctrl);return 0;}
        if(wp==VK_TAB){app->move((GetKeyState(VK_SHIFT)&0x8000)?-1:1);return 0;}
        if(ctrl&&wp=='A'){SendMessageW(hwnd,EM_SETSEL,0,-1);return 0;}
        if(ctrl&&wp==VK_BACK){
            DWORD start=0,end=0;SendMessageW(hwnd,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
            if(start==end){
                int length=GetWindowTextLengthW(hwnd);std::wstring content(length+1,L'\0');GetWindowTextW(hwnd,content.data(),length+1);content.resize(length);
                while(start>0&&iswspace(content[start-1]))--start;
                if(start>0){bool word=iswalnum(content[start-1])!=0;while(start>0&&!iswspace(content[start-1])&&(iswalnum(content[start-1])!=0)==word)--start;}
            }
            SendMessageW(hwnd,EM_SETSEL,start,end);SendMessageW(hwnd,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(L""));return 0;
        }
        if(ctrl&&wp==VK_OEM_COMMA){app->open_settings();return 0;}
    }
    if(msg==WM_PAINT&&!search_input_is_composited(hwnd)){auto result=DefSubclassProc(hwnd,msg,wp,lp);if(GetWindowTextLengthW(hwnd)==0){HDC dc=GetDC(hwnd);auto previous=SelectObject(dc,app->inputFont);SetTextColor(dc,app->light?RGB(149,152,155):RGB(142,142,153));SetBkMode(dc,TRANSPARENT);RECT rect;GetClientRect(hwnd,&rect);auto text=app->placeholder();DrawTextW(dc,text.c_str(),-1,&rect,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);SelectObject(dc,previous);ReleaseDC(hwnd,dc);}return result;}
    if(msg==WM_CHAR&&!app->composing&&(wp==VK_RETURN||wp==VK_ESCAPE||wp==VK_TAB||wp==1||wp==11||wp==127))return 0;return DefSubclassProc(hwnd,msg,wp,lp);
}
LRESULT CALLBACK window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(!app)return DefWindowProcW(hwnd,msg,wp,lp);
    static const UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    if(taskbarCreated&&msg==taskbarCreated){app->tray();return 0;}
    switch(msg){
        case WM_PAINT:app->paint();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_CTLCOLOREDIT:{HDC dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,app->light?RGB(19,20,23):RGB(237,237,242));SetBkColor(dc,app->edit_background());return reinterpret_cast<LRESULT>(app->editBrush);}
        case WM_SETTINGCHANGE:app->refresh_region();app->refresh_theme(true);app->tray(true);return 0;
        case WM_THEMECHANGED:app->refresh_theme(true);app->tray(true);return 0;
        case WM_DWMCOMPOSITIONCHANGED:app->refresh_theme(true);return 0;
        case WM_DWMCOLORIZATIONCOLORCHANGED:app->invalidate();return 0;
        case WM_COMMAND:if(LOWORD(wp)==ID_QUERY&&HIWORD(wp)==EN_CHANGE&&!app->queryPending){app->queryPending=PostMessageW(hwnd,MSG_QUERY,0,0)!=0;}return 0;
        case MSG_QUERY:app->queryPending=false;app->update_query();return 0;
        case MSG_ICONS:if(IsWindowVisible(hwnd))app->invalidate();return 0;
        case WM_TIMER:
            if(wp==TIMER_HISTORY_EXPIRY){if(app->expire_history()&&app->resultMode==Mode::Clipboard)app->rebuild();}
            if(wp==TIMER_TOAST){KillTimer(hwnd,TIMER_TOAST);app->toast.clear();app->invalidate();}
            if(wp==TIMER_TRIM_GRAPHICS){KillTimer(hwnd,TIMER_TRIM_GRAPHICS);if(!IsWindowVisible(hwnd))app->surface.trim();}
            if(wp==TIMER_PASTE){if(GetForegroundWindow()!=app->pasteTarget){KillTimer(hwnd,TIMER_PASTE);return 0;}if(paste_to(app->pasteTarget)||++app->pasteAttempts>=20)KillTimer(hwnd,TIMER_PASTE);}
            if(wp==TIMER_SMOKE){if(app->smokeStage==0){app->smoke_sample("open");app->set_mode(Mode::Convert);SetWindowTextW(app->edit,L"25 c to f");app->update_query();}else if(app->smokeStage==1){app->smoke_sample("converter");app->hide();}else if(app->smokeStage==2){app->smoke_sample("idle");DestroyWindow(hwnd);}app->smokeStage++;}return 0;
        case WM_HOTKEY:
            if(auto it=app->hotkeys.find(static_cast<int>(wp));it!=app->hotkeys.end()){
                if(!app->settings_capture_registered_hotkey(lp))app->command(it->second);
            }
            return 0;
        case WM_CLIPBOARDUPDATE:if(app->historyEnabled&&GetClipboardSequenceNumber()!=app->ownSequence)SetEvent(app->captureEvent);return 0;
        case MSG_APPS:{std::unique_ptr<std::vector<AppEntry>> found(reinterpret_cast<std::vector<AppEntry>*>(lp));app->apps=std::move(*found);app->apply_usage();app->indexing=false;app->rebuild();app->settings_refresh_commands();return 0;}
        case MSG_CLIP:{std::unique_ptr<CapturedText> item(reinterpret_cast<CapturedText*>(lp));if(item->generation==app->historyGeneration)app->accept_clip(item->sequence,std::move(item->text));return 0;}
        case MSG_RATE:{std::unique_ptr<RateSnapshot> snapshot(reinterpret_cast<RateSnapshot*>(lp));app->rateBusy=false;app->rateError=snapshot->error;if(snapshot->error.empty()){app->rates.clear();for(auto& rate:snapshot->rates)app->rates[rate.to]=std::move(rate);app->rateFetchedOn=app->today();app->save_rates();}app->rebuild();return 0;}
        case MSG_CAMERA:if(app->mode==Mode::Camera&&IsWindowVisible(hwnd)){if(app->cameraPending&&!app->camera.running())app->cameraPending=!app->camera.start(hwnd,MSG_CAMERA,app->cameraDevice);app->invalidate();}return 0;
        case MSG_STATUS:app->storage_status(wp,static_cast<uint64_t>(lp));return 0;
        case MSG_SHOW:app->show(Mode::Home,false);return 0;
        case MSG_TRAY:if(lp==WM_LBUTTONUP)app->show(Mode::Home);if(lp==WM_RBUTTONUP)app->tray_menu();return 0;
        case WM_NCHITTEST:{POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&point);RECT client{};GetClientRect(hwnd,&client);if(!PtInRect(&client,point))break;if((GetKeyState(VK_MENU)&0x8000)||app->draggable_at(point.x/app->scale,point.y/app->scale))return HTCAPTION;return HTCLIENT;}
        case WM_NCLBUTTONDBLCLK:if(wp==HTCAPTION)return 0;break;
        case WM_ENTERSIZEMOVE:app->popup=0;app->hover=-1;app->invalidate();return 0;
        case WM_EXITSIZEMOVE:app->remember_position();if(GetForegroundWindow()==hwnd)SetFocus(app->edit);return 0;
        case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:app->click_palette(GET_X_LPARAM(lp)/app->scale,GET_Y_LPARAM(lp)/app->scale,msg==WM_LBUTTONDBLCLK);return 0;
        case WM_MOUSEMOVE:{TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,hwnd,0};TrackMouseEvent(&tracking);float x=GET_X_LPARAM(lp)/app->scale,y=GET_Y_LPARAM(lp)/app->scale;if(app->popup){auto r=app->popup_bounds();if(x>=r.left&&x<r.right&&y>=r.top+32&&y<r.bottom-6){int selected=int((y-r.top-32)/32);if(selected!=app->popupSelected){app->popupSelected=selected;app->invalidate();}}return 0;}int h=app->row_at(x,y);if(h!=app->hover){app->hover=h;app->invalidate();}return 0;}
        case WM_MOUSELEAVE:app->hover=-1;app->invalidate();return 0;
        case WM_MOUSEWHEEL:app->wheel(GET_WHEEL_DELTA_WPARAM(wp));return 0;
        case WM_DPICHANGED:{auto* r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);app->layout();app->invalidate();return 0;}
        case WM_DISPLAYCHANGE:app->tray(true);app->invalidate();return 0; // Restore/clamp on the next show, without visible intermediate moves.
        case WM_SIZE:if(app->target){if(!app->surface.resize(LOWORD(lp),HIWORD(lp)))app->release_graphics();app->invalidate();}return 0;
        case WM_ACTIVATEAPP:if(!wp&&IsWindowVisible(hwnd))app->hide(false);return 0;
        case WM_TIMECHANGE:if(app->expire_history()&&app->resultMode==Mode::Clipboard)app->rebuild();return 0;
        case WM_POWERBROADCAST:if(wp==PBT_APMSUSPEND){app->cameraPending=false;app->camera.stop();}else if(wp==PBT_APMRESUMEAUTOMATIC){if(app->expire_history()&&app->resultMode==Mode::Clipboard)app->rebuild();}return TRUE;
        case WM_WTSSESSION_CHANGE:if(wp==WTS_SESSION_LOCK){app->cameraPending=false;app->camera.stop();}return 0;
        case WM_CLOSE:if(app->keepRunning)app->hide();else DestroyWindow(hwnd);return 0;
        case WM_DESTROY:{app->exiting=true;app->camera.stop();RemoveClipboardFormatListener(hwnd);WTSUnRegisterSessionNotification(hwnd);NOTIFYICONDATAW d{sizeof(d)};d.hWnd=hwnd;d.uID=1;Shell_NotifyIconW(NIM_DELETE,&d);for(auto& [id,command]:app->hotkeys)UnregisterHotKey(hwnd,id);PostQuitMessage(0);return 0;}
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR args,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    (void)args;
    std::filesystem::path overrideDir;bool background=false,inspect=false,smoke=false;
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool invalidArguments=!argv;
    if(argv){for(int i=1;i<argc;i++){
        const std::wstring option=argv[i];
        if(option==L"--data-dir"){
            if(i+1>=argc||!*argv[i+1]||std::wstring_view(argv[i+1]).starts_with(L"--")){invalidArguments=true;break;}
            overrideDir=argv[++i];
        }
        else if(option==L"--background")background=true;
        else if(option==L"--inspect")inspect=true;
        else if(option==L"--smoke")smoke=true;
        else invalidArguments=true;
    }LocalFree(argv);}
    if(invalidArguments){CoUninitialize();return 1;}
    std::wstring profile;
    if(!overrideDir.empty()){
        std::error_code error;overrideDir=std::filesystem::absolute(overrideDir,error).lexically_normal();
        if(error){CoUninitialize();return 1;}
        profile=overrideDir.wstring();CharLowerBuffW(profile.data(),static_cast<DWORD>(profile.size()));
        while(profile.size()>3&&(profile.back()==L'\\'||profile.back()==L'/'))profile.pop_back();
    }
    const auto paletteClass=instance_class(profile);
    HWND existing=FindWindowW(paletteClass.c_str(),nullptr);if(existing){if(!background)PostMessageW(existing,MSG_SHOW,0,0);CoUninitialize();return 0;}
    INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_STANDARD_CLASSES|ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);
    auto application=std::make_unique<App>(instance,overrideDir);app=application.get();app->smoke=smoke;
    WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=instance;wc.lpfnWndProc=window_proc;wc.lpszClassName=paletteClass.c_str();wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=app->brandIcons.large();wc.hIconSm=app->brandIcons.small();wc.style=CS_DROPSHADOW|CS_DBLCLKS;RegisterClassExW(&wc);
    wc.lpfnWndProc=settings_proc;wc.lpszClassName=L"MinicastSettings";wc.hbrBackground=GetSysColorBrush(COLOR_BTNFACE);RegisterClassExW(&wc);
    DWORD extendedStyle=WS_EX_TOPMOST|(inspect?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW);
    app->hwnd=CreateWindowExW(extendedStyle,paletteClass.c_str(),app->isolatedData?L"Minicast — Test Profile":L"Minicast",WS_POPUP|WS_CLIPCHILDREN,100,100,PALETTE_WIDTH,PALETTE_HEIGHT,nullptr,nullptr,instance,nullptr);if(!app->hwnd)return 1;
    apply_window_theme(app->hwnd,app->light);apply_palette_frame(app->hwnd);
    app->edit=create_search_input(app->hwnd,ID_QUERY,instance);if(!app->edit)return 1;
    if(search_input_is_composited(app->edit))SetWindowLongPtrW(app->hwnd,GWL_STYLE,GetWindowLongPtrW(app->hwnd,GWL_STYLE)&~WS_CLIPCHILDREN);
    app->surface.set_transparent_input(search_input_is_composited(app->edit));set_search_input_theme(app->edit,app->light);
    SendMessageW(app->edit,EM_SETLIMITTEXT,256,0);SetWindowSubclass(app->edit,edit_proc,1,0);app->layout();
    app->settingsFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    app->load_settings();app->setup_startup();app->load_usage();app->load_apps();app->apply_usage();app->load_rates();app->start_clipboard();AddClipboardFormatListener(app->hwnd);WTSRegisterSessionNotification(app->hwnd,NOTIFY_FOR_THIS_SESSION);app->tray();app->hint();app->scan_apps();app->refresh_rates();app->rebuild();
    app->readyMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-app->launchTime).count();
    if(!background)app->show(Mode::Home,false);if(app->smoke)SetTimer(app->hwnd,TIMER_SMOKE,2000,nullptr);
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){
        // Record before dialog/system-key translation (especially Alt+Space).
        if(app->settings_capture_message(msg))continue;
        if(app->settings&&IsDialogMessageW(app->settings,&msg))continue;
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    // Drain posted heap-owned results after workers are stopped in App destruction.
    application.reset();app=nullptr;CoUninitialize();return 0;
}
