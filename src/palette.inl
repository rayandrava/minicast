// Palette geometry is in device-independent pixels. Windows supplies the acrylic
// backdrop; the theme only controls tint and contrast, never a wallpaper image.
D2D1_COLOR_F ink() { return light?color(19,20,23):color(239,239,242); }
D2D1_COLOR_F muted() { return light?color(100,103,108):color(161,162,170); }
D2D1_COLOR_F faint() { return light?color(148,151,155):color(117,118,128); }
D2D1_COLOR_F rule() { return light?color(0,0,0,.12f):color(255,255,255,.10f); }
D2D1_COLOR_F selection() { return light?color(30,36,45,.09f):color(255,255,255,.105f); }
D2D1_COLOR_F tile_fill() { return light?color(60,72,92,.065f):color(255,255,255,.045f); }
COLORREF edit_background() { return palette_background(light); }
void refresh_theme(bool force=false) {
    bool current=system_uses_light_theme();if(!force&&light==current)return;light=current;
    if(editBrush)DeleteObject(editBrush);editBrush=CreateSolidBrush(edit_background());
    if(edit)set_search_input_theme(edit,light);
    surface.set_theme(hwnd,light);if(settings)settings_theme();
    if(edit)InvalidateRect(edit,nullptr,TRUE);invalidate();
}
void outline(float x,float y,float w,float h,D2D1_COLOR_F tint,float radius=4,float stroke=1) {
    brush->SetColor(tint);target->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+h),radius,radius),brush.p,stroke);
}
void aligned(const std::wstring& value,float x,float y,float w,float h,IDWriteTextFormat* font,D2D1_COLOR_F tint,DWRITE_TEXT_ALIGNMENT alignment) {
    font->SetTextAlignment(alignment);text(value,x,y,w,h,font,tint);font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
}
void keycap(const std::wstring& label,float x,float y,float width) {
    outline(x,y,width,18,rule(),3);
    aligned(label,x,y+1,width,17,small.p,muted(),DWRITE_TEXT_ALIGNMENT_CENTER);
}
bool emoji_view() const {return resultMode==Mode::Emoji;}
bool clipboard_view() const {return resultMode==Mode::Clipboard;}
float list_top() const {return conversionCard?261.f:float(ROW_TOP);}
bool list_group_header(int index) const {
    int first=conversionCard?1:0;
    return !conversionCard&&!rows.empty()&&(resultMode==Mode::Home||resultMode==Mode::Apps)&&!rows.front().group.empty()&&
        (index==first||rows[index].group!=rows[index-1].group);
}
void build_list_layout(){
    listPositions.assign(rows.size(),0.f);listHeight=0;
    for(int i=conversionCard?1:0;i<int(rows.size());++i){
        if(list_group_header(i))listHeight+=26;
        listPositions[i]=listHeight;listHeight+=ROW_HEIGHT;
    }
}
float list_row_offset(int index) const {return index>=0&&index<int(listPositions.size())?listPositions[index]:0.f;}
float list_content_height() const {return listHeight;}
ListLayout visible_list(float offset) const {
    ListLayout result;int first=conversionCard?1:0;
    if(int(listPositions.size())<=first)return result;
    auto begin=std::upper_bound(listPositions.begin()+first,listPositions.end(),offset-ROW_HEIGHT);
    for(int index=int(begin-listPositions.begin());index<int(rows.size());++index){
        float y=list_top()+listPositions[index]-offset;bool header=list_group_header(index);
        if(y-(header?26:0)>=FOOTER_TOP)break;
        if(result.count<int(result.items.size()))result.items[result.count++]={index,y,header};
    }return result;
}
int list_capacity() const {return std::max(1,int((FOOTER_TOP-list_top())/ROW_HEIGHT));}
float maximum_list_scroll() const {return std::max(0.f,list_content_height()-(FOOTER_TOP-list_top()));}
void schedule_scroll_frame(){
    // One-shot timer, active only during scrolling. Presentation retains VSync.
    LARGE_INTEGER due{};due.QuadPart=-10000;
    if(scrollTimer&&SetWaitableTimer(scrollTimer,&due,0,nullptr,nullptr,FALSE))return;
    if(scrollTimer){CloseHandle(scrollTimer);scrollTimer=nullptr;}
    SetTimer(hwnd,TIMER_SCROLL,16,nullptr);
}
void stop_scroll_animation(){if(scrollTimer)CancelWaitableTimer(scrollTimer);KillTimer(hwnd,TIMER_SCROLL);scrollAnimating=false;}
void animate_scroll(){
    auto now=std::chrono::steady_clock::now();float elapsed=std::chrono::duration<float,std::milli>(now-scrollTick).count();scrollTick=now;
    float& offset=emoji_view()?gridOffset:scroll;
    float maximum=emoji_view()?std::max(0.f,gridHeight-FOOTER_TOP):maximum_list_scroll();
    scrollTarget=std::clamp(scrollTarget,0.f,maximum);
    offset=smooth_scroll_step(offset,scrollTarget,elapsed);
    if(offset==scrollTarget)stop_scroll_animation();
    hover=-1;invalidate();
}
void scroll_to(float value,bool animate){
    float maximum=emoji_view()?std::max(0.f,gridHeight-FOOTER_TOP):maximum_list_scroll();
    scrollTarget=std::clamp(value,0.f,maximum);
    viewportScrolled=true;hover=-1;
    if(animate){if(!scrollAnimating){scrollTick=std::chrono::steady_clock::now();scrollAnimating=true;schedule_scroll_frame();}}
    else {stop_scroll_animation();if(emoji_view())gridOffset=scrollTarget;else scroll=scrollTarget;}
    invalidate();
}
struct ScrollbarGeometry {float x=0,top=0,height=0,thumbTop=0,thumbHeight=0,maximum=0,offset=0;};
ScrollbarGeometry scrollbar_geometry() const {
    ScrollbarGeometry bar;
    if(mode==Mode::Camera)return bar;
    bar.x=(clipboard_view()?283.f:750.f)-11;
    bar.top=emoji_view()?71.f:list_top()+3;
    bar.height=FOOTER_TOP-5-bar.top;
    float fraction=1;
    if(emoji_view()){
        bar.maximum=std::max(0.f,gridHeight-FOOTER_TOP);bar.offset=gridOffset;
        fraction=(FOOTER_TOP-66.f)/std::max(1.f,gridHeight-66);
    }else{
        bar.maximum=maximum_list_scroll();bar.offset=scroll;
        fraction=(FOOTER_TOP-list_top())/std::max(1.f,list_content_height());
    }
    if(bar.maximum<=0)return bar;
    bar.thumbHeight=std::clamp(bar.height*fraction,24.f,bar.height);
    bar.thumbTop=bar.top+(bar.height-bar.thumbHeight)*std::clamp(bar.offset/bar.maximum,0.f,1.f);
    return bar;
}
bool scrollbar_at(float x,float y) const {
    auto bar=scrollbar_geometry();return !popup&&bar.maximum>0&&x>=bar.x-3&&x<bar.x+10&&y>=bar.top&&y<bar.top+bar.height;
}
void paint_scrollbar() {
    auto bar=scrollbar_geometry();if(bar.maximum<=0)return;
    rect(bar.x,bar.top,6,bar.height,light?color(0,0,0,.05f):color(255,255,255,.05f),3);
    rect(bar.x,bar.thumbTop,6,bar.thumbHeight,light?color(0,0,0,scrollbarDragging?.48f:.30f):color(255,255,255,scrollbarDragging?.55f:.35f),3);
}
void set_scrollbar_offset(float value) {
    scroll_to(value,false);
}
void drag_scrollbar(float y) {
    auto bar=scrollbar_geometry();if(bar.maximum<=0||bar.height<=bar.thumbHeight)return;
    set_scrollbar_offset((y-scrollbarGrab-bar.top)/(bar.height-bar.thumbHeight)*bar.maximum);
}
bool begin_scrollbar(float x,float y) {
    if(!scrollbar_at(x,y))return false;
    auto bar=scrollbar_geometry();
    if(y>=bar.thumbTop&&y<bar.thumbTop+bar.thumbHeight){
        stop_scroll_animation();scrollbarGrab=y-bar.thumbTop;scrollbarDragging=true;SetCapture(hwnd);invalidate();
    }else{
        float page=FOOTER_TOP-(emoji_view()?66.f:list_top());
        scroll_to((scrollAnimating?scrollTarget:bar.offset)+(y<bar.thumbTop?-page:page),true);
    }
    return true;
}
void position_edit() {
    if(!edit)return;
    int width=(emoji_view()||clipboard_view())?474:640;
    if(width==editLayoutWidth&&dpi==editLayoutDpi)return;
    editLayoutWidth=width;editLayoutDpi=dpi;
    SetWindowPos(edit,nullptr,int(49*scale),int(20*scale),int(width*scale),int(29*scale),SWP_NOZORDER|SWP_NOACTIVATE);
}
void layout() {
    dpi=GetDpiForWindow(hwnd);scale=dpi/96.f;if(inputFont)DeleteObject(inputFont);
    inputFont=CreateFontW(-MulDiv(19,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(inputFont),TRUE);position_edit();
}
std::wstring placeholder() {
    if(mode==Mode::Emoji)return L"Search emoji…";
    if(mode==Mode::Clipboard)return L"Search clipboard…";
    if(mode==Mode::Camera)return L"Camera Mirror";
    if(mode==Mode::Convert)return L"Calculate or convert…";
    return L"Search for apps or commands…";
}
void build_grid() {
    grid.clear();gridSections.clear();gridHeight=0;
    if(!emoji_view())return;
    float y=76;size_t start=0;
    while(start<rows.size()) {
        size_t end=start+1;while(end<rows.size()&&rows[end].group==rows[start].group)++end;
        gridSections.push_back({rows[start].group,int(end-start),y});y+=22;
        for(size_t n=start;n<end;++n) {
            int local=int(n-start);float x=15+(local%8)*91.f,top=y+(local/8)*91.f;
            grid.push_back({int(n),D2D1::RectF(x,top,x+83,top+83)});
        }
        y+=float((end-start+7)/8)*91+7;start=end;
    }
    gridHeight=y;gridOffset=std::clamp(gridOffset,0.f,std::max(0.f,gridHeight-FOOTER_TOP));
}
void ensure_selection_visible() {
    viewportScrolled=false;stop_scroll_animation();
    selected=std::clamp(selected,0,std::max(0,int(rows.size())-1));
    if(emoji_view()) {
        if(selected<int(grid.size())) {
            auto r=grid[selected].bounds;
            if(r.top-gridOffset<91)gridOffset=std::max(0.f,r.top-98);
            if(r.bottom-gridOffset>FOOTER_TOP-8)gridOffset=r.bottom-FOOTER_TOP+8;
        }
        gridOffset=std::clamp(gridOffset,0.f,std::max(0.f,gridHeight-FOOTER_TOP));return;
    }
    if(selected>=(conversionCard?1:0)&&selected<int(rows.size())){
        float y=list_row_offset(selected),height=FOOTER_TOP-list_top();
        if(y<scroll)scroll=y;
        if(y+ROW_HEIGHT>scroll+height)scroll=y+ROW_HEIGHT-height;
    }
    scroll=std::clamp(scroll,0.f,maximum_list_scroll());
}
int row_at(float x,float y) const {
    if(y<65||y>=FOOTER_TOP)return -1;
    if(emoji_view()) {
        for(auto& tile:grid){auto r=tile.bounds;r.top-=gridOffset;r.bottom-=gridOffset;if(x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)return tile.row;}return -1;
    }
    if(clipboard_view()&&x>=283)return -1;
    if(conversionCard&&y>=94&&y<220)return 0;
    if(y<list_top())return -1;
    auto visible=visible_list(scroll);
    for(int n=0;n<visible.count;++n){auto position=visible.items[n];if(y>=position.y&&y<position.y+ROW_HEIGHT)return position.row;}return -1;
}
void paint_header() {
    // Input, header, results and footer share the single backdrop painted below.
    if(mode!=Mode::Home){rect(15,21,22,22,selection(),4);draw_icon("arrow-left",19,25,14,ink());}
    else draw_icon("search",16,23,21,faint());
    if(emoji_view()||clipboard_view()) {
        outline(537,16,198,34,rule(),7);
        draw_icon(emoji_view()?"smile":"clipboard",550,25,16,ink());
        std::wstring filter=emoji_view()?(emojiCategory<0?L"All Categories":std::wstring(emoji_category_name(EmojiCategory(emojiCategory)))):(clipboardFilter==1?L"Text":clipboardFilter==2?L"Images":L"All Types");
        text(filter,574,23,139,24,body.p,ink());
        {brush->SetColor(muted());target->DrawLine(D2D1::Point2F(715,31),D2D1::Point2F(718,34),brush.p);target->DrawLine(D2D1::Point2F(718,34),D2D1::Point2F(721,31),brush.p);}
    } else draw_icon("settings-2",709,25,17,muted());
    paint_search_input(edit,target.p,placeholder().c_str());
    rect(1,65,748,1,rule());
}
void row_icon(const Row& row,float x,float y) {
    if(row.action==Action::App&&draw_app_icon(row.value,x,y))return;
    auto fill=light?color(112,118,127):color(104,110,122);
    if(row.glyph==L"clipboard")fill=color(242,135,35);
    else if(row.glyph==L"smile")fill=color(226,166,35);
    else if(row.glyph==L"camera")fill=color(228,85,103);
    else if(row.glyph==L"calculator"||row.glyph==L"arrow-left-right")fill=color(57,139,222);
    else if(row.glyph==L"clock-3")fill=color(146,106,214);
    rect(x,y,22,22,fill,5);draw_icon(narrow(row.glyph),x+4,y+4,14,color(255,255,255));
}
std::wstring row_shortcut(const Row& row) const {
    if(row.action==Action::Settings)return L"Ctrl ,";
    std::wstring id;
    if(row.action==Action::App)id=L"app:"+row.value;
    else if(row.action==Action::Mode){
        auto next=Mode(row.index);
        id=next==Mode::Apps?L"apps.open":next==Mode::Emoji?L"emoji.open":next==Mode::Clipboard?L"clipboard.open":next==Mode::Camera?L"camera.toggle":L"";
        if(next==Mode::Convert){for(const auto& binding:bindings)if(binding.id.starts_with(L"converter."))return hotkey_name(binding);}
    }
    if(!id.empty())for(const auto& binding:bindings)if(binding.id==id)return hotkey_name(binding);
    return {};
}
void paint_list() {
    bool clip=clipboard_view();float width=clip?283.f:750.f;
    if(scrollbar_geometry().maximum>0)width-=12;
    auto visible=visible_list(scroll);
    if(!conversionCard&&!(!rows.empty()&&list_group_header(0)))text(clip?L"Clipboard History":query.empty()?L"Suggestions":L"Results",15,76,width-30,20,small.p,muted());
    std::vector<std::wstring> visibleApps;
    for(int n=0;n<visible.count;++n){auto& row=rows[visible.items[n].row];if(row.action==Action::App)visibleApps.push_back(row.value);}
    appIcons.request(hwnd,MSG_ICONS,visibleApps);
    target->PushAxisAlignedClip(D2D1::RectF(1,list_top(),width,FOOTER_TOP),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for(int n=0;n<visible.count;++n) {
        auto position=visible.items[n];int index=position.row;auto& row=rows[index];float y=position.y;
        if(position.header)text(row.group,15,y-18,width-30,20,small.p,muted());
        if(index==selected)rect(7,y,width-14,38,selection(),9);
        else if(index==hover)rect(7,y,width-14,38,tile_fill(),9);
        row_icon(row,15,y+8);
        auto shortcut=clip?std::wstring{}:row_shortcut(row);
        text(row.title,49,y+8,width-(shortcut.empty()?64:220),24,body.p,ink());
        if(!shortcut.empty())aligned(shortcut,width-180,y+10,165,22,small.p,muted(),DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    target->PopAxisAlignedClip();
    paint_scrollbar();
}
void paint_emoji() {
    target->PushAxisAlignedClip(D2D1::RectF(1,66,749,FOOTER_TOP),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for(const auto& section:gridSections) {
        float y=section.y-gridOffset;if(y+20<66||y>=FOOTER_TOP)continue;
        text(section.name+L"   "+std::to_wstring(section.count),15,y,700,21,small.p,muted());
    }
    for(const auto& tile:grid) {
        auto r=tile.bounds;r.top-=gridOffset;r.bottom-=gridOffset;
        if(r.bottom<66||r.top>=FOOTER_TOP)continue;
        rect(r.left,r.top,83,83,tile.row==hover?selection():tile_fill(),10);
        if(tile.row==selected){outline(r.left+1,r.top+1,81,81,light?color(91,95,98):color(201,204,213),9,2);outline(r.left+3,r.top+3,77,77,light?color(255,255,255,.85f):color(255,255,255,.16f),7);}
        icon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        aligned(rows[tile.row].glyph,r.left,r.top,83,83,icon.p,ink(),DWRITE_TEXT_ALIGNMENT_CENTER);
        icon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    target->PopAxisAlignedClip();
    paint_scrollbar();
}
const HistoryItem* selected_history() const {
    if(selected<0||selected>=int(rows.size())||!rows[selected].identity)return nullptr;
    for(const auto& item:history)if(item.timestamp==rows[selected].identity)return &item;return nullptr;
}
std::wstring copied_time(uint64_t timestamp) {
    FILETIME ft{DWORD(timestamp),DWORD(timestamp>>32)},local;SYSTEMTIME time{};
    if(!FileTimeToLocalFileTime(&ft,&local)||!FileTimeToSystemTime(&local,&time))return L"Unknown";
    wchar_t date[64]{},clock[32]{};GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,DATE_SHORTDATE,&time,nullptr,date,64,nullptr);GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT,TIME_NOSECONDS,&time,nullptr,clock,32);
    return std::wstring(date)+L"  "+clock;
}
void paint_clipboard() {
    paint_list();rect(283,66,1,FOOTER_TOP-66,rule());
    auto item=selected_history();
    if(!item){aligned(historyEnabled?std::wstring(history.empty()?L"Copy text or an image to add it here":L"No matches"):L"Text and images · Encrypted on this PC · "+std::to_wstring(historyRetentionDays)+L" days",302,208,430,80,body.p,muted(),DWRITE_TEXT_ALIGNMENT_CENTER);return;}
    rect(300,82,433,208,tile_fill(),10);
    if(item->dib.empty())text(item->text.substr(0,12000),313,94,407,184,previewText.p,ink());
    else {
        BITMAPINFOHEADER h{};memcpy(&h,item->dib.data(),sizeof(h));
        auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE));
        if(!clipboardBitmap||clipboardBitmapIdentity!=item->timestamp){
            clipboardBitmap.reset();
            target->CreateBitmap(D2D1::SizeU(h.biWidth,-h.biHeight),item->dib.data()+40,h.biWidth*4,properties,clipboardBitmap.put());
            clipboardBitmapIdentity=item->timestamp;
        }
        float fit=std::min(407.f/h.biWidth,184.f/-h.biHeight),w=h.biWidth*fit,hh=-h.biHeight*fit;
        if(clipboardBitmap)target->DrawBitmap(clipboardBitmap.p,D2D1::RectF(313+(407-w)/2,94+(184-hh)/2,313+(407+w)/2,94+(184+hh)/2));
    }
    text(L"Information",300,294,420,22,small.p,muted());
    auto metadata=[&](const std::wstring& key,const std::wstring& value,float y){text(key,300,y,125,23,small.p,muted());aligned(value,423,y,310,23,small.p,ink(),DWRITE_TEXT_ALIGNMENT_TRAILING);rect(300,y+25,433,1,rule());};
    metadata(L"Type",item->dib.empty()?L"Text":L"Image",322);
    if(item->dib.empty())metadata(L"Characters",std::to_wstring(item->text.size()),350);
    else {BITMAPINFOHEADER h{};memcpy(&h,item->dib.data(),sizeof(h));metadata(L"Dimensions",std::to_wstring(h.biWidth)+L" × "+std::to_wstring(-h.biHeight),350);}
    const size_t bytes=item->dib.empty()?item->text.size()*sizeof(wchar_t):item->dib.size();
    wchar_t sizeLabel[64]{};
    if(bytes>=1024*1024)swprintf(sizeLabel,64,L"%.2f MiB",double(bytes)/(1024*1024));
    else if(bytes>=1024)swprintf(sizeLabel,64,L"%.1f KiB",double(bytes)/1024);
    else swprintf(sizeLabel,64,L"%llu bytes",static_cast<unsigned long long>(bytes));
    metadata(L"Stored size",sizeLabel,378);
    metadata(L"Copied",copied_time(item->timestamp),406);
}
std::pair<std::wstring,std::wstring> currency_label(const std::wstring& code) {
    const std::pair<const wchar_t*,const wchar_t*> names[]={{L"USD",L"US Dollar"},{L"IDR",L"Indonesian Rupiah"},{L"JPY",L"Japanese Yen"},{L"EUR",L"Euro"},{L"GBP",L"British Pound"},{L"SGD",L"Singapore Dollar"},{L"AUD",L"Australian Dollar"},{L"CAD",L"Canadian Dollar"},{L"KRW",L"South Korean Won"},{L"INR",L"Indian Rupee"},{L"MYR",L"Malaysian Ringgit"},{L"THB",L"Thai Baht"}};
    std::wstring label=code;for(auto [key,name]:names)if(code==key){label=name;break;}
    std::wstring symbol=code+L" ";if(code==L"IDR")symbol=L"Rp ";if(code==L"USD")symbol=L"$";if(code==L"JPY")symbol=L"¥";if(code==L"EUR")symbol=L"€";if(code==L"GBP")symbol=L"£";return {label,symbol};
}
std::wstring grouped_number(std::wstring value) {
    auto point=value.find(L'.');size_t end=point==value.npos?value.size():point,start=(!value.empty()&&(value[0]==L'-'||value[0]==L'+'))?1:0;
    for(size_t p=end;p>start+3;){p-=3;value.insert(p,1,L',');}return value;
}
void paint_conversion() {
    if(rows.empty())return;const auto& answer=rows.front();
    std::wstring input=trim(query);if(lower(input).starts_with(L"convert "))input=trim(input.substr(8));
    std::wstring output=answer.title,from=L"Expression",to=L"Result",note=answer.detail;
    if(auto currency=parse_currency(input,localCurrency)){
        auto source=currency_label(currency->from),destination=currency_label(currency->to);from=source.first;to=destination.first;
        if(!answer.value.empty()) {
            output=destination.second+grouped_number(answer.value);
            if(currency->from==currency->to)note=L"Same currency";
            else {auto rate=rates.find(currency->from==L"USD"?currency->to:currency->from);if(rate!=rates.end())note=L"Reference rates · "+rate->second.date;}
        }
    }else if(answer.glyph==L"clock-3"){from=L"Time";to=L"Converted time";}
    else if(auto units=convert_units(input)){from=L"Unit conversion";to=L"Result";}
    text(answer.glyph==L"arrow-left-right"?L"Currency":answer.glyph==L"clock-3"?L"Time":L"Calculator",15,76,500,20,small.p,muted());
    rect(7,94,736,126,selection(),10);
    if(selected==0)outline(7.5f,94.5f,735,125,rule(),10);
    rect(375,95,1,39,rule());rect(375,180,1,39,rule());
    // Long expressions shrink within their own layout instead of shifting the halves.
    auto value=[&](const std::wstring& s,float x,float width){
        Com<IDWriteTextLayout> layout;
        if(SUCCEEDED(textFactory->CreateTextLayout(s.c_str(),UINT32(s.size()),heading.p,width,45,layout.put()))){
            DWRITE_TEXT_RANGE range{0,UINT32(s.size())};layout->SetFontSize(s.size()>25?16.f:s.size()>17?20.f:24.f,range);layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            brush->SetColor(ink());target->DrawTextLayout(D2D1::Point2F(x,129),layout.p,brush.p,D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    };
    value(input,18,319);value(output,410,319);
    aligned(L"→",355,137,40,24,body.p,ink(),DWRITE_TEXT_ALIGNMENT_CENTER);
    aligned(note,160,161,430,22,small.p,faint(),DWRITE_TEXT_ALIGNMENT_CENTER);
    auto badge=[&](const std::wstring& s,float center){float width=std::clamp(float(s.size())*6.4f+16,54.f,250.f);rect(center-width/2,188,width,20,light?color(0,0,0,.075f):color(255,255,255,.07f),2);aligned(s,center-width/2,189,width,19,small.p,ink(),DWRITE_TEXT_ALIGNMENT_CENTER);};
    badge(from,192);badge(to,558);
    if(rows.size()>1){text(L"Applications & Commands",15,237,700,20,small.p,muted());paint_list();}
}
void paint_camera() {
    auto current=camera.latest_frame();
    if(current&&!current->pixels.empty()) {
        auto& frame=*current;
        if(cameraBitmap&&(cameraBitmap->GetPixelSize().width!=frame.width||cameraBitmap->GetPixelSize().height!=frame.height))cameraBitmap.reset();
        if(!cameraBitmap){auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE));target->CreateBitmap(D2D1::SizeU(frame.width,frame.height),frame.pixels.data(),frame.width*4,properties,cameraBitmap.put());}
        else if(cameraSequence!=frame.sequence)cameraBitmap->CopyFromMemory(nullptr,frame.pixels.data(),frame.width*4);cameraSequence=frame.sequence;
        float w=718,h=w*frame.height/frame.width;if(h>279){h=279;w=h*frame.width/frame.height;}
        if(cameraBitmap){if(mirrored)target->SetTransform(D2D1::Matrix3x2F::Scale(-1,1,D2D1::Point2F(375,0))*D2D1::Matrix3x2F::Scale(scale,scale));target->DrawBitmap(cameraBitmap.p,D2D1::RectF((750-w)/2,82,(750+w)/2,82+h));target->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale));}
    }else {rect(16,82,718,279,tile_fill(),7);draw_icon("camera",358,155,34,muted());aligned(camera.status(),32,221,686,70,previewText.p,muted(),DWRITE_TEXT_ALIGNMENT_CENTER);}
    const wchar_t* labels[]={L"Save",L"Copy",mirrored?L"Mirror: On":L"Mirror: Off",L"Next Camera"};
    for(int i=0;i<4;++i){float x=16+i*182;rect(x,375,172,32,tile_fill(),5);aligned(labels[i],x,381,172,24,body.p,ink(),DWRITE_TEXT_ALIGNMENT_CENTER);}
    text(camera.status(),16,413,718,18,small.p,muted());
}
std::wstring primary_label() const {
    if(mode==Mode::Camera)return L"Save Snapshot";
    if(rows.empty()||selected>=int(rows.size()))return L"";
    auto& row=rows[selected];
    if(row.action==Action::Copy){if(row.value.empty()&&!row.identity)return L"";return emoji_view()||clipboard_view()?L"Paste":L"Copy Answer";}
    return row.action==Action::Query?L"Use Example":L"Open";
}
void paint_footer() {
    rect(0,FOOTER_TOP,750,1,rule());
    std::wstring label=L"";std::string glyph="settings-2";
    if(emoji_view()){label=selected<int(rows.size())?rows[selected].title:L"Emoji";glyph="smile";}
    if(clipboard_view()){label=L"Clipboard History";glyph="clipboard";}
    if(mode==Mode::Camera){label=L"Camera Mirror";glyph="camera";}
    draw_icon(glyph,14,447,17,muted());
    text(toast.empty()?(label.empty()?status:label):toast,37,445,385,24,small.p,muted());
    auto primary=primary_label();if(!primary.empty()){aligned(primary,440,445,151,24,body.p,ink(),DWRITE_TEXT_ALIGNMENT_TRAILING);outline(598,446,18,18,rule(),3);draw_icon("corner-down-left",602,450,10,muted());}
    rect(628,447,1,15,rule());text(L"Actions",641,445,51,24,body.p,muted());keycap(L"Ctrl",694,446,26);keycap(L"K",724,446,17);
}
bool render_frame() {
    if(!init_graphics())return false;
    target->BeginDraw();target->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale));target->Clear(color(0,0,0,0));
    // DWM supplies live blur and noise. A denser, brighter neutral tint gives
    // the Start-menu-like opacity requested without introducing separate
    // header/body sheets, extra blur passes or a different fallback color.
    COLORREF tint=palette_background(light);
    float opacity=surface.acrylic?(light?.72f:.70f):1.f;
    rect(0,0,750,475,color(GetRValue(tint),GetGValue(tint),GetBValue(tint),opacity),8);
    paint_header();
    if(mode==Mode::Camera)paint_camera();else if(emoji_view())paint_emoji();else if(clipboard_view())paint_clipboard();else if(conversionCard)paint_conversion();else paint_list();
    if(rows.empty()&&mode!=Mode::Camera&&!clipboard_view())aligned(indexing&&apps.empty()?L"Finding apps…":L"No Results",175,225,400,29,body.p,muted(),DWRITE_TEXT_ALIGNMENT_CENTER);
    paint_footer();if(popup)paint_popup();
    auto hr=target->EndDraw();if(SUCCEEDED(hr))hr=surface.present();
    if(FAILED(hr))release_graphics();
    return SUCCEEDED(hr);
}
void paint() {
    PAINTSTRUCT ps;BeginPaint(hwnd,&ps);
    // Background index/rate updates may invalidate a hidden palette. Keep it
    // idle; show() explicitly renders fresh content before making it visible.
    if(IsWindowVisible(hwnd))render_frame();
    EndPaint(hwnd,&ps);
}
#include "palette_actions.inl"
