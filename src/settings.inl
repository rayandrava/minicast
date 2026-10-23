// Included inside App. Controls remain native for keyboard navigation and
// accessibility. One recorder and one clear button are reused over list cells;
// the app index never creates a native window for every command.
enum : int {
    SETTINGS_SEARCH=209, SETTINGS_STARTUP=210, SETTINGS_BACKGROUND=211,
    SETTINGS_DONE=212, SETTINGS_SHORTCUTS_TITLE=213, SETTINGS_GENERAL_TITLE=214,
    SETTINGS_SELECTED=215, SETTINGS_RETENTION=216, SETTINGS_EMPTY=217,
    SETTINGS_RETENTION_DAYS=218, SETTINGS_RETENTION_LABEL=219,
    SETTINGS_FEEDBACK=220, SETTINGS_VERSION=221, SETTINGS_UPDATES=222, SETTINGS_MENU_WINDOWS=230
};
UINT settingsDpi=96;
HFONT settingsHeadingFont=nullptr;
HBRUSH settingsBackgroundBrush=nullptr,settingsFieldBrush=nullptr;
HIMAGELIST settingsRowHeight=nullptr;
std::vector<int> settingsVisible;
bool settingsUpdating=false,settingsFeedbackError=false;
ShortcutChord settingsShortcut{};
bool settingsIncludeWindows=false,settingsMenuOpen=false,settingsRefreshPending=false;
std::wstring settingsEditingId,settingsClearId;

static LRESULT CALLBACK settings_control_proc(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR data) {
    auto self=reinterpret_cast<App*>(data);int id=GetDlgCtrlID(control);
    if(id==SETTINGS_RETENTION_DAYS&&(message==WM_GETDLGCODE||message==WM_KEYDOWN)){
        bool dropped=SendMessageW(control,CB_GETDROPPEDSTATE,0,0)!=0;
        if(message==WM_GETDLGCODE&&lp){
            auto key=reinterpret_cast<MSG*>(lp);
            if(key->message==WM_KEYDOWN&&(key->wParam==VK_RETURN||(key->wParam==VK_ESCAPE&&dropped)))
                return DefSubclassProc(control,message,wp,lp)|DLGC_WANTMESSAGE;
        }
        if(message==WM_KEYDOWN&&wp==VK_RETURN){
            if(dropped)return DefSubclassProc(control,message,wp,lp);
            if(!(lp&(LPARAM(1)<<30)))SendMessageW(control,CB_SHOWDROPDOWN,TRUE,0);
            return 0;
        }
        // Escape cancels an open dropdown instead of closing all Settings.
        if(message==WM_KEYDOWN&&wp==VK_ESCAPE&&dropped)return DefSubclassProc(control,message,wp,lp);
    }
    if(id==SETTINGS_HOTKEY){
        if(message==WM_CONTEXTMENU){self->settings_shortcut_menu(self->settingsEditingId,lp);return 0;}
        if(message==WM_KILLFOCUS&&!self->settingsMenuOpen&&reinterpret_cast<HWND>(wp)!=GetDlgItem(self->settings,SETTINGS_REMOVE))self->settings_end_recording(false);
        if(message==WM_MOUSEWHEEL||message==WM_MOUSEHWHEEL){
            self->settings_end_recording(true);
            SendMessageW(GetDlgItem(self->settings,SETTINGS_COMMAND),message,wp,lp);return 0;
        }
        // Also handle directly dispatched messages; the normal message pump
        // consumes these before IsDialogMessage and TranslateMessage.
        MSG key{control,message,wp,lp};
        if(self->settings_capture_message(key))return 0;
        if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS|DLGC_WANTCHARS;
    }
    if(id==SETTINGS_REMOVE&&message==WM_KILLFOCUS&&reinterpret_cast<HWND>(wp)!=GetDlgItem(self->settings,SETTINGS_HOTKEY))self->settings_end_recording(false);
    if(message==WM_GETDLGCODE&&lp){
        auto key=reinterpret_cast<MSG*>(lp);
        if(key->message==WM_KEYDOWN&&(key->wParam==VK_ESCAPE||
            (key->wParam==VK_RETURN&&(id==SETTINGS_SEARCH||id==SETTINGS_COMMAND))||
            (key->wParam==VK_TAB&&(id==SETTINGS_COMMAND||id==SETTINGS_REMOVE))||
            (key->wParam==VK_DELETE&&id==SETTINGS_COMMAND)))
            return DefSubclassProc(control,message,wp,lp)|DLGC_WANTMESSAGE;
    }
    if(message==WM_KEYDOWN&&wp==VK_ESCAPE){
        if(!(lp&(LPARAM(1)<<30)))PostMessageW(self->settings,WM_CLOSE,0,0);
        return 0;
    }
    if(message==WM_KEYDOWN&&wp==VK_RETURN&&(id==SETTINGS_SEARCH||id==SETTINGS_COMMAND)){
        if(!(lp&(LPARAM(1)<<30))&&!self->settings_id().empty()){
            if(id==SETTINGS_SEARCH)SetFocus(GetDlgItem(self->settings,SETTINGS_COMMAND));
            else self->settings_begin_recording(self->settings_id());
        }
        return 0;
    }
    if(message==WM_KEYDOWN&&wp==VK_DELETE&&id==SETTINGS_COMMAND&&
        !(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)&&!(GetKeyState(VK_SHIFT)&0x8000)&&
        !((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)){
        if(!(lp&(LPARAM(1)<<30)))self->settings_clear_shortcut(self->settings_id());return 0;
    }
    if(message==WM_KEYDOWN&&wp==VK_TAB&&(id==SETTINGS_COMMAND||id==SETTINGS_REMOVE)&&
        !(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)&&!((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)){
        if(!(lp&(LPARAM(1)<<30))){
            bool backwards=(GetKeyState(VK_SHIFT)&0x8000)!=0;
            HWND target=GetDlgItem(self->settings,backwards?(id==SETTINGS_REMOVE?SETTINGS_COMMAND:SETTINGS_SEARCH):
                (id==SETTINGS_COMMAND&&IsWindowVisible(GetDlgItem(self->settings,SETTINGS_REMOVE))?SETTINGS_REMOVE:SETTINGS_STARTUP));
            SetFocus(target);
        }
        return 0;
    }
    if(id==SETTINGS_COMMAND&&(message==WM_VSCROLL||message==WM_HSCROLL||message==WM_MOUSEWHEEL||message==WM_MOUSEHWHEEL)){
        self->settings_end_recording(false);
        auto result=DefSubclassProc(control,message,wp,lp);self->settings_position_clear();return result;
    }
    if(id==SETTINGS_COMMAND&&message==WM_CONTEXTMENU){
        auto target=self->settings_id();
        if(lp!=LPARAM(-1)){
            POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(control,&point);
            LVHITTESTINFO hit{};hit.pt=point;int row=ListView_SubItemHitTest(control,&hit);
            if(row<0)return 0;target=self->settings_row_id(row);
        }
        self->settings_shortcut_menu(target,lp);return 0;
    }
    // A report header sends its notifications to the list, not the window.
    if(message==WM_NOTIFY&&id==SETTINGS_COMMAND){
        auto notification=reinterpret_cast<NMHDR*>(lp);
        if(notification->hwndFrom==ListView_GetHeader(control)&&notification->code==NM_CUSTOMDRAW)
            return self->settings_message(self->settings,message,wp,lp);
    }
    return DefSubclassProc(control,message,wp,lp);
}

COLORREF settings_background() const {return light?RGB(246,247,249):RGB(28,28,30);}
COLORREF settings_field() const {return light?RGB(255,255,255):RGB(36,36,39);}
COLORREF settings_ink() const {return light?RGB(24,25,28):RGB(237,237,241);}
COLORREF settings_muted() const {return light?RGB(103,107,115):RGB(161,163,173);}
int settings_px(int value) const {return MulDiv(value,settingsDpi,96);}

void settings_feedback(const std::wstring& text,bool error=false) {
    if(!settings)return;
    settingsFeedbackError=error;
    SetDlgItemTextW(settings,SETTINGS_FEEDBACK,text.c_str());
    if(HWND label=GetDlgItem(settings,SETTINGS_FEEDBACK))InvalidateRect(label,nullptr,TRUE);
}

void settings_theme() {
    if(!settings)return;
    apply_window_theme(settings,light);
    if(settingsBackgroundBrush)DeleteObject(settingsBackgroundBrush);
    if(settingsFieldBrush)DeleteObject(settingsFieldBrush);
    settingsBackgroundBrush=CreateSolidBrush(settings_background());
    settingsFieldBrush=CreateSolidBrush(settings_field());
    HWND list=GetDlgItem(settings,SETTINGS_COMMAND);
    if(list){ListView_SetBkColor(list,settings_field());ListView_SetTextBkColor(list,settings_field());ListView_SetTextColor(list,settings_ink());}
    RedrawWindow(settings,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}

void settings_layout() {
    if(!settings)return;
    settings_end_recording(true);
    settingsDpi=GetDpiForWindow(settings);
    if(!settingsDpi)settingsDpi=96;
    HFONT font=CreateFontW(-settings_px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HFONT heading=CreateFontW(-settings_px(16),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(HWND child=GetWindow(settings,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
        SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    for(int id:{SETTINGS_SHORTCUTS_TITLE,SETTINGS_GENERAL_TITLE})
        SendDlgItemMessageW(settings,id,WM_SETFONT,reinterpret_cast<WPARAM>(heading),TRUE);
    if(settingsFont)DeleteObject(settingsFont);settingsFont=font;
    if(settingsHeadingFont)DeleteObject(settingsHeadingFont);settingsHeadingFont=heading;
    auto place=[&](int id,int x,int y,int width,int height){
        SetWindowPos(GetDlgItem(settings,id),nullptr,settings_px(x),settings_px(y),settings_px(width),settings_px(height),SWP_NOZORDER|SWP_NOACTIVATE);
    };
    RECT client{};GetClientRect(settings,&client);
    int compact=std::clamp(610-MulDiv(client.bottom-client.top,96,settingsDpi),0,130);
    auto below=[&](int id,int x,int y,int width,int height){place(id,x,y-compact,width,height);};
    place(SETTINGS_SHORTCUTS_TITLE,22,18,596,22);
    place(SETTINGS_SEARCH,22,49,596,32);
    place(SETTINGS_COMMAND,22,92,596,280-compact);
    place(SETTINGS_EMPTY,38,210-compact/2,564,24);
    below(SETTINGS_FEEDBACK,22,380,596,22);
    below(SETTINGS_GENERAL_TITLE,22,408,596,22);
    below(SETTINGS_STARTUP,22,440,272,26);
    below(SETTINGS_BACKGROUND,318,440,300,26);
    below(SETTINGS_HISTORY,22,482,190,26);
    below(SETTINGS_RETENTION_LABEL,224,484,62,22);
    below(SETTINGS_RETENTION_DAYS,290,480,112,130);
    SendDlgItemMessageW(settings,SETTINGS_RETENTION_DAYS,CB_SETITEMHEIGHT,WPARAM(-1),settings_px(24));
    below(SETTINGS_CLEAR,506,480,112,30);
    below(SETTINGS_RETENTION,22,518,596,20);
    below(SETTINGS_REFRESH,22,558,130,32);
    below(SETTINGS_VERSION,170,564,150,24);
    below(SETTINGS_UPDATES,330,558,174,32);
    below(SETTINGS_DONE,522,558,96,32);
    HWND list=GetDlgItem(settings,SETTINGS_COMMAND);
    RECT listClient{};GetClientRect(list,&listClient);
    int shortcutWidth=settings_px(248);
    int available=int(listClient.right)-((GetWindowLongPtrW(list,GWL_STYLE)&WS_VSCROLL)?0:GetSystemMetricsForDpi(SM_CXVSCROLL,settingsDpi));
    ListView_SetColumnWidth(list,0,std::max(settings_px(220),available-shortcutWidth));
    ListView_SetColumnWidth(list,1,shortcutWidth);
    HIMAGELIST height=ImageList_Create(1,settings_px(36),ILC_COLOR32,1,1);
    ListView_SetImageList(list,height,LVSIL_SMALL);
    if(settingsRowHeight)ImageList_Destroy(settingsRowHeight);settingsRowHeight=height;
    for(int id:{int(SETTINGS_SEARCH),SETTINGS_HOTKEY})
        SendDlgItemMessageW(settings,id,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(settings_px(9),settings_px(9)));
    settings_position_clear();
}

Binding settings_binding(const std::wstring& id) const {
    for(const auto& binding:bindings)if(binding.id==id)return binding;
    return {id};
}

std::wstring settings_id() {
    int row=ListView_GetNextItem(GetDlgItem(settings,SETTINGS_COMMAND),-1,LVNI_SELECTED);
    if(row<0||row>=int(settingsVisible.size()))return L"";
    int index=settingsVisible[row];return index>=0&&index<int(settingsCommands.size())?settingsCommands[index].first:L"";
}

std::wstring settings_row_id(int row) const {
    if(row<0||row>=int(settingsVisible.size()))return L"";
    int index=settingsVisible[row];
    return index>=0&&index<int(settingsCommands.size())?settingsCommands[index].first:L"";
}

int settings_row(const std::wstring& id) const {
    for(int row=0;row<int(settingsVisible.size());++row)if(settings_row_id(row)==id)return row;
    return -1;
}

bool settings_shortcut_rects(int row,RECT& chip,RECT& clear,bool fullyVisible=true) const {
    HWND list=GetDlgItem(settings,SETTINGS_COMMAND);RECT cell{},client{},header{};
    if(row<0||!ListView_GetSubItemRect(list,row,1,LVIR_BOUNDS,&cell))return false;
    GetClientRect(list,&client);GetWindowRect(ListView_GetHeader(list),&header);
    MapWindowPoints(nullptr,list,reinterpret_cast<POINT*>(&header),2);
    if(fullyVisible&&(cell.top<header.bottom||cell.bottom>client.bottom))return false;
    int middle=(cell.top+cell.bottom)/2;
    clear={cell.right-settings_px(34),middle-settings_px(13),cell.right-settings_px(8),middle+settings_px(13)};
    chip={cell.left+settings_px(8),middle-settings_px(13),clear.left-settings_px(6),middle+settings_px(13)};
    return chip.right>chip.left;
}

void settings_position_clear() {
    if(!settings)return;
    HWND button=GetDlgItem(settings,SETTINGS_REMOVE);if(!button)return;
    auto id=settings_id();auto binding=settings_binding(id);RECT chip{},clear{};
    bool hasShortcut=binding.key||(id==settingsEditingId&&settingsShortcut.key);
    if(!hasShortcut||!settings_shortcut_rects(settings_row(id),chip,clear)){
        settingsClearId.clear();ShowWindow(button,SW_HIDE);return;
    }
    settingsClearId=id;
    auto label=L"Clear shortcut for "+settings_label(id);SetWindowTextW(button,label.c_str());
    MapWindowPoints(GetDlgItem(settings,SETTINGS_COMMAND),settings,reinterpret_cast<POINT*>(&clear),2);
    SetWindowPos(button,HWND_TOP,clear.left,clear.top,clear.right-clear.left,clear.bottom-clear.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);
}

void settings_end_recording(bool focusList) {
    if(!settings||settingsEditingId.empty())return;
    settingsEditingId.clear();
    HWND recorder=GetDlgItem(settings,SETTINGS_HOTKEY);
    if(focusList&&GetFocus()==recorder)SetFocus(GetDlgItem(settings,SETTINGS_COMMAND));
    ShowWindow(recorder,SW_HIDE);
    if(!settingsFeedbackError)settings_feedback(L"");
    InvalidateRect(GetDlgItem(settings,SETTINGS_COMMAND),nullptr,FALSE);
    settings_position_clear();
}

void settings_begin_recording(const std::wstring& id) {
    if(!settings||id.empty())return;
    // Copy the target: ending the previous edit can invalidate a caller's ID.
    std::wstring target=id;int row=settings_row(target);if(row<0)return;
    settings_end_recording(false);
    HWND list=GetDlgItem(settings,SETTINGS_COMMAND);
    ListView_SetItemState(list,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_EnsureVisible(list,row,FALSE);
    RECT chip{},clear{};if(!settings_shortcut_rects(row,chip,clear))return;
    settingsEditingId=target;
    auto binding=settings_binding(target);settings_write_shortcut({binding.modifiers,binding.key});
    settingsIncludeWindows=false;
    MapWindowPoints(list,settings,reinterpret_cast<POINT*>(&chip),2);
    HWND recorder=GetDlgItem(settings,SETTINGS_HOTKEY);
    SetWindowPos(recorder,HWND_TOP,chip.left,chip.top,chip.right-chip.left,chip.bottom-chip.top,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    SetFocus(recorder);SendMessageW(recorder,EM_SETSEL,0,0);
    settings_feedback(L"Enter to save · Esc to cancel");settings_position_clear();
    InvalidateRect(list,nullptr,FALSE);
}

void settings_shortcut_menu(const std::wstring& id,LPARAM point) {
    if(id.empty())return;
    auto target=id;
    if(settingsEditingId!=target)settings_begin_recording(target);
    if(settingsEditingId!=target)return;
    HWND recorder=GetDlgItem(settings,SETTINGS_HOTKEY);POINT position{};
    if(point==LPARAM(-1)){RECT rect{};GetWindowRect(recorder,&rect);position={rect.left,rect.bottom};}
    else position={GET_X_LPARAM(point),GET_Y_LPARAM(point)};
    auto chord=settingsShortcut;
    HMENU menu=CreatePopupMenu();if(!menu)return;
    AppendMenuW(menu,MF_STRING|(chord.modifiers&MOD_WIN?MF_CHECKED:MF_UNCHECKED),SETTINGS_MENU_WINDOWS,L"Include Windows key");
    settingsMenuOpen=true;
    int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,position.x,position.y,0,settings,nullptr);DestroyMenu(menu);settingsMenuOpen=false;
    if(!settings)return;
    if(settings_row(target)<0){if(settingsRefreshPending)settings_refresh_commands();return;}
    if(choice==SETTINGS_MENU_WINDOWS){
        if(settingsEditingId!=target)settings_begin_recording(target);
        chord.modifiers^=MOD_WIN;settingsIncludeWindows=(chord.modifiers&MOD_WIN)!=0;
        settings_record_chord(chord);SetFocus(recorder);
    }else if(settingsEditingId==target)SetFocus(recorder);
    if(settingsRefreshPending)settings_refresh_commands();
}

std::wstring settings_label(const std::wstring& id) const {
    for(const auto& item:settingsCommands)if(item.first==id)return item.second;
    return id;
}

Binding settings_draft() {
    return {settingsEditingId,settingsShortcut.modifiers,settingsShortcut.key};
}

void settings_write_shortcut(ShortcutChord chord) {
    settingsShortcut=chord;
    auto label=chord.key?hotkey_name({L"",chord.modifiers,chord.key}):(chord.modifiers&MOD_WIN?L"Win + …":L"");
    SetDlgItemTextW(settings,SETTINGS_HOTKEY,label.c_str());
}

bool settings_recording() const {
    return settings&&!settingsEditingId.empty()&&GetForegroundWindow()==settings&&
        GetFocus()==GetDlgItem(settings,SETTINGS_HOTKEY)&&
        IsWindowEnabled(GetDlgItem(settings,SETTINGS_HOTKEY));
}

void settings_record_chord(ShortcutChord chord) {
    settings_write_shortcut(chord);settings_feedback(L"Enter to save · Esc to cancel");settings_position_clear();
}

bool settings_capture_registered_hotkey(LPARAM value) {
    if(!settings_recording())return false;
    // Keep registrations intact. An existing Minicast shortcut records here
    // instead of opening the launcher, clipboard or camera while editing.
    auto chord=shortcut_from_hotkey(value);if(settingsIncludeWindows)chord.modifiers|=MOD_WIN;
    settings_record_chord(chord);return true;
}

bool settings_capture_message(const MSG& message) {
    if(!settings_recording()||message.hwnd!=GetDlgItem(settings,SETTINGS_HOTKEY))return false;
    if(message.message==WM_KEYUP||message.message==WM_SYSKEYUP||
        message.message==WM_CHAR||message.message==WM_SYSCHAR||
        message.message==WM_DEADCHAR||message.message==WM_SYSDEADCHAR)return true;
    if(message.message!=WM_KEYDOWN&&message.message!=WM_SYSKEYDOWN)return false;
    UINT modifiers=0;
    if(GetKeyState(VK_CONTROL)&0x8000)modifiers|=MOD_CONTROL;
    if(GetKeyState(VK_MENU)&0x8000)modifiers|=MOD_ALT;
    if(GetKeyState(VK_SHIFT)&0x8000)modifiers|=MOD_SHIFT;
    if((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)modifiers|=MOD_WIN;
    if(message.wParam==VK_APPS&&!modifiers){
        if(!(message.lParam&(LPARAM(1)<<30)))settings_shortcut_menu(settingsEditingId,LPARAM(-1));return true;
    }
    auto input=shortcut_input(UINT(message.wParam),modifiers,(message.lParam&(LPARAM(1)<<30))!=0);
    switch(input){
        case ShortcutInput::capture:
            settings_record_chord({modifiers|(settingsIncludeWindows?UINT(MOD_WIN):0u),UINT(message.wParam)});break;
        case ShortcutInput::clear:settingsIncludeWindows=false;settings_record_chord({});break;
        case ShortcutInput::cancel:
            settings_end_recording(true);break;
        case ShortcutInput::accept:settings_commit_recording();break;
        case ShortcutInput::next:case ShortcutInput::previous:{
            // Commit before traversing native controls. On failure keep the
            // draft in its row, so a conflict never silently drops the edit.
            bool backwards=input==ShortcutInput::previous;
            if(settings_commit_recording()){
                HWND next=backwards?GetDlgItem(settings,SETTINGS_SEARCH):
                    (IsWindowVisible(GetDlgItem(settings,SETTINGS_REMOVE))?GetDlgItem(settings,SETTINGS_REMOVE):GetDlgItem(settings,SETTINGS_STARTUP));
                SetFocus(next);
            }
            break;
        }
        case ShortcutInput::ignore:break;
    }
    return true;
}

void settings_selection() {
    if(settingsUpdating||!settings)return;
    auto id=settings_id();
    if(!settingsEditingId.empty()&&settingsEditingId!=id)settings_end_recording(false);
    if(settingsEditingId.empty())settings_feedback(L"");
    settings_position_clear();
}

void settings_filter(const std::wstring& preferred=L"") {
    if(!settings)return;
    auto previous=preferred.empty()?settings_id():preferred;
    settings_end_recording(true);
    int length=GetWindowTextLengthW(GetDlgItem(settings,SETTINGS_SEARCH));
    std::wstring query(size_t(length)+1,L'\0');
    GetDlgItemTextW(settings,SETTINGS_SEARCH,query.data(),length+1);query.resize(length);
    query=lower(trim(query));
    HWND list=GetDlgItem(settings,SETTINGS_COMMAND);
    settingsUpdating=true;SendMessageW(list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list);settingsVisible.clear();
    int selected=0;
    for(int index=0;index<int(settingsCommands.size());++index){
        const auto& [id,label]=settingsCommands[index];auto binding=settings_binding(id);
        auto shortcut=binding.key?hotkey_name(binding):L"Record shortcut";
        if(!query.empty()&&!contains_folded(label,query)&&!contains_folded(shortcut,query))continue;
        int row=int(settingsVisible.size());settingsVisible.push_back(index);if(id==previous)selected=row;
        LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=row;item.pszText=const_cast<wchar_t*>(label.c_str());
        ListView_InsertItem(list,&item);ListView_SetItemText(list,row,1,const_cast<wchar_t*>(shortcut.c_str()));
    }
    if(!settingsVisible.empty()){ListView_SetItemState(list,selected,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(list,selected,FALSE);}
    ShowWindow(GetDlgItem(settings,SETTINGS_EMPTY),settingsVisible.empty()?SW_SHOW:SW_HIDE);
    SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);settingsUpdating=false;settings_selection();
}

void settings_refresh_commands() {
    if(!settings)return;
    if(settingsMenuOpen){settingsRefreshPending=true;return;}
    settingsRefreshPending=false;
    auto selected=settings_id(),editing=settingsEditingId;
    auto draft=settingsShortcut;bool includeWindows=settingsIncludeWindows,error=settingsFeedbackError;
    HWND previousFocus=GetFocus();
    wchar_t feedback[1024]{};GetDlgItemTextW(settings,SETTINGS_FEEDBACK,feedback,1024);
    // The index can reorder or remove apps. Recreate the overlay using its
    // stable command ID, never the new occupant of its previous row number.
    settings_end_recording(true);
    settingsCommands.clear();settingsVisible.clear();
    settingsCommands.emplace_back(L"launcher.open",L"Open Minicast");
    for(const auto& [id,label]:commands)if(wcscmp(id,L"launcher.open")!=0)settingsCommands.emplace_back(id,label);
    for(const auto& entry:apps)settingsCommands.emplace_back(L"app:"+entry.target,entry.name);
    settings_filter(selected);
    if(!editing.empty()&&settings_row(editing)>=0){
        settings_begin_recording(editing);settingsIncludeWindows=includeWindows;
        settings_record_chord(draft);settings_feedback(feedback,error);
        if(previousFocus==GetDlgItem(settings,SETTINGS_REMOVE)&&IsWindowVisible(previousFocus))SetFocus(previousFocus);
    }
    EnableWindow(GetDlgItem(settings,SETTINGS_REFRESH),!indexing);
}

void settings_select_command(const std::wstring& id) {
    if(!settings)return;
    settingsUpdating=true;SetDlgItemTextW(settings,SETTINGS_SEARCH,L"");settingsUpdating=false;
    settings_filter(id);settings_begin_recording(id);
}

void settings_sync_options() {
    if(!settings)return;
    SendDlgItemMessageW(settings,SETTINGS_HISTORY,BM_SETCHECK,historyEnabled?BST_CHECKED:BST_UNCHECKED,0);
    SendDlgItemMessageW(settings,SETTINGS_STARTUP,BM_SETCHECK,mini::startup_enabled()?BST_CHECKED:BST_UNCHECKED,0);
    SendDlgItemMessageW(settings,SETTINGS_BACKGROUND,BM_SETCHECK,keepRunning?BST_CHECKED:BST_UNCHECKED,0);
    SendDlgItemMessageW(settings,SETTINGS_RETENTION_DAYS,CB_SETCURSEL,historyRetentionDays==30?2:historyRetentionDays==14?1:0,0);
    EnableWindow(GetDlgItem(settings,SETTINGS_REFRESH),!indexing);
}

void open_settings() {
    if(settings){settings_sync_options();ShowWindow(settings,SW_SHOW);SetForegroundWindow(settings);return;}
    cameraPending=false;camera.stop();settingsDpi=GetDpiForWindow(hwnd);
    const DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN;
    const DWORD extended=WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT;
    RECT bounds{0,0,settings_px(640),settings_px(610)};AdjustWindowRectExForDpi(&bounds,style,FALSE,extended,settingsDpi);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor);
    int width=int(bounds.right-bounds.left),height=std::min(int(bounds.bottom-bounds.top),int(monitor.rcWork.bottom-monitor.rcWork.top));
    int x=monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2;
    int y=monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2;
    settings=CreateWindowExW(extended,L"MinicastSettings",L"Minicast Settings",style,x,y,width,height,hwnd,nullptr,instance,nullptr);
    if(!settings)return;
    auto add=[&](const wchar_t* cls,const wchar_t* label,DWORD controlStyle,int id){
        HWND child=control(settings,cls,label,controlStyle,0,0,1,1,id);
        // Empty class overrides disable visual styles using the documented API.
        // It lets standard native controls use WM_CTLCOLOR in either app theme.
        SetWindowTheme(child,L"",L"");
        SetWindowSubclass(child,settings_control_proc,1,reinterpret_cast<DWORD_PTR>(this));return child;
    };
    add(L"STATIC",L"Shortcuts",SS_NOPREFIX,SETTINGS_SHORTCUTS_TITLE);
    HWND search=add(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,SETTINGS_SEARCH);
    SendMessageW(search,EM_SETLIMITTEXT,128,0);
    SendMessageW(search,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Search commands or apps"));
    HWND list=add(WC_LISTVIEWW,L"Commands and apps",WS_BORDER|WS_TABSTOP|WS_CLIPSIBLINGS|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER|LVS_SHAREIMAGELISTS,SETTINGS_COMMAND);
    ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    HWND header=ListView_GetHeader(list);SetWindowLongPtrW(header,GWL_STYLE,GetWindowLongPtrW(header,GWL_STYLE)|HDS_NOSIZING);
    LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=328;column.pszText=const_cast<wchar_t*>(L"Command / App");ListView_InsertColumn(list,0,&column);
    column.cx=248;column.pszText=const_cast<wchar_t*>(L"Shortcut");ListView_InsertColumn(list,1,&column);
    add(L"STATIC",L"No matches",SS_CENTER|SS_NOPREFIX,SETTINGS_EMPTY);
    // HOTKEY_CLASS deliberately excludes Space and several other useful keys.
    // A native read-only EDIT exposes the label accessibly while our focused
    // message filter records the exact chord without automatic modifiers.
    HWND recorder=add(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_READONLY|ES_AUTOHSCROLL|ES_CENTER,SETTINGS_HOTKEY);
    SendMessageW(recorder,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Press shortcut"));ShowWindow(recorder,SW_HIDE);
    HWND clearShortcut=add(L"BUTTON",L"Clear shortcut",BS_OWNERDRAW|WS_TABSTOP,SETTINGS_REMOVE);ShowWindow(clearShortcut,SW_HIDE);
    add(L"STATIC",L"",SS_NOPREFIX,SETTINGS_FEEDBACK);
    add(L"STATIC",L"General",SS_NOPREFIX,SETTINGS_GENERAL_TITLE);
    add(L"BUTTON",L"Start with Windows",BS_AUTOCHECKBOX|WS_TABSTOP,SETTINGS_STARTUP);
    add(L"BUTTON",L"Close to tray",BS_AUTOCHECKBOX|WS_TABSTOP,SETTINGS_BACKGROUND);
    add(L"BUTTON",L"Clipboard history",BS_AUTOCHECKBOX|WS_TABSTOP,SETTINGS_HISTORY);
    add(L"STATIC",L"Keep &for",0,SETTINGS_RETENTION_LABEL);
    HWND retention=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,SETTINGS_RETENTION_DAYS);
    for(int days:{7,14,30}){
        auto label=std::to_wstring(days)+L" days";
        auto index=SendMessageW(retention,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        SendMessageW(retention,CB_SETITEMDATA,index,days);
    }
    add(L"BUTTON",L"Clear history",BS_OWNERDRAW|WS_TABSTOP,SETTINGS_CLEAR);
    add(L"STATIC",L"Text only · encrypted locally · 500 items / 4 MiB",SS_NOPREFIX,SETTINGS_RETENTION);
    add(L"BUTTON",L"Refresh apps",BS_OWNERDRAW|WS_TABSTOP,SETTINGS_REFRESH);
    add(L"STATIC",L"v" MINICAST_VERSION_WSTRING,SS_NOPREFIX,SETTINGS_VERSION);
    add(L"BUTTON",L"Check for updates",BS_OWNERDRAW|WS_TABSTOP,SETTINGS_UPDATES);
    add(L"BUTTON",L"&Done",BS_OWNERDRAW|WS_TABSTOP,SETTINGS_DONE);
    settings_layout();settings_theme();settings_refresh_commands();settings_sync_options();
    ShowWindow(settings,SW_SHOW);SetForegroundWindow(settings);SetFocus(search);
}

void unregister_command_hotkeys(const std::wstring& command) {
    for(auto it=hotkeys.begin();it!=hotkeys.end();){if(it->second==command){UnregisterHotKey(hwnd,it->first);it=hotkeys.erase(it);}else ++it;}
    if(command==L"launcher.open")registeredLauncher={};
}

bool settings_apply_shortcut(const Binding& draft) {
    if(draft.id.empty())return false;
    auto proposed=bindings;
    proposed.erase(std::remove_if(proposed.begin(),proposed.end(),[&](const auto& binding){return binding.id==draft.id;}),proposed.end());
    if(!draft.key){
        if(!settings_binding(draft.id).key)return true;
        if(!save_settings(&proposed)){settings_feedback(L"Couldn't save. Your previous shortcut is unchanged.",true);return false;}
        bindings=std::move(proposed);unregister_command_hotkeys(draft.id);
        if(draft.id==L"launcher.open")tray(true);
        return true;
    }
    switch(shortcut_problem({draft.modifiers,draft.key})){
        case ShortcutProblem::reserved:settings_feedback(L"F12 is reserved by Windows.",true);return false;
        case ShortcutProblem::needs_modifier:settings_feedback(L"Add Ctrl, Alt, Shift, or Windows.",true);return false;
        case ShortcutProblem::empty:case ShortcutProblem::modifier:settings_feedback(L"Press a key with your modifiers.",true);return false;
        case ShortcutProblem::none:break;
    }
    for(const auto& binding:bindings){
        if(binding.id!=draft.id&&binding.modifiers==draft.modifiers&&binding.key==draft.key){
            settings_feedback(L"Used by "+settings_label(binding.id)+L".",true);return false;
        }
    }
    auto current=settings_binding(draft.id);
    if(current.key==draft.key&&current.modifiers==draft.modifiers){
        for(const auto& item:hotkeys)if(item.second==draft.id)return true;
    }
    int runtime=nextHotkey++;
    // Register first: a conflict must leave the previous working shortcut intact.
    if(!RegisterHotKey(hwnd,runtime,draft.modifiers|MOD_NOREPEAT,draft.key)){
        DWORD error=GetLastError();
        settings_feedback(error==ERROR_HOTKEY_ALREADY_REGISTERED?
            L"Windows or another app is using this shortcut.":
            L"Windows couldn't register this shortcut.",true);return false;
    }
    proposed.push_back(draft);
    if(!save_settings(&proposed)){
        // The old registration and unsaved draft remain available for retry.
        UnregisterHotKey(hwnd,runtime);
        settings_feedback(L"Couldn't save. Your previous shortcut is unchanged.",true);return false;
    }
    bindings=std::move(proposed);unregister_command_hotkeys(draft.id);hotkeys[runtime]=draft.id;
    if(draft.id==L"launcher.open"){registeredLauncher={draft.modifiers,draft.key};tray(true);}
    return true;
}

bool settings_commit_recording() {
    auto draft=settings_draft();if(draft.id.empty())return true;
    if(!settings_apply_shortcut(draft))return false;
    settings_end_recording(true);settings_filter(draft.id);settings_feedback(L"");return true;
}

void settings_clear_shortcut(const std::wstring& id) {
    if(id.empty())return;
    auto target=id;settings_end_recording(true);
    if(!settings_apply_shortcut({target,0,0}))return;
    settings_filter(target);settings_feedback(L"");SetFocus(GetDlgItem(settings,SETTINGS_COMMAND));
}

COLORREF settings_row_fill(bool selected) const {
    return selected?(light?RGB(232,235,240):RGB(48,49,53)):settings_field();
}

void settings_draw_cross(HDC dc,const RECT& rect,COLORREF color) {
    LOGBRUSH brush{BS_SOLID,color,0};
    HPEN pen=ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND,settings_px(2),&brush,0,nullptr);
    auto previous=SelectObject(dc,pen);int x=(rect.left+rect.right)/2,y=(rect.top+rect.bottom)/2,r=settings_px(4);
    MoveToEx(dc,x-r,y-r,nullptr);LineTo(dc,x+r,y+r);
    MoveToEx(dc,x+r,y-r,nullptr);LineTo(dc,x-r,y+r);
    SelectObject(dc,previous);DeleteObject(pen);
}

LRESULT settings_draw_row(NMLVCUSTOMDRAW& item) {
    HWND list=GetDlgItem(settings,SETTINGS_COMMAND);int row=int(item.nmcd.dwItemSpec);
    auto id=settings_row_id(row);if(id.empty())return CDRF_SKIPDEFAULT;
    bool selected=ListView_GetItemState(list,row,LVIS_SELECTED)!=0;
    RECT bounds{};ListView_GetItemRect(list,row,&bounds,LVIR_BOUNDS);
    HDC dc=item.nmcd.hdc;int saved=SaveDC(dc);IntersectClipRect(dc,bounds.left,bounds.top,bounds.right,bounds.bottom);
    HBRUSH fill=CreateSolidBrush(settings_row_fill(selected));FillRect(dc,&bounds,fill);DeleteObject(fill);
    SelectObject(dc,settingsFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,settings_ink());
    RECT label=bounds;label.left+=settings_px(10);label.right=ListView_GetColumnWidth(list,0)-settings_px(10);
    const auto& name=settingsCommands[settingsVisible[row]].second;
    DrawTextW(dc,name.c_str(),-1,&label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    RECT chip{},clear{};
    if(settings_shortcut_rects(row,chip,clear,false)){
        auto binding=settings_binding(id);
        auto text=binding.key?hotkey_name(binding):L"Record shortcut";
        HBRUSH chipFill=CreateSolidBrush(settings_field());HPEN border=CreatePen(PS_SOLID,1,light?RGB(210,214,220):RGB(68,68,75));
        auto oldBrush=SelectObject(dc,chipFill),oldPen=SelectObject(dc,border);
        RoundRect(dc,chip.left,chip.top,chip.right,chip.bottom,settings_px(8),settings_px(8));
        SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(chipFill);DeleteObject(border);
        SetTextColor(dc,binding.key?settings_ink():settings_muted());RECT textRect=chip;InflateRect(&textRect,-settings_px(6),0);
        DrawTextW(dc,text.c_str(),-1,&textRect,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        if(binding.key)settings_draw_cross(dc,clear,settings_muted());
    }
    if(selected&&GetFocus()==list&&!(SendMessageW(list,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){
        InflateRect(&bounds,-settings_px(2),-settings_px(2));DrawFocusRect(dc,&bounds);
    }
    RestoreDC(dc,saved);return CDRF_SKIPDEFAULT;
}

LRESULT settings_draw_button(const DRAWITEMSTRUCT& item) {
    bool disabled=(item.itemState&ODS_DISABLED)!=0,pressed=(item.itemState&ODS_SELECTED)!=0;
    if(item.CtlID==SETTINGS_REMOVE){
        auto fill=CreateSolidBrush(pressed?(light?RGB(216,220,228):RGB(66,67,73)):settings_row_fill(true));
        FillRect(item.hDC,&item.rcItem,fill);DeleteObject(fill);settings_draw_cross(item.hDC,item.rcItem,settings_muted());
        if((item.itemState&ODS_FOCUS)&&!(item.itemState&ODS_NOFOCUSRECT)){RECT focus=item.rcItem;InflateRect(&focus,-2,-2);DrawFocusRect(item.hDC,&focus);}
        return TRUE;
    }
    COLORREF fill=pressed?(light?RGB(226,229,235):RGB(55,55,61)):settings_field();
    COLORREF border=light?RGB(208,212,220):RGB(68,68,75);
    FillRect(item.hDC,&item.rcItem,settingsBackgroundBrush);
    HBRUSH brush=CreateSolidBrush(fill);HPEN pen=CreatePen(PS_SOLID,1,border);
    auto oldBrush=SelectObject(item.hDC,brush),oldPen=SelectObject(item.hDC,pen);
    RoundRect(item.hDC,item.rcItem.left,item.rcItem.top,item.rcItem.right,item.rcItem.bottom,settings_px(8),settings_px(8));
    SelectObject(item.hDC,oldBrush);SelectObject(item.hDC,oldPen);DeleteObject(brush);DeleteObject(pen);
    auto oldFont=SelectObject(item.hDC,settingsFont);
    SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,disabled?settings_muted():settings_ink());
    wchar_t label[80]{};GetWindowTextW(item.hwndItem,label,80);RECT text=item.rcItem;
    UINT format=DT_CENTER|DT_VCENTER|DT_SINGLELINE;if(item.itemState&ODS_NOACCEL)format|=DT_HIDEPREFIX;
    DrawTextW(item.hDC,label,-1,&text,format);
    if((item.itemState&ODS_FOCUS)&&!(item.itemState&ODS_NOFOCUSRECT)){InflateRect(&text,-settings_px(4),-settings_px(4));DrawFocusRect(item.hDC,&text);}
    SelectObject(item.hDC,oldFont);
    return TRUE;
}

LRESULT settings_message(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    switch(message){
        case WM_COMMAND:{
            int id=LOWORD(wp),notification=HIWORD(wp);
            if(id==IDCANCEL||id==SETTINGS_DONE){DestroyWindow(window);return 0;}
            if(id==SETTINGS_UPDATES){open_releases();return 0;}
            if(id==SETTINGS_SEARCH&&notification==EN_CHANGE&&!settingsUpdating){settings_filter();return 0;}
            if(id==IDOK){if(!settingsEditingId.empty())settings_commit_recording();return 0;}
            if(id==SETTINGS_REMOVE){auto target=settingsClearId;settings_clear_shortcut(target);return 0;}
            if(id==SETTINGS_STARTUP){
                bool enabled=SendDlgItemMessageW(window,SETTINGS_STARTUP,BM_GETCHECK,0,0)==BST_CHECKED;std::wstring error;
                if(!mini::set_startup_enabled(enabled,error)){settings_sync_options();settings_feedback(error.empty()?L"Could not update startup.":error,true);}
                else settings_feedback(L"");return 0;
            }
            if(id==SETTINGS_BACKGROUND){
                bool previous=keepRunning;
                keepRunning=SendDlgItemMessageW(window,SETTINGS_BACKGROUND,BM_GETCHECK,0,0)==BST_CHECKED;
                bool saved=save_settings();if(!saved)keepRunning=previous;
                settings_sync_options();settings_feedback(saved?L"":L"Couldn't save. Close to tray is unchanged.",!saved);return 0;
            }
            if(id==SETTINGS_HISTORY){
                bool saved=enable_history(SendDlgItemMessageW(window,SETTINGS_HISTORY,BM_GETCHECK,0,0)==BST_CHECKED);
                settings_feedback(saved?L"":L"Couldn't save. Clipboard history is unchanged.",!saved);return 0;
            }
            if(id==SETTINGS_RETENTION_DAYS){
                if(notification==CBN_SELENDOK||(notification==CBN_SELCHANGE&&!SendDlgItemMessageW(window,id,CB_GETDROPPEDSTATE,0,0))){
                    int index=int(SendDlgItemMessageW(window,id,CB_GETCURSEL,0,0));
                    if(index>=0&&index<3){
                        int days=int(SendDlgItemMessageW(window,id,CB_GETITEMDATA,index,0));
                        bool saved=set_history_retention(days);
                        settings_sync_options();settings_feedback(saved?L"":L"Couldn't save retention. Try again.",!saved);
                    }
                }else if(notification==CBN_SELENDCANCEL)settings_sync_options();
                return 0;
            }
            if(id==SETTINGS_CLEAR){clear_history();settings_feedback(L"Clearing history…");return 0;}
            if(id==SETTINGS_REFRESH){scan_apps();EnableWindow(GetDlgItem(window,SETTINGS_REFRESH),!indexing);settings_feedback(L"");return 0;}
            break;
        }
        case WM_NOTIFY:{
            auto header=reinterpret_cast<NMHDR*>(lp);HWND list=GetDlgItem(window,SETTINGS_COMMAND);
            if(header->hwndFrom==list){
                if(header->code==LVN_ITEMCHANGED){auto change=reinterpret_cast<NMLISTVIEW*>(lp);if((change->uChanged&LVIF_STATE)&&((change->uOldState^change->uNewState)&LVIS_SELECTED))settings_selection();return 0;}
                if(header->code==LVN_BEGINSCROLL){settings_end_recording(true);ShowWindow(GetDlgItem(window,SETTINGS_REMOVE),SW_HIDE);return 0;}
                if(header->code==LVN_ENDSCROLL){settings_position_clear();return 0;}
                if(header->code==NM_CLICK){
                    auto click=reinterpret_cast<NMITEMACTIVATE*>(lp);auto target=settings_row_id(click->iItem);RECT chip{},clear{};
                    if(!target.empty()&&settings_shortcut_rects(click->iItem,chip,clear)){
                        if(PtInRect(&clear,click->ptAction)&&settings_binding(target).key)settings_clear_shortcut(target);
                        else if(PtInRect(&chip,click->ptAction))settings_begin_recording(target);
                    }
                    return 0;
                }
                if(header->code==NM_DBLCLK){auto click=reinterpret_cast<NMITEMACTIVATE*>(lp);settings_begin_recording(settings_row_id(click->iItem));return 0;}
                if(header->code==LVN_KEYDOWN){auto key=reinterpret_cast<NMLVKEYDOWN*>(lp);if(key->wVKey==VK_RETURN){settings_begin_recording(settings_id());return 0;}break;}
                if(header->code==NM_CUSTOMDRAW){
                    auto draw=reinterpret_cast<NMLVCUSTOMDRAW*>(lp);
                    if(draw->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
                    if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT)return settings_draw_row(*draw);
                }
            }
            if(list&&header->hwndFrom==ListView_GetHeader(list)&&header->code==NM_CUSTOMDRAW){
                auto draw=reinterpret_cast<NMCUSTOMDRAW*>(lp);
                if(draw->dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
                if(draw->dwDrawStage==CDDS_ITEMPREPAINT){
                    FillRect(draw->hdc,&draw->rc,settingsBackgroundBrush);SetBkMode(draw->hdc,TRANSPARENT);SetTextColor(draw->hdc,settings_muted());
                    wchar_t label[80]{};HDITEMW item{};item.mask=HDI_TEXT;item.pszText=label;item.cchTextMax=80;
                    Header_GetItem(header->hwndFrom,int(draw->dwItemSpec),&item);RECT text=draw->rc;text.left+=settings_px(8);
                    DrawTextW(draw->hdc,label,-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);return CDRF_SKIPDEFAULT;
                }
            }
            break;
        }
        case WM_DRAWITEM:return settings_draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));
        case WM_ERASEBKGND:{RECT client{};GetClientRect(window,&client);FillRect(reinterpret_cast<HDC>(wp),&client,settingsBackgroundBrush?settingsBackgroundBrush:GetSysColorBrush(COLOR_BTNFACE));return 1;}
        case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{
            HDC dc=reinterpret_cast<HDC>(wp);int id=GetDlgCtrlID(reinterpret_cast<HWND>(lp));
            bool field=message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX||id==SETTINGS_EMPTY||id==SETTINGS_HOTKEY||id==SETTINGS_RETENTION_DAYS;
            COLORREF foreground=(id==SETTINGS_RETENTION||id==SETTINGS_FEEDBACK||id==SETTINGS_EMPTY)?settings_muted():settings_ink();
            if(id==SETTINGS_FEEDBACK&&settingsFeedbackError)foreground=light?RGB(173,45,48):RGB(255,154,154);
            SetTextColor(dc,foreground);SetBkColor(dc,field?settings_field():settings_background());
            return reinterpret_cast<LRESULT>(field?settingsFieldBrush:settingsBackgroundBrush);
        }
        case WM_DPICHANGED:{
            auto bounds=reinterpret_cast<RECT*>(lp);
            MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromRect(bounds,MONITOR_DEFAULTTONEAREST),&monitor);
            LONG width=std::min(bounds->right-bounds->left,monitor.rcWork.right-monitor.rcWork.left);
            LONG height=std::min(bounds->bottom-bounds->top,monitor.rcWork.bottom-monitor.rcWork.top);
            LONG x=std::clamp(bounds->left,monitor.rcWork.left,monitor.rcWork.right-width);
            LONG y=std::clamp(bounds->top,monitor.rcWork.top,monitor.rcWork.bottom-height);
            SetWindowPos(window,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
            settings_layout();return 0;
        }
        case WM_SETTINGCHANGE:case WM_THEMECHANGED:refresh_theme(true);settings_theme();return 0;
        case WM_KEYDOWN:if(wp==VK_ESCAPE){if(!(lp&(LPARAM(1)<<30)))DestroyWindow(window);return 0;}break;
        case WM_CLOSE:DestroyWindow(window);return 0;
        case WM_DESTROY:
            settings=nullptr;settingsEditingId.clear();settingsClearId.clear();settingsVisible.clear();settingsVisible.shrink_to_fit();settingsCommands.clear();settingsCommands.shrink_to_fit();
            if(settingsRowHeight){ImageList_Destroy(settingsRowHeight);settingsRowHeight=nullptr;}
            if(settingsHeadingFont){DeleteObject(settingsHeadingFont);settingsHeadingFont=nullptr;}
            if(settingsBackgroundBrush){DeleteObject(settingsBackgroundBrush);settingsBackgroundBrush=nullptr;}
            if(settingsFieldBrush){DeleteObject(settingsFieldBrush);settingsFieldBrush=nullptr;}
            return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
