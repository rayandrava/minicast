D2D1_RECT_F popup_bounds() const {
    float width=popup==2?238.f:284.f,height=38+float(popupActions.size())*32;
    float x=PALETTE_WIDTH-width-14,y=popup==2?73.f:FOOTER_TOP-height-8;
    return D2D1::RectF(x,y,x+width,y+height);
}
void paint_popup() {
    auto r=popup_bounds();float width=r.right-r.left,height=r.bottom-r.top;
    rect(r.left-4,r.top-2,width+8,height+8,color(0,0,0,light?.08f:.25f),10);
    rect(r.left,r.top,width,height,light?color(250,251,254):color(41,41,47),8);outline(r.left,r.top,width,height,rule(),8);
    text(popup==2?L"Categories":L"Actions",r.left+12,r.top+9,width-24,20,small.p,muted());
    for(size_t i=0;i<popupActions.size();++i){float y=r.top+32+float(i)*32;
        if(int(i)==popupSelected)rect(r.left+5,y,width-10,30,selection(),4);
        text(popupActions[i].name,r.left+12,y+5,width-80,22,body.p,ink());
        aligned(popupActions[i].shortcut,r.right-77,y+6,65,21,small.p,muted(),DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
}
void open_actions() {
    update_query();if(popup==1){popup=0;invalidate();return;}
    popup=1;popupSelected=0;popupActions.clear();
    auto primary=primary_label();if(!primary.empty())popupActions.push_back({primary,L"Enter",0});
    if(mode==Mode::Camera){popupActions.push_back({L"Copy Snapshot",L"Ctrl ↵",1});popupActions.push_back({mirrored?L"Turn Mirroring Off":L"Turn Mirroring On",L"",4});popupActions.push_back({L"Next Camera",L"",5});}
    else if((emoji_view()||clipboard_view())&&!primary.empty())popupActions.push_back({L"Copy",L"Ctrl ↵",1});
    if(clipboard_view()&&selected_history())popupActions.push_back({L"Delete Entry",L"",2});
    if(selected<int(rows.size())&&rows[selected].action==Action::App)popupActions.push_back({L"Assign Hotkey",L"",8});
    popupActions.push_back({L"Settings",L"Ctrl ,",3});invalidate();
}
void open_categories() {
    update_query();if(popup==2){popup=0;invalidate();return;}
    popup=2;popupSelected=emojiCategory+1;popupActions={{L"All Categories",L"",100}};
    for(int i=0;i<emoji_category_count;++i)popupActions.push_back({std::wstring(emoji_category_name(EmojiCategory(i))),L"",101+i});invalidate();
}
void run_popup_action() {
    if(!popup||popupSelected<0||popupSelected>=int(popupActions.size()))return;
    int command=popupActions[popupSelected].command;popup=0;invalidate();
    if(command>=100){emojiCategory=command-101;selected=scroll=0;gridOffset=0;rebuild(false);return;}
    if(command==0){activate();return;}if(command==1){activate(true);return;}
    if(command==2){if(auto item=selected_history()){auto identity=item->timestamp;history.erase(std::remove_if(history.begin(),history.end(),[&](const auto& entry){return entry.timestamp==identity;}),history.end());queue_history(L"Entry deleted.",L"Couldn't save the deletion yet.");rebuild();notify(L"Deleting entry…");}return;}
    if(command==4){camera_click(2);return;}if(command==5){camera_click(3);return;}
    std::wstring commandId;
    if(command==8&&selected<int(rows.size())&&rows[selected].action==Action::App)commandId=L"app:"+rows[selected].value;
    open_settings();if(!commandId.empty())settings_select_command(commandId);
}
void go_back() {
    if(popup){popup=0;invalidate();return;}
    if(mode!=Mode::Home)set_mode(Mode::Home);else hide();
}
bool draggable_at(float x,float y) const {
    if(x<0||x>=PALETTE_WIDTH||y<0||y>=PALETTE_HEIGHT||popup)return false;
    if(y<65){
        if(mode!=Mode::Home&&x<42)return false;
        if((emoji_view()||clipboard_view())&&x>=537)return false;
        if(!emoji_view()&&!clipboard_view()&&x>=700)return false;
        // Keep normal caret placement, selection and IME inside the EDIT.
        if(x>=49&&x<49+editLayoutWidth&&y>=20&&y<49)return false;
        return true;
    }
    if(y>=FOOTER_TOP)return x<435;
    if(mode==Mode::Camera||emoji_view()||(clipboard_view()&&x>=283))return false;
    return row_at(x,y)<0;
}
void click_palette(float x,float y,bool doubleClick=false) {
    update_query();
    if(popup){auto r=popup_bounds();if(x>=r.left&&x<r.right&&y>=r.top+32&&y<r.bottom-6){popupSelected=int((y-r.top-32)/32);run_popup_action();}else{popup=0;invalidate();}return;}
    if(y<65){if(x<42&&mode!=Mode::Home)go_back();else if(x>=537&&emoji_view())open_categories();else if(x>=700&&!clipboard_view())open_settings();return;}
    if(y>=FOOTER_TOP){if(x>=637)open_actions();else if(x>=435&&!primary_label().empty())activate();return;}
    if(mode==Mode::Camera){if(y>=375&&y<=407){int button=int((x-16)/182);if(x>=16&&button>=0&&button<4)camera_click(button);}return;}
    int index=row_at(x,y);if(index<0)return;selected=index;hover=-1;invalidate();
    // The clipboard list selects the preview; Enter, double-click or the footer pastes.
    if(!clipboard_view()||doubleClick)activate();
}
void move_grid_vertical(int direction) {
    update_query();if(grid.empty())return;
    auto current=grid[selected].bounds;int best=-1;float distance=1e9f;
    for(const auto& tile:grid){float dy=tile.bounds.top-current.top;if(dy*direction<=0)continue;float score=std::abs(dy)*1000+std::abs(tile.bounds.left-current.left);if(score<distance){distance=score;best=tile.row;}}
    if(best>=0)selected=best;ensure_selection_visible();hover=-1;invalidate();
}
void wheel(int delta) {
    if(popup){popupSelected=std::clamp(popupSelected+(delta>0?-1:1),0,int(popupActions.size())-1);invalidate();return;}
    if(emoji_view()){gridOffset=std::clamp(gridOffset+(delta>0?-91.f:91.f),0.f,std::max(0.f,gridHeight-FOOTER_TOP));hover=-1;invalidate();return;}
    move(delta>0?-3:3);
}
