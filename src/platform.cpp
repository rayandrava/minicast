#include "platform.hpp"
#include "app_shell.hpp"
#include "history_retention.hpp"
#include "clipboard_image.hpp"
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <fstream>
#include <algorithm>
#include <set>
#include <sstream>
#include <iomanip>

namespace mini {
std::wstring widen(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(!n)return {};std::wstring r(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n);return r;}
std::string narrow(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);std::string r(n,' ');WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n,nullptr,nullptr);return r;}
std::filesystem::path data_dir(){PWSTR p=nullptr;std::filesystem::path dir;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&p))){dir=p;CoTaskMemFree(p);}else{wchar_t tmp[MAX_PATH];GetTempPathW(MAX_PATH,tmp);dir=tmp;}dir/=L"Minicast";std::error_code ec;std::filesystem::create_directories(dir,ec);return dir;}
std::wstring error_text(HRESULT code){wchar_t* p=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,reinterpret_cast<LPWSTR>(&p),0,nullptr);std::wstring s=p?trim(p):L"Windows could not complete this action";if(p)LocalFree(p);return s;}
std::vector<unsigned char> read_bytes(const std::filesystem::path& path,size_t limit){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return {};auto len=f.tellg();if(len<0||static_cast<uint64_t>(len)>limit)return {};std::vector<unsigned char>b(static_cast<size_t>(len));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),b.size());if(!f)return {};return b;}
bool write_atomic(const std::filesystem::path& path,const std::vector<unsigned char>& bytes){auto temp=path;temp+=L".tmp";HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;DWORD written=0;bool ok=WriteFile(f,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size();if(ok)ok=FlushFileBuffers(f)!=0;CloseHandle(f);if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;return ok;}
std::vector<AppEntry> discover_apps(const std::atomic<bool>& exiting){
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);std::vector<AppEntry> apps;std::set<std::wstring> seen;
    for(auto folder:{FOLDERID_Programs,FOLDERID_CommonPrograms}){PWSTR raw=nullptr;if(FAILED(SHGetKnownFolderPath(folder,0,nullptr,&raw)))continue;std::filesystem::path root(raw);CoTaskMemFree(raw);std::error_code ec;
        std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
        while(it!=end&&!exiting&&apps.size()<5000){auto path=it->path();if(lower(path.extension().wstring())==L".lnk"){
            auto name=path.stem().wstring();auto lowered=lower(name);if(lowered.find(L"uninstall")==lowered.npos&&seen.insert(lowered).second)apps.push_back({name,path.wstring(),lowered,false});
        }it.increment(ec);if(ec)ec.clear();}
    }
    if(!exiting){Com<IShellItem> folder;if(SUCCEEDED(SHCreateItemFromParsingName(L"shell:AppsFolder",nullptr,IID_IShellItem,reinterpret_cast<void**>(folder.put())))){
        Com<IEnumShellItems> items;if(SUCCEEDED(folder->BindToHandler(nullptr,BHID_EnumItems,IID_IEnumShellItems,reinterpret_cast<void**>(items.put())))){
            IShellItem* raw=nullptr;while(!exiting&&apps.size()<5000&&items->Next(1,&raw,nullptr)==S_OK){Com<IShellItem> item;item.p=raw;PWSTR name=nullptr,path=nullptr;
                if(SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&SUCCEEDED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,&path))){auto key=lower(name);if(seen.insert(key).second)apps.push_back({name,path,key,true});}if(name)CoTaskMemFree(name);if(path)CoTaskMemFree(path);
            }
        }
    }}
    std::sort(apps.begin(),apps.end(),[](auto& a,auto& b){return a.search<b.search;});if(SUCCEEDED(initialized))CoUninitialize();return apps;
}
bool launch_app(HWND hwnd,const AppEntry& app){
    if(app.packaged){const auto parsingName=app_shell_parsing_name(app.target);PIDLIST_ABSOLUTE pidl=nullptr;if(FAILED(SHParseDisplayName(parsingName.c_str(),nullptr,&pidl,0,nullptr)))return false;SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_IDLIST;info.hwnd=hwnd;info.lpIDList=pidl;info.nShow=SW_SHOWNORMAL;bool ok=ShellExecuteExW(&info);CoTaskMemFree(pidl);return ok;}
    SHELLEXECUTEINFOW info{sizeof(info)};info.hwnd=hwnd;info.lpFile=app.target.c_str();info.nShow=SW_SHOWNORMAL;return ShellExecuteExW(&info)!=0;
}
bool copy_text(HWND hwnd,const std::wstring& text){
    if(!OpenClipboard(hwnd))return false;size_t bytes=(text.size()+1)*sizeof(wchar_t);HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!mem){CloseClipboard();return false;}void* p=GlobalLock(mem);if(!p){GlobalFree(mem);CloseClipboard();return false;}memcpy(p,text.c_str(),bytes);GlobalUnlock(mem);if(!EmptyClipboard()||!SetClipboardData(CF_UNICODETEXT,mem)){GlobalFree(mem);CloseClipboard();return false;}CloseClipboard();return true;
}
bool paste_to(HWND target){if(!target||!IsWindow(target)||GetForegroundWindow()!=target)return false;for(int key:{VK_CONTROL,VK_MENU,VK_SHIFT,VK_LWIN,VK_RWIN})if(GetAsyncKeyState(key)&0x8000)return false;INPUT in[4]{};for(auto& i:in)i.type=INPUT_KEYBOARD;in[0].ki.wVk=VK_CONTROL;in[1].ki.wVk='V';in[2].ki.wVk='V';in[2].ki.dwFlags=KEYEVENTF_KEYUP;in[3].ki.wVk=VK_CONTROL;in[3].ki.dwFlags=KEYEVENTF_KEYUP;return SendInput(4,in,sizeof(INPUT))==4;}
static void append32(std::vector<unsigned char>& out,uint32_t n){for(int i=0;i<4;i++)out.push_back(static_cast<unsigned char>(n>>(8*i)));}
bool save_history(const std::filesystem::path& path,const std::vector<HistoryItem>& items){
    if(items.size()>history_max_items)return false;
    size_t characters=0;
    for(const auto& item:items){
        if(item.text.size()>history_max_item_characters)return false;
        if(item.dib.size()>history_max_image_bytes || (!item.dib.empty() && normalize_clipboard_dib(item.dib.data(),item.dib.size())!=item.dib))return false;
        characters+=item.text.size()+item.dib.size()/sizeof(wchar_t);
        if(characters>history_max_bytes/sizeof(wchar_t))return false;
    }
    std::vector<unsigned char> plain{'M','C','H','2'};append32(plain,static_cast<uint32_t>(items.size()));
    for(auto& item:items){auto s=narrow(item.text);append32(plain,static_cast<uint32_t>(s.size()));append32(plain,static_cast<uint32_t>(item.timestamp));append32(plain,static_cast<uint32_t>(item.timestamp>>32));plain.insert(plain.end(),s.begin(),s.end());append32(plain,static_cast<uint32_t>(item.dib.size()));plain.insert(plain.end(),item.dib.begin(),item.dib.end());}
    DATA_BLOB input{static_cast<DWORD>(plain.size()),plain.data()},output{};
    bool protectedData=CryptProtectData(&input,L"Minicast clipboard",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)!=FALSE;
    SecureZeroMemory(plain.data(),plain.size());if(!protectedData)return false;
    std::vector<unsigned char> encrypted(output.pbData,output.pbData+output.cbData);LocalFree(output.pbData);return write_atomic(path,encrypted);
}
std::vector<HistoryItem> load_history(const std::filesystem::path& path,int retentionDays,bool* pruned){
    if(pruned)*pruned=false;
    auto data=read_bytes(path,256*1024*1024);if(data.empty())return {};DATA_BLOB input{static_cast<DWORD>(data.size()),data.data()},out{};if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out))return {};
    std::vector<HistoryItem> items;size_t at=4;bool version2=out.cbData>=8&&memcmp(out.pbData,"MCH2",4)==0;bool valid=version2||(out.cbData>=8&&memcmp(out.pbData,"MCH1",4)==0);
    auto read=[&]()->uint32_t{if(at+4>out.cbData){valid=false;return 0;}uint32_t n=0;for(int i=0;i<4;i++)n|=uint32_t(out.pbData[at++])<<(8*i);return n;};
    auto count=valid?read():0;if(count>500)valid=false;
    FILETIME now;GetSystemTimeAsFileTime(&now);uint64_t ticks=(uint64_t(now.dwHighDateTime)<<32)|now.dwLowDateTime;
    for(uint32_t i=0;valid&&i<count;i++){auto len=read();uint64_t timestamp=read();timestamp|=uint64_t(read())<<32;if(len>history_max_item_utf8_bytes||at+len>out.cbData){valid=false;break;}auto text=widen(std::string(reinterpret_cast<char*>(out.pbData+at),len));at+=len;if(text.size()>history_max_item_characters){valid=false;break;}std::vector<unsigned char> dib;if(version2){auto imageLen=read();if(!valid||imageLen>history_max_image_bytes||imageLen>out.cbData-at){valid=false;break;}if(imageLen){dib.assign(out.pbData+at,out.pbData+at+imageLen);if(normalize_clipboard_dib(dib.data(),dib.size())!=dib){valid=false;break;}}at+=imageLen;}items.push_back({std::move(text),timestamp,std::move(dib)});}
    SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);if(!valid)return {};const bool changed=prune_history(items,ticks,retentionDays);if(pruned)*pruned=changed;return items;
}
struct Internet {HINTERNET h=nullptr;explicit Internet(HINTERNET x):h(x){}~Internet(){if(h)WinHttpCloseHandle(h);}operator HINTERNET()const{return h;}};
Rate fetch_rate(const std::wstring& from,const std::wstring& to){
    Rate result{from,to};Internet session(WinHttpOpen(L"Minicast/0.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));if(!session){result.error=L"Network unavailable";return result;}WinHttpSetTimeouts(session,4000,4000,4000,5000);
    Internet connection(WinHttpConnect(session,L"api.frankfurter.dev",INTERNET_DEFAULT_HTTPS_PORT,0));if(!connection){result.error=L"Cannot connect to rate provider";return result;}
    auto path=L"/v2/rate/"+lower(from)+L"/"+lower(to);Internet request(WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request||!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr)){result.error=L"Offline — no cached rate for this pair";return result;}
    DWORD status=0,size=sizeof(status);WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);if(status!=200){result.error=L"Currency pair unavailable (HTTP "+std::to_wstring(status)+L")";return result;}
    std::string body;char buf[4096];DWORD got;while(WinHttpReadData(request,buf,sizeof(buf),&got)&&got){body.append(buf,got);if(body.size()>65536){result.error=L"Unexpected rate response";return result;}}
    auto json=parse_json(body);if(json){auto rate=json->get("rate"),date=json->get("date");if(rate&&date&&rate->kind==Json::Number&&date->kind==Json::String){result.value=widen(rate->text);result.date=widen(date->text);}}
    if(result.value.empty()||result.date.empty())result.error=L"Provider returned an unsupported response";return result;
}
RateSnapshot fetch_daily_rates(){
    RateSnapshot result;
    Internet session(WinHttpOpen(L"Minicast/0.2",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session){result.error=L"Offline — using saved exchange rates";return result;}
    WinHttpSetTimeouts(session,4000,4000,4000,6000);
    Internet connection(WinHttpConnect(session,L"api.frankfurter.dev",INTERNET_DEFAULT_HTTPS_PORT,0));
    Internet request(connection?WinHttpOpenRequest(connection,L"GET",L"/v2/rates?base=USD",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr);
    if(!request||!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr)){result.error=L"Offline — using saved exchange rates";return result;}
    DWORD status=0,size=sizeof(status);WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);
    if(status!=200){result.error=L"Rate refresh unavailable — using saved rates";return result;}
    std::string body;char buf[8192];DWORD got=0;bool readOk=true;
    for(;;){if(!WinHttpReadData(request,buf,sizeof(buf),&got)){readOk=false;break;}if(!got)break;body.append(buf,got);if(body.size()>256*1024){readOk=false;break;}}
    auto json=readOk?parse_json(body):std::optional<Json>{};
    if(json&&json->kind==Json::Array){
        for(auto& [key,row]:json->members){
            auto base=row.get("base"),quote=row.get("quote"),rate=row.get("rate"),date=row.get("date");
            if(!base||!quote||!rate||!date||base->kind!=Json::String||quote->kind!=Json::String||rate->kind!=Json::Number||date->kind!=Json::String)continue;
            if(base->text!="USD"||quote->text.size()!=3||date->text.size()!=10)continue;
            bool validCode=std::all_of(quote->text.begin(),quote->text.end(),[](char c){return c>='A'&&c<='Z';});
            auto value=widen(rate->text);if(validCode&&currency_result(L"1",value,6))result.rates.push_back({L"USD",widen(quote->text),value,widen(date->text),{}});
        }
    }
    if(result.rates.size()<10){result.rates.clear();result.error=L"Unexpected rate data — saved rates kept";}
    return result;
}
static std::wstring date_string(const SYSTEMTIME& s){wchar_t b[80];swprintf(b,80,L"%04u-%02u-%02u %02u:%02u",s.wYear,s.wMonth,s.wDay,s.wHour,s.wMinute);return b;}
static std::optional<DYNAMIC_TIME_ZONE_INFORMATION> zone(std::wstring s){
    s=lower(trim(s));if(s==L"local"){DYNAMIC_TIME_ZONE_INFORMATION z{};GetDynamicTimeZoneInformation(&z);return z;}
    const std::pair<const wchar_t*,const wchar_t*> aliases[]={
        {L"jakarta",L"SE Asia Standard Time"},{L"wib",L"SE Asia Standard Time"},{L"bangkok",L"SE Asia Standard Time"},
        {L"london",L"GMT Standard Time"},{L"uk",L"GMT Standard Time"},{L"utc",L"UTC"},{L"gmt",L"UTC"},
        {L"new york",L"Eastern Standard Time"},{L"nyc",L"Eastern Standard Time"},{L"los angeles",L"Pacific Standard Time"},
        {L"la",L"Pacific Standard Time"},{L"tokyo",L"Tokyo Standard Time"},{L"japan",L"Tokyo Standard Time"},
        {L"singapore",L"Singapore Standard Time"},{L"beijing",L"China Standard Time"},{L"hong kong",L"China Standard Time"},
        {L"sydney",L"AUS Eastern Standard Time"},{L"paris",L"Romance Standard Time"},{L"berlin",L"W. Europe Standard Time"},
        {L"dubai",L"Arabian Standard Time"},{L"india",L"India Standard Time"},{L"kolkata",L"India Standard Time"},
        {L"chicago",L"Central Standard Time"},{L"denver",L"Mountain Standard Time"},{L"bali",L"Singapore Standard Time"}};
    for(auto [a,b]:aliases)if(s==a){s=lower(b);break;}
    static const std::vector<DYNAMIC_TIME_ZONE_INFORMATION> zones=[](){std::vector<DYNAMIC_TIME_ZONE_INFORMATION> zs;DYNAMIC_TIME_ZONE_INFORMATION z{};for(DWORD i=0;EnumDynamicTimeZoneInformation(i,&z)==ERROR_SUCCESS;i++)zs.push_back(z);return zs;}();
    for(auto z:zones)if(lower(z.TimeZoneKeyName)==s||lower(z.StandardName)==s)return z;return {};
}
std::optional<Conversion> convert_time(const std::wstring& query){
    auto q=lower(trim(query));if(q.size()>256)return {};
    if(q.starts_with(L"unix ")){auto arg=trim(q.substr(5));wchar_t* end;long long seconds=wcstoll(arg.c_str(),&end,10);if(*end||end==arg.c_str()||seconds < -11644473600LL||seconds>253402300799LL)return Conversion{L"Invalid Unix timestamp",L"Use seconds, not milliseconds"};uint64_t v=static_cast<uint64_t>(seconds+11644473600LL)*10000000;FILETIME ft{static_cast<DWORD>(v),static_cast<DWORD>(v>>32)};SYSTEMTIME st;if(!FileTimeToSystemTime(&ft,&st))return {};return Conversion{date_string(st)+L" UTC",L"Unix seconds → UTC"};}
    if(q.starts_with(L"time ")||q.starts_with(L"now ")){auto name=trim(q.substr(q.find(L' ')+1));if(name.starts_with(L"in "))name=name.substr(3);auto z=zone(name);if(!z)return Conversion{L"Choose a city or time zone",L"Try: time Jakarta · time London · time UTC"};SYSTEMTIME utc,local;GetSystemTime(&utc);SystemTimeToTzSpecificLocalTimeEx(&*z,&utc,&local);return Conversion{date_string(local),name};}
    auto sep=q.find(L" to ");if(sep==q.npos)return {};auto lhs=trim(q.substr(0,sep)),rhs=trim(q.substr(sep+4));auto space=lhs.find(L' ');if(space==lhs.npos)return {};
    auto clock=lhs.substr(0,space),source=trim(lhs.substr(space+1));int days=0;
    if(rhs.ends_with(L" tomorrow")){days=1;rhs=trim(rhs.substr(0,rhs.size()-9));}else if(rhs.ends_with(L" today"))rhs=trim(rhs.substr(0,rhs.size()-6));
    auto src=zone(source),dst=zone(rhs);if(!src||!dst)return {};
    bool pm=clock.ends_with(L"pm"),am=clock.ends_with(L"am");if(pm||am)clock.resize(clock.size()-2);
    int hour=0,minute=0;auto colon=clock.find(L':');auto hs=clock.substr(0,colon),ms=colon==clock.npos?L"0":clock.substr(colon+1);
    if(hs.empty()||ms.empty()||hs.size()>2||ms.size()>2)return {};for(auto c:hs+ms)if(c<L'0'||c>L'9')return {};
    hour=std::stoi(hs);minute=std::stoi(ms);if(minute>59||hour>23||((am||pm)&&(hour<1||hour>12)))return Conversion{L"Invalid time",L"Use 9am or 21:30"};if(am||pm)hour=(hour%12)+(pm?12:0);
    SYSTEMTIME now,local;GetSystemTime(&now);SystemTimeToTzSpecificLocalTimeEx(&*src,&now,&local);local.wHour=0;local.wMinute=0;local.wSecond=0;local.wMilliseconds=0;
    FILETIME ft;SystemTimeToFileTime(&local,&ft);uint64_t ticks=((uint64_t(ft.dwHighDateTime)<<32)|ft.dwLowDateTime)+uint64_t(days)*86400*10000000;ft={static_cast<DWORD>(ticks),static_cast<DWORD>(ticks>>32)};FileTimeToSystemTime(&ft,&local);local.wHour=hour;local.wMinute=minute;
    SYSTEMTIME utc,back,out;if(!TzSpecificLocalTimeToSystemTimeEx(&*src,&local,&utc)||!SystemTimeToTzSpecificLocalTimeEx(&*src,&utc,&back))return {};
    if(back.wHour!=local.wHour||back.wMinute!=local.wMinute||back.wDay!=local.wDay)return Conversion{L"This local time does not exist",L"Daylight-saving transition — choose a different time"};
    // Detect repeated local times by testing the surrounding three hours in UTC.
    FILETIME uf;SystemTimeToFileTime(&utc,&uf);uint64_t base=(uint64_t(uf.dwHighDateTime)<<32)|uf.dwLowDateTime;
    for(int delta=-180;delta<=180;delta+=30){if(!delta)continue;uint64_t v=base+int64_t(delta)*60*10000000;FILETIME test{static_cast<DWORD>(v),static_cast<DWORD>(v>>32)};SYSTEMTIME u,l;FileTimeToSystemTime(&test,&u);SystemTimeToTzSpecificLocalTimeEx(&*src,&u,&l);if(l.wYear==local.wYear&&l.wMonth==local.wMonth&&l.wDay==local.wDay&&l.wHour==local.wHour&&l.wMinute==local.wMinute)return Conversion{L"Ambiguous daylight-saving time",L"This clock time occurs twice; use a UTC source time"};}
    if(!SystemTimeToTzSpecificLocalTimeEx(&*dst,&utc,&out))return {};return Conversion{date_string(out)+L" · "+rhs,date_string(local)+L" · "+source};
}
std::wstring hotkey_name(const Binding& b){
    if(!b.key)return L"Not assigned";
    std::wstring s;
    if(b.modifiers&MOD_CONTROL)s+=L"Ctrl + ";
    if(b.modifiers&MOD_ALT)s+=L"Alt + ";
    if(b.modifiers&MOD_SHIFT)s+=L"Shift + ";
    if(b.modifiers&MOD_WIN)s+=L"Win + ";
    if(b.key==VK_SPACE)s+=L"Space";
    else if(b.key>=VK_F1&&b.key<=VK_F24)s+=L"F"+std::to_wstring(b.key-VK_F1+1);
    else if((b.key>='A'&&b.key<='Z')||(b.key>='0'&&b.key<='9'))s+=wchar_t(b.key);
    else {
        UINT scan=MapVirtualKeyExW(b.key,MAPVK_VK_TO_VSC,GetKeyboardLayout(0));
        // The extended bit distinguishes arrows/Home/Delete from numpad keys.
        bool extended=b.key==VK_LEFT||b.key==VK_RIGHT||b.key==VK_UP||b.key==VK_DOWN||
            b.key==VK_HOME||b.key==VK_END||b.key==VK_PRIOR||b.key==VK_NEXT||
            b.key==VK_INSERT||b.key==VK_DELETE||b.key==VK_DIVIDE||b.key==VK_NUMLOCK;
        wchar_t name[64]{};
        if(scan&&GetKeyNameTextW(LONG((scan<<16)|(extended?1u<<24:0)),name,64)>0)s+=name;
        else s+=L"Key "+std::to_wstring(b.key);
    }
    return s;
}
}
