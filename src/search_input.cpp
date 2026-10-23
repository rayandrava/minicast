#include "search_input.hpp"
#include "search_metrics.hpp"
#include "theme.hpp"
#include <richedit.h>
#include <textserv.h>
#include <imm.h>
#include <dwrite.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include <algorithm>
#include <cstring>

namespace mini {
namespace {
// The bundled MinGW header predates these documented Windows 8 interfaces.
// Keep only their ABI declarations here; implementation lives in Msftedit.dll.
// https://learn.microsoft.com/windows/win32/api/textserv/nl-textserv-itextservices2
struct TextServices2 : ITextServices {
    virtual HRESULT TxGetNaturalSize2(DWORD,HDC,HDC,DVTARGETDEVICE*,DWORD,const SIZEL*,LONG*,LONG*,LONG*)=0;
    virtual HRESULT TxDrawD2D(ID2D1RenderTarget*,LPCRECTL,LPRECT,LONG)=0;
};
struct TextHost2 : ITextHost {
    virtual BOOL TxIsDoubleClickPending()=0;
    virtual HRESULT TxGetWindow(HWND*)=0;
    virtual HRESULT TxSetForegroundWindow()=0;
    virtual HPALETTE TxGetPalette()=0;
    virtual HRESULT TxGetEastAsianFlags(LONG*)=0;
    virtual HCURSOR TxSetCursor2(HCURSOR,BOOL)=0;
    virtual void TxFreeTextServicesNotification()=0;
    virtual HRESULT TxGetEditStyle(DWORD,DWORD*)=0;
    virtual HRESULT TxGetWindowStyles(DWORD*,DWORD*)=0;
    virtual HRESULT TxShowDropCaret(BOOL,HDC,LPCRECT)=0;
    virtual HRESULT TxDestroyCaret()=0;
    virtual HRESULT TxGetHorzExtent(LONG*)=0;
};
struct RichAccessibility : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateProvider(IRawElementProviderWindowlessSite*,IRawElementProviderSimple**)=0;
};
struct RichUiaInformation : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetBoundaryRectangle(UiaRect*)=0;
    virtual HRESULT STDMETHODCALLTYPE IsVisible()=0;
};
struct RichUiaOverrides : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPropertyOverrideValue(PROPERTYID,VARIANT*)=0;
};
constexpr GUID richAccessibilityId{0x983e572d,0x20cd,0x460b,{0x91,0x04,0x83,0x11,0x15,0x92,0xdd,0x10}};
constexpr GUID richUiaInformationId{0x23969a9d,0x8546,0x4032,{0xa1,0xbb,0x73,0x75,0x0c,0xbf,0x33,0x33}};
constexpr DWORD textD2D=0x01000000;
// Msftedit's DPI message, also used by Microsoft's windowless TextInput host.
// Set it on initial layout as well as monitor changes; HWND metrics alone do
// not initialize the text engine's D2D scaling.
// https://github.com/microsoft/react-native-windows/pull/16288
constexpr UINT setTextDpi=WM_USER+328; // EM_SETDPI
constexpr UINT_PTR caretTimer=0x7fff;
constexpr wchar_t inputClass[]=L"MinicastSearchInput";
// Stable UIA IDs; the host needs no UI Automation client interfaces.
constexpr PROPERTYID uiaControlType=30003,uiaName=30005,uiaControlElement=30016,uiaContentElement=30017;
constexpr LONG uiaPane=50033;

struct RichLibrary {
    HMODULE module=LoadLibraryExW(L"msftedit.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    PCreateTextServices create=module?reinterpret_cast<PCreateTextServices>(GetProcAddress(module,"CreateTextServices")):nullptr;
    using Shutdown=HRESULT(WINAPI*)(IUnknown*);
    Shutdown shutdown=module?reinterpret_cast<Shutdown>(GetProcAddress(module,"ShutdownTextServices")):nullptr;
    const IID* host=module?reinterpret_cast<const IID*>(GetProcAddress(module,"IID_ITextHost")):nullptr;
    const IID* host2=module?reinterpret_cast<const IID*>(GetProcAddress(module,"IID_ITextHost2")):nullptr;
    const IID* services2=module?reinterpret_cast<const IID*>(GetProcAddress(module,"IID_ITextServices2")):nullptr;
    const IID* overrides=module?reinterpret_cast<const IID*>(GetProcAddress(module,"IID_IRicheditUiaOverrides")):nullptr;
};
RichLibrary& rich_library(){static RichLibrary library;return library;}
struct AutomationLibrary {
    // Accessibility stays available without loading UIA's infrastructure on
    // every launcher start. The first screen-reader request loads it once.
    HMODULE module=LoadLibraryExW(L"uiautomationcore.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    using ReturnProvider=LRESULT(WINAPI*)(HWND,WPARAM,LPARAM,IRawElementProviderSimple*);
    using HostProvider=HRESULT(WINAPI*)(HWND,IRawElementProviderSimple**);
    ReturnProvider returnProvider=module?reinterpret_cast<ReturnProvider>(GetProcAddress(module,"UiaReturnRawElementProvider")):nullptr;
    HostProvider hostProvider=module?reinterpret_cast<HostProvider>(GetProcAddress(module,"UiaHostProviderFromHwnd")):nullptr;
};
AutomationLibrary& automation_library(){static AutomationLibrary library;return library;}

struct SearchHost final : TextHost2, IRawElementProviderSimple, IRawElementProviderFragment,
    IRawElementProviderFragmentRoot, IRawElementProviderWindowlessSite, RichUiaInformation, RichUiaOverrides {
    LONG refs=1;
    HWND window=nullptr;
    HFONT font=nullptr;
    Com<TextServices2> services;
    Com<IRawElementProviderSimple> provider;
    Com<IDWriteFactory> writeFactory;
    Com<IDWriteTextFormat> placeholderFormat;
    CHARFORMAT2W character{};
    PARAFORMAT paragraph{};
    bool light=true,focused=false,caretVisible=false,caretOn=true,caretEmpty=false,drawing=false;
    RECT caret{0,0,1,19};
    DWORD maxLength=2048;
    int textPixels=19;
    SearchInputScale inputScale{96};

    SearchHost(HWND hwnd):window(hwnd){
        character.cbSize=sizeof(character);character.dwMask=CFM_COLOR|CFM_FACE|CFM_SIZE|CFM_CHARSET;
        character.yHeight=textPixels*15;character.bCharSet=DEFAULT_CHARSET;character.crTextColor=RGB(19,20,23);
        wcscpy_s(character.szFaceName,L"Segoe UI");
        paragraph.cbSize=sizeof(paragraph);paragraph.dwMask=PFM_ALIGNMENT;paragraph.wAlignment=PFA_LEFT;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;auto& lib=rich_library();
        if(iid==IID_IUnknown||(lib.host&&iid==*lib.host)||(lib.host2&&iid==*lib.host2))*out=static_cast<TextHost2*>(this);
        else if(iid==__uuidof(IRawElementProviderSimple))*out=static_cast<IRawElementProviderSimple*>(this);
        else if(iid==__uuidof(IRawElementProviderFragment))*out=static_cast<IRawElementProviderFragment*>(this);
        else if(iid==__uuidof(IRawElementProviderFragmentRoot))*out=static_cast<IRawElementProviderFragmentRoot*>(this);
        else if(iid==__uuidof(IRawElementProviderWindowlessSite))*out=static_cast<IRawElementProviderWindowlessSite*>(this);
        else if(iid==richUiaInformationId)*out=static_cast<RichUiaInformation*>(this);
        else if(lib.overrides&&iid==*lib.overrides)*out=static_cast<RichUiaOverrides*>(this);
        if(!*out)return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release() override{auto left=InterlockedDecrement(&refs);if(!left)delete this;return left;}
    RECT client() const {RECT r{};if(window)GetClientRect(window,&r);return r;}
    void invalidate(){if(!window||drawing)return;RECT r{};GetWindowRect(window,&r);MapWindowPoints(nullptr,GetParent(window),reinterpret_cast<POINT*>(&r),2);InvalidateRect(GetParent(window),&r,FALSE);}
    LRESULT send(UINT msg,WPARAM wp=0,LPARAM lp=0){LRESULT result=0;if(services)services->TxSendMessage(msg,wp,lp,&result);return result;}
    bool initialize(){
        auto& lib=rich_library();if(!lib.create||!lib.services2)return false;
        Com<IUnknown> unknown;
        if(FAILED(lib.create(nullptr,this,unknown.put()))||!unknown)return false;
        if(FAILED(unknown->QueryInterface(*lib.services2,reinterpret_cast<void**>(services.put())))){
            if(lib.shutdown){auto failed=unknown.p;unknown.p=nullptr;lib.shutdown(failed);}
            return false;
        }
        send(EM_SETTEXTMODE,TM_PLAINTEXT|TM_MULTILEVELUNDO);
        send(EM_SETEVENTMASK,0,ENM_CHANGE|ENM_SELCHANGE);
        send(EM_SETEDITSTYLE,SES_LOGICALCARET|SES_USECTF,SES_LOGICALCARET|SES_USECTF);
        send(EM_EXLIMITTEXT,0,maxLength);
        sync_dpi();
        RECT r=client();services->OnTxInPlaceActivate(&r);return true;
    }
    void disconnect(){
        KillTimer(window,caretTimer);focused=false;
        if(services){services->OnTxUIDeactivate();services->OnTxInPlaceDeactivate();}
        provider.reset();
        // Text services retain a non-owning host pointer. Shutdown disconnects
        // it for outstanding UIA clients AND consumes our service reference.
        // Detach it first so Com cannot release the same reference twice.
        if(services&&rich_library().shutdown){auto detached=services.p;services.p=nullptr;rich_library().shutdown(detached);}
        services.reset();window=nullptr;
    }
    void sync_dpi(){
        SearchInputScale current{window?GetDpiForWindow(window):96};
        if(inputScale.dpi!=current.dpi)placeholderFormat.reset();
        inputScale=current;
        send(setTextDpi,inputScale.dpi,inputScale.dpi);
    }
    void apply_format(){
        if(!services)return;
        services->OnTxPropertyBitsChange(TXTBIT_CHARFORMATCHANGE,TXTBIT_CHARFORMATCHANGE);
        // Update both existing text and the default used after clearing/pasting.
        send(EM_SETCHARFORMAT,0,reinterpret_cast<LPARAM>(&character));
        send(EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&character));
    }
    void update_font(HFONT value){
        font=value;LOGFONTW lf{};if(!value||!GetObjectW(value,sizeof(lf),&lf))return;
        sync_dpi();
        textPixels=static_cast<int>(std::max<LONG>(1,std::abs(lf.lfHeight)));
        // LOGFONT already includes monitor scaling. Undo it when converting
        // physical font pixels to twips; RichEdit applies its DPI exactly once.
        character.yHeight=inputScale.font_twips(textPixels);character.dwMask|=CFM_WEIGHT;character.wWeight=static_cast<WORD>(lf.lfWeight);
        wcsncpy_s(character.szFaceName,lf.lfFaceName,_TRUNCATE);placeholderFormat.reset();
        apply_format();invalidate();
    }
    void theme(bool value){if(light==value)return;light=value;character.crTextColor=light?RGB(19,20,23):RGB(239,239,242);
        apply_format();invalidate();}
    void blink(){caretOn=true;KillTimer(window,caretTimer);UINT interval=GetCaretBlinkTime();
        if(focused&&caretVisible&&IsWindowVisible(window)&&interval&&interval!=INFINITE)SetTimer(window,caretTimer,interval,nullptr);invalidate();}
    void draw(ID2D1RenderTarget* target,const wchar_t* placeholder){
        if(!services||!target||!IsWindowVisible(window))return;
        RECT r=client();POINT origin{};MapWindowPoints(window,GetParent(window),&origin,1);
        D2D1_MATRIX_3X2_F previous{};target->GetTransform(&previous);auto aa=target->GetTextAntialiasMode();
        float previousDpiX=96,previousDpiY=96;target->GetDpi(&previousDpiX,&previousDpiY);
        const auto translation=D2D1::Matrix3x2F::Translation(inputScale.dips(float(origin.x)),inputScale.dips(float(origin.y)));
        target->SetDpi(float(inputScale.dpi),float(inputScale.dpi));
        target->SetTransform(translation);
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        target->PushAxisAlignedClip(D2D1::RectF(0,0,inputScale.dips(float(r.right)),inputScale.dips(float(r.bottom))),D2D1_ANTIALIAS_MODE_ALIASED);
        // RichEdit takes physical client bounds, even though ordinary D2D
        // drawing uses DIPs. Match the target DPI to its native layout DPI.
        drawing=true;RECTL bounds{0,0,r.right,r.bottom};services->TxDrawD2D(target,&bounds,nullptr,TXTVIEW_ACTIVE);drawing=false;
        target->SetTransform(translation);
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        Com<ID2D1SolidColorBrush> brush;
        if(SUCCEEDED(target->CreateSolidColorBrush(light?D2D1::ColorF(0x131417):D2D1::ColorF(0xefeff2),brush.put()))){
            if(!send(WM_GETTEXTLENGTH)&&placeholder&&*placeholder){
                if(!writeFactory)DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(writeFactory.put()));
                if(writeFactory&&!placeholderFormat){writeFactory->CreateTextFormat(character.szFaceName,nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,inputScale.dips(float(textPixels)),L"",placeholderFormat.put());if(placeholderFormat)placeholderFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);}
                brush->SetColor(light?D2D1::ColorF(0x93979d):D2D1::ColorF(0x88888f));
                if(placeholderFormat)target->DrawText(placeholder,static_cast<UINT32>(wcslen(placeholder)),placeholderFormat.p,D2D1::RectF(0,0,inputScale.dips(float(r.right)),inputScale.dips(float(r.bottom))),brush.p,D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
            if(focused&&caretVisible&&caretOn&&!caretEmpty){brush->SetColor(light?D2D1::ColorF(0x131417):D2D1::ColorF(0xefeff2));target->FillRectangle(D2D1::RectF(inputScale.dips(float(caret.left)),inputScale.dips(float(caret.top)),inputScale.dips(float(caret.right)),inputScale.dips(float(caret.bottom))),brush.p);}
        }
        target->PopAxisAlignedClip();target->SetDpi(previousDpiX,previousDpiY);target->SetTransform(previous);target->SetTextAntialiasMode(aa);
    }

    HDC TxGetDC() override{return window?GetDC(window):nullptr;}
    INT TxReleaseDC(HDC dc) override{return window?ReleaseDC(window,dc):0;}
    BOOL TxShowScrollBar(INT,BOOL) override{return FALSE;}
    BOOL TxEnableScrollBar(INT,INT) override{return FALSE;}
    BOOL TxSetScrollRange(INT,LONG,INT,BOOL) override{return FALSE;}
    BOOL TxSetScrollPos(INT,INT,BOOL) override{return FALSE;}
    void TxInvalidateRect(LPCRECT,BOOL) override{invalidate();}
    void TxViewChange(BOOL) override{invalidate();}
    BOOL TxCreateCaret(HBITMAP flags,INT width,INT height) override{
        caretEmpty=(reinterpret_cast<ULONG_PTR>(flags)&64)!=0;caret.right=caret.left+std::max(1,width);caret.bottom=caret.top+std::max(1,height);
        // Keep a hidden system caret for IME/accessibility positioning; draw the
        // visible caret in D2D so GDI never punches holes into the Acrylic.
        return window?CreateCaret(window,nullptr,std::max(1,width),std::max(1,height)):FALSE;
    }
    BOOL TxShowCaret(BOOL show) override{caretVisible=show!=FALSE;blink();return TRUE;}
    BOOL TxSetCaretPos(INT x,INT y) override{int width=caret.right-caret.left,height=caret.bottom-caret.top;caret={x,y,x+width,y+height};if(focused)::SetCaretPos(x,y);blink();return TRUE;}
    BOOL TxSetTimer(UINT id,UINT timeout) override{return window&&SetTimer(window,id,timeout,nullptr)!=0;}
    void TxKillTimer(UINT id) override{if(window)KillTimer(window,id);}
    void TxScrollWindowEx(INT,INT,LPCRECT,LPCRECT,HRGN,LPRECT,UINT) override{invalidate();}
    void TxSetCapture(BOOL capture) override{if(capture&&window)::SetCapture(window);else if(GetCapture()==window)ReleaseCapture();}
    void TxSetFocus() override{if(window)::SetFocus(window);}
    void TxSetCursor(HCURSOR cursor,BOOL) override{::SetCursor(cursor);}
    BOOL TxScreenToClient(LPPOINT point) override{return window&&ScreenToClient(window,point);}
    BOOL TxClientToScreen(LPPOINT point) override{return window&&ClientToScreen(window,point);}
    HRESULT TxActivate(LONG* old) override{if(old)*old=0;return S_OK;}
    HRESULT TxDeactivate(LONG) override{return S_OK;}
    HRESULT TxGetClientRect(LPRECT rect) override{if(!rect)return E_POINTER;*rect=client();return S_OK;}
    HRESULT TxGetViewInset(LPRECT rect) override{if(!rect)return E_POINTER;*rect={};return S_OK;}
    HRESULT TxGetCharFormat(const CHARFORMATW** format) override{if(!format)return E_POINTER;*format=reinterpret_cast<const CHARFORMATW*>(&character);return S_OK;}
    HRESULT TxGetParaFormat(const PARAFORMAT** format) override{if(!format)return E_POINTER;*format=&paragraph;return S_OK;}
    COLORREF TxGetSysColor(int index) override{if(index==COLOR_WINDOWTEXT)return character.crTextColor;if(index==COLOR_WINDOW)return palette_background(light);return GetSysColor(index);}
    HRESULT TxGetBackStyle(TXTBACKSTYLE* style) override{if(!style)return E_POINTER;*style=TXTBACK_TRANSPARENT;return S_OK;}
    HRESULT TxGetMaxLength(DWORD* length) override{if(!length)return E_POINTER;*length=maxLength;return S_OK;}
    HRESULT TxGetScrollBars(DWORD* bars) override{if(!bars)return E_POINTER;*bars=ES_AUTOHSCROLL;return S_OK;}
    HRESULT TxGetPasswordChar(TCHAR* ch) override{if(!ch)return E_POINTER;*ch=0;return S_OK;}
    HRESULT TxGetAcceleratorPos(LONG* pos) override{if(!pos)return E_POINTER;*pos=-1;return S_OK;}
    // No document/print extent: this is a single-line UI field. Supplying a
    // synthetic 96-DPI extent here introduces RichEdit's implicit zoom.
    HRESULT TxGetExtent(LPSIZEL size) override{if(!size)return E_POINTER;*size={};return E_NOTIMPL;}
    HRESULT OnTxCharFormatChange(const CHARFORMATW*) override{return S_OK;}
    HRESULT OnTxParaFormatChange(const PARAFORMAT*) override{return S_OK;}
    HRESULT TxGetPropertyBits(DWORD mask,DWORD* bits) override{if(!bits)return E_POINTER;*bits=mask&(textD2D|TXTBIT_SAVESELECTION|TXTBIT_DISABLEDRAG);return S_OK;}
    HRESULT TxNotify(DWORD code,void*) override{
        if(!window)return S_OK;
        if(code==EN_CHANGE)SendMessageW(GetParent(window),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(window),EN_CHANGE),reinterpret_cast<LPARAM>(window));
        if(code==EN_SELCHANGE)blink();invalidate();return S_OK;
    }
    HIMC TxImmGetContext() override{return window?ImmGetContext(window):nullptr;}
    void TxImmReleaseContext(HIMC context) override{if(window)ImmReleaseContext(window,context);}
    HRESULT TxGetSelectionBarWidth(LONG* width) override{if(!width)return E_POINTER;*width=0;return S_OK;}
    BOOL TxIsDoubleClickPending() override{MSG msg{};return window&&PeekMessageW(&msg,window,WM_LBUTTONDBLCLK,WM_LBUTTONDBLCLK,PM_NOREMOVE);}
    HRESULT TxGetWindow(HWND* out) override{if(!out)return E_POINTER;*out=window;return window?S_OK:E_FAIL;}
    HRESULT TxSetForegroundWindow() override{return window&&::SetForegroundWindow(GetAncestor(window,GA_ROOT))?S_OK:S_FALSE;}
    HPALETTE TxGetPalette() override{return nullptr;}
    HRESULT TxGetEastAsianFlags(LONG* flags) override{if(!flags)return E_POINTER;*flags=0;return S_OK;}
    HCURSOR TxSetCursor2(HCURSOR cursor,BOOL) override{return ::SetCursor(cursor);}
    void TxFreeTextServicesNotification() override{}
    HRESULT TxGetEditStyle(DWORD,DWORD* style) override{if(!style)return E_POINTER;*style=0;return S_OK;}
    HRESULT TxGetWindowStyles(DWORD* style,DWORD* ex) override{if(style)*style=window?static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)):0;if(ex)*ex=window?static_cast<DWORD>(GetWindowLongPtrW(window,GWL_EXSTYLE)):0;return S_OK;}
    HRESULT TxShowDropCaret(BOOL,HDC,LPCRECT) override{return S_OK;}
    HRESULT TxDestroyCaret() override{caretVisible=false;KillTimer(window,caretTimer);if(focused)DestroyCaret();invalidate();return S_OK;}
    HRESULT TxGetHorzExtent(LONG* extent) override{if(!extent)return E_POINTER;*extent=client().right;return S_OK;}

    // The host is an HWND-backed UIA fragment root. Its single child is the
    // system RichEdit provider, which supplies native Text and Value patterns.
    bool ensure_provider(){
        if(provider)return true;if(!services)return false;Com<RichAccessibility> access;
        return SUCCEEDED(services->QueryInterface(richAccessibilityId,reinterpret_cast<void**>(access.put())))&&
            SUCCEEDED(access->CreateProvider(static_cast<IRawElementProviderWindowlessSite*>(this),provider.put()));
    }
    HRESULT child_fragment(IRawElementProviderFragment** out){if(!out)return E_POINTER;*out=nullptr;return ensure_provider()?provider->QueryInterface(__uuidof(IRawElementProviderFragment),reinterpret_cast<void**>(out)):S_OK;}
    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* out) override{if(!out)return E_POINTER;*out=static_cast<ProviderOptions>(ProviderOptions_ServerSideProvider|ProviderOptions_UseComThreading);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID,IUnknown** out) override{if(!out)return E_POINTER;*out=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID id,VARIANT* out) override{
        if(!out)return E_POINTER;VariantInit(out);
        if(id==uiaControlType){out->vt=VT_I4;out->lVal=uiaPane;}
        else if(id==uiaControlElement||id==uiaContentElement){out->vt=VT_BOOL;out->boolVal=VARIANT_FALSE;}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** out) override{if(!out)return E_POINTER;*out=nullptr;if(!window)return UIA_E_ELEMENTNOTAVAILABLE;auto get=automation_library().hostProvider;return get?get(window,out):E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,IRawElementProviderFragment** out) override{if(!out)return E_POINTER;*out=nullptr;if(direction==NavigateDirection_FirstChild||direction==NavigateDirection_LastChild)return child_fragment(out);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** out) override{if(!out)return E_POINTER;*out=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* out) override{return GetBoundaryRectangle(out);}
    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** out) override{if(!out)return E_POINTER;*out=nullptr;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetFocus() override{if(!window)return UIA_E_ELEMENTNOTAVAILABLE;::SetFocus(window);return S_OK;}
    HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** out) override{if(!out)return E_POINTER;*out=this;AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x,double y,IRawElementProviderFragment** out) override{if(!out)return E_POINTER;*out=nullptr;RECT r{};if(window&&GetWindowRect(window,&r)&&x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)return child_fragment(out);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** out) override{if(!out)return E_POINTER;*out=nullptr;return window&&::GetFocus()==window?child_fragment(out):S_OK;}
    HRESULT STDMETHODCALLTYPE GetAdjacentFragment(NavigateDirection direction,IRawElementProviderFragment** out) override{if(!out)return E_POINTER;*out=nullptr;if(direction==NavigateDirection_Parent){*out=static_cast<IRawElementProviderFragment*>(this);AddRef();}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRuntimeIdPrefix(SAFEARRAY** out) override{if(!out)return E_POINTER;*out=SafeArrayCreateVector(VT_I4,0,2);if(!*out)return E_OUTOFMEMORY;LONG* values=nullptr;if(SUCCEEDED(SafeArrayAccessData(*out,reinterpret_cast<void**>(&values)))){values[0]=UiaAppendRuntimeId;values[1]=window?GetDlgCtrlID(window):0;SafeArrayUnaccessData(*out);}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetBoundaryRectangle(UiaRect* out) override{if(!out)return E_POINTER;RECT r{};if(!window||!GetWindowRect(window,&r)){*out={};return UIA_E_ELEMENTNOTAVAILABLE;}*out={static_cast<double>(r.left),static_cast<double>(r.top),static_cast<double>(r.right-r.left),static_cast<double>(r.bottom-r.top)};return S_OK;}
    HRESULT STDMETHODCALLTYPE IsVisible() override{return window&&IsWindowVisible(window)?S_OK:S_FALSE;}
    HRESULT STDMETHODCALLTYPE GetPropertyOverrideValue(PROPERTYID id,VARIANT* out) override{if(!out)return E_POINTER;VariantInit(out);if(id==uiaName){out->vt=VT_BSTR;out->bstrVal=SysAllocString(L"Search");return out->bstrVal?S_OK:E_OUTOFMEMORY;}return S_FALSE;}
};

SearchHost* host(HWND window){return reinterpret_cast<SearchHost*>(GetWindowLongPtrW(window,GWLP_USERDATA));}
LRESULT CALLBACK search_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    auto input=host(hwnd);
    if(msg==WM_NCCREATE){input=new SearchHost(hwnd);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(input));return TRUE;}
    if(!input)return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg){
    case WM_CREATE: return input->initialize()?0:-1;
    case WM_DESTROY:if(input->provider){auto clear=automation_library().returnProvider;if(clear)clear(hwnd,0,0,nullptr);}break;
    case WM_NCDESTROY: input->disconnect();SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);input->Release();return DefWindowProcW(hwnd,msg,wp,lp);
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT paint{};BeginPaint(hwnd,&paint);EndPaint(hwnd,&paint);return 0;}
    case WM_SETFONT:input->update_font(reinterpret_cast<HFONT>(wp));return 0;
    case WM_GETFONT:return reinterpret_cast<LRESULT>(input->font);
    case WM_SIZE:input->sync_dpi();if(input->services)input->services->OnTxPropertyBitsChange(TXTBIT_CLIENTRECTCHANGE,TXTBIT_CLIENTRECTCHANGE);input->invalidate();return 0;
    case WM_SETCURSOR: ::SetCursor(LoadCursorW(nullptr,IDC_IBEAM));return TRUE;
    case WM_SETFOCUS:input->focused=true;if(input->services)input->services->OnTxUIActivate();input->send(msg,wp,lp);input->blink();return 0;
    case WM_KILLFOCUS:input->send(msg,wp,lp);input->focused=false;input->caretVisible=false;KillTimer(hwnd,caretTimer);if(input->services)input->services->OnTxUIDeactivate();DestroyCaret();input->invalidate();return 0;
    case WM_SHOWWINDOW:if(!wp)KillTimer(hwnd,caretTimer);else input->blink();break;
    case WM_TIMER:if(wp==caretTimer){if(!IsWindowVisible(hwnd)||!input->focused){KillTimer(hwnd,caretTimer);return 0;}input->caretOn=!input->caretOn;input->invalidate();return 0;}return input->send(msg,wp,lp);
    case WM_GETDLGCODE:return DLGC_WANTCHARS|DLGC_WANTARROWS|DLGC_HASSETSEL;
    case WM_GETOBJECT:if(static_cast<LONG>(lp)==UiaRootObjectId&&input->ensure_provider()){auto send=automation_library().returnProvider;if(send)return send(hwnd,wp,lp,static_cast<IRawElementProviderSimple*>(input));}break;
    case EM_SETLIMITTEXT:input->maxLength=wp?static_cast<DWORD>(std::min<WPARAM>(wp,65536)):2048;if(input->services)input->services->OnTxPropertyBitsChange(TXTBIT_MAXLENGTHCHANGE,TXTBIT_MAXLENGTHCHANGE);return input->send(EM_EXLIMITTEXT,0,input->maxLength);
    case EM_SETMARGINS:return 0; // transparent search aligns directly to its bounds
    case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK: ::SetFocus(hwnd);return input->send(msg,wp,lp);
    }
    if(input->services){
        // Preserve standard edit operations and native mouse/IME/TSF messages.
        // Unhandled window-management messages still go to DefWindowProc.
        LRESULT result=0;HRESULT hr=input->services->TxSendMessage(msg,wp,lp,&result);
        if(hr==S_OK)return result;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}

HWND create_search_input(HWND parent,int id,HINSTANCE instance){
    WNDCLASSW cls{};cls.lpfnWndProc=search_proc;cls.hInstance=instance;cls.lpszClassName=inputClass;cls.hCursor=LoadCursorW(nullptr,IDC_IBEAM);cls.style=CS_DBLCLKS;
    RegisterClassW(&cls);
    auto window=CreateWindowExW(WS_EX_TRANSPARENT,inputClass,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,49,20,640,29,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
    if(!window)window=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,49,20,640,29,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
    return window;
}
bool search_input_is_composited(HWND hwnd){wchar_t name[64]{};return hwnd&&GetClassNameW(hwnd,name,64)&&wcscmp(name,inputClass)==0;}
void set_search_input_theme(HWND hwnd,bool light){if(search_input_is_composited(hwnd))host(hwnd)->theme(light);}
void paint_search_input(HWND hwnd,ID2D1RenderTarget* target,const wchar_t* placeholder){if(search_input_is_composited(hwnd))host(hwnd)->draw(target,placeholder);}
}
