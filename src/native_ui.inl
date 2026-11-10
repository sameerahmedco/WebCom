// Native browser chrome. All coordinates are device-independent pixels;
// the reference layout is 40 px of tabs, 44 px of tools and 37 px of bookmarks.
// Included by app.cpp after BrowserApp::Impl is defined.

void BrowserApp::Impl::Fonts() {
    for (HFONT font : {font_, small_font_, logo_font_, heading_font_}) if (font) DeleteObject(font);
    auto create = [&](int height, int weight) {
        return CreateFontW(-D(height), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    };
    font_ = create(14, FW_NORMAL);
    small_font_ = create(12, FW_NORMAL);
    logo_font_ = create(88, FW_NORMAL);
    heading_font_ = create(22, FW_MEDIUM);
    // Appearance changes refresh the actual edit backgrounds, not just the
    // painted shell around them. The saved DCs never retain these brushes.
    for (HBRUSH brush : {edit_brush_,idle_edit_brush_,search_brush_,toast_brush_})
        if(brush)DeleteObject(brush);
    edit_brush_=CreateSolidBrush(kFrame);idle_edit_brush_=CreateSolidBrush(kSurface);
    search_brush_=CreateSolidBrush(kSearch);toast_brush_=CreateSolidBrush(kSurface);
    native_chrome_.bookmark_widths.clear();
    for (HWND h : {address_, home_search_, native_filter_, tab_strip_, toast_, suggestion_panel_})
        if (h) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    for (const auto* group : {&toolbar_, &bookmark_buttons_})
        for (HWND h : *group) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    for (HWND h : home_buttons_) if (h) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
}

HWND BrowserApp::Impl::Button(UINT id, const wchar_t* label) {
    HWND h = CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 1, 1, window_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), instance_, nullptr);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SetWindowSubclass(h, ButtonProc, 1, reinterpret_cast<DWORD_PTR>(this));
    if (tooltips_) {
        TOOLINFOW tool{};tool.cbSize=sizeof(tool);
        tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = window_; tool.uId = reinterpret_cast<UINT_PTR>(h);
        tool.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(tooltips_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
    }
    return h;
}

void BrowserApp::Impl::Controls() {
    // Native Win32 tooltip popups can flash at (0,0) while chrome controls are
    // being repositioned. WebCom keeps control names for accessibility and uses
    // the dark menu/labels for discoverability instead of a separate topmost popup.
    tooltips_ = nullptr;
    tab_strip_ = CreateWindowExW(0, L"STATIC", L"Browser tabs. Use Ctrl+Tab to switch tabs.",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_NOTIFY, 0, 0, 1, 1, window_,
        reinterpret_cast<HMENU>(TabStrip), instance_, nullptr);
    SetWindowSubclass(tab_strip_, TabProc, 1, reinterpret_cast<DWORD_PTR>(this));
    for (const auto& [id, label] : std::initializer_list<std::pair<UINT, const wchar_t*>>{
        {TabList,L"Search open tabs"}, {NewTab,L"New tab (Ctrl+T)"},
        {MinimizeWindow,L"Minimize"}, {MaximizeWindow,L"Maximize or restore"}, {CloseWindow,L"Close window"},
        {Back,L"Back (Alt+Left)"}, {Forward,L"Forward (Alt+Right)"}, {Reload,L"Reload (Ctrl+R)"},
        {AiMode,L"AI Mode - open Google AI Mode"}, {BookmarkPage,L"Bookmark this page (Ctrl+D)"},
        {SiteInfo,L"Site information and permissions"}, {DownloadsQuick,L"Recent downloads"},
        {Profile,L"Profiles"}, {More,L"Customize and control WebCom"}, {BookmarksOverflow,L"More bookmarks"}})
        toolbar_.push_back(Button(id,label));
    address_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0,0,1,1,window_,reinterpret_cast<HMENU>(Address),instance_,nullptr);
    home_search_ = CreateWindowExW(0,L"EDIT",L"",WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0,0,1,1,window_,reinterpret_cast<HMENU>(HomeSearch),instance_,nullptr);
    native_filter_ = CreateWindowExW(0,L"EDIT",L"",WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0,0,1,1,window_,reinterpret_cast<HMENU>(NativeFilter),instance_,nullptr);
    suggestion_panel_ = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"STATIC", L"",
        WS_CHILD | SS_NOTIFY, 0,0,1,1,window_,nullptr,instance_,nullptr);
    SetWindowSubclass(suggestion_panel_, SuggestionProc, 1, reinterpret_cast<DWORD_PTR>(this));
    for (HWND h : {address_,home_search_,native_filter_}) {
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
        SendMessageW(h,EM_SETLIMITTEXT,16384,0);
        SendMessageW(h,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(0,0));
        SetWindowSubclass(h,EditProc,1,reinterpret_cast<DWORD_PTR>(this));
        // The cue is painted in our own dark palette when the native edit is empty.
    }
    home_buttons_ = {Button(HomeGo,L"Search Google"),Button(Google,L"Google"),
        Button(YouTube,L"YouTube"),Button(GitHub,L"GitHub")};
    toast_ = CreateWindowExW(0,L"STATIC",L"",WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
        0,0,1,1,window_,nullptr,instance_,nullptr);
    SendMessageW(toast_,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
}

RECT BrowserApp::Impl::ContentBounds() const {
    RECT r{}; GetClientRect(window_,&r);
    r.top = ContentTop(); r.bottom = std::max(r.bottom,r.top);
    return r;
}

void BrowserApp::Impl::Layout() {
    if (!window_ || !address_ || IsIconic(window_)) return;
    auto& cache=native_chrome_;
    if (cache.layout_running) { cache.layout_again=true;return; }
    cache.layout_running=true;
    struct ResetLayout { bool& value; ~ResetLayout(){value=false;} } reset{cache.layout_running};
    for (int pass=0;pass<2;++pass) {
        cache.layout_again=false;
        RECT client{};GetClientRect(window_,&client);
        const int width=static_cast<int>(client.right);
        if (width<=0 || client.bottom<=0) return;
        const int dip_width=MulDiv(width,96,static_cast<int>(dpi_));
        const bool chrome=!fullscreen_;
        const auto* tab=Active();
        const bool focused=GetFocus()==address_;
        const bool native_home=tab && IsHome(tab->url) && !tab->creating && !tab->web;
        NativeLayoutBatch batch(window_,cache.placements);
        auto place=[&](UINT id,int x,int y,int w,int h,bool visible=true) {
            batch.Place(GetDlgItem(window_,id),D(x),D(y),D(w),D(h),chrome && visible);
        };
        const auto visible_indices=VisibleTabIndices();
        std::size_t display_active=0;
        if(const auto found=std::find(visible_indices.begin(),visible_indices.end(),active_);found!=visible_indices.end())
            display_active=static_cast<std::size_t>(found-visible_indices.begin());
        std::size_t pinned_count=0;
        while(pinned_count<visible_indices.size() && tabs_[visible_indices[pinned_count]]->activity.pinned) ++pinned_count;
        const auto strip=MakeChromeTabStripPlan(dip_width,visible_indices.size(),pinned_count,display_active,first_visible_tab_);
        first_visible_tab_=strip.first_unpinned;
        tab_rects_.assign(tabs_.size(),RECT{});
        cache.tab_close_rects.assign(tabs_.size(),RECT{});
        for (const auto& slot : strip.slots) {
            if(slot.index>=visible_indices.size())continue;
            const auto actual=visible_indices[slot.index];
            tab_rects_[actual]={D(slot.left),D(6),D(slot.right),D(40)};
            if(!slot.pinned) cache.tab_close_rects[actual]={D(slot.right-28),D(10),D(slot.right-4),D(34)};
        }
        batch.Place(tab_strip_,D(strip.strip_left),0,D(strip.strip_width),D(40),chrome && !strip.slots.empty());
        place(TabList,6,6,28,28);place(NewTab,strip.new_tab_left,4,32,32);
        place(MinimizeWindow,dip_width-138,0,46,40);
        place(MaximizeWindow,dip_width-92,0,46,40);
        place(CloseWindow,dip_width-46,0,46,40);
        const int profile_width=static_cast<int>(std::min<std::size_t>(140,48+profile_name_.size()*7));
        const auto tools=MakeChromeToolbarLayout(dip_width,profile_width,native_home,!download_entries_.empty());
        place(Back,6,47,32,32);place(Forward,43,47,32,32,tools.show_forward);
        place(Reload,tools.show_forward?79:43,47,32,32);
        cache.omnibox=chrome?RECT{D(tools.omnibox_left),D(45),D(tools.omnibox_right),D(81)}:RECT{};
        batch.Place(address_,D(tools.edit_left),D(53),D(tools.edit_right-tools.edit_left),D(23),chrome);
        place(SiteInfo,tools.omnibox_left+4,49,28,28,tab && IsWebAddress(tab->url));
        place(AiMode,tools.action_left,51,82,24,tools.show_ai);
        place(BookmarkPage,tools.omnibox_right-34,49,28,28,tab && !IsInternalPage(tab->url));
        place(DownloadsQuick,tools.download_left,47,34,32,tools.show_download);
        place(Profile,tools.profile_left,46,tools.profile_width,34);
        place(More,tools.more_left,46,34,34);
        if (cache.dpi!=dpi_ || cache.bookmark_widths.size()!=bookmark_buttons_.size()) {
            cache.bookmark_widths.clear();
            HDC dc=GetDC(window_);const auto previous=SelectObject(dc,small_font_);
            for (HWND button:bookmark_buttons_) {
                SIZE size{};const auto title=ControlText(button);
                GetTextExtentPoint32W(dc,title.c_str(),static_cast<int>(title.size()),&size);
                cache.bookmark_widths.push_back(std::clamp(static_cast<int>(size.cx)+D(34),D(66),D(216)));
            }
            SelectObject(dc,previous);ReleaseDC(window_,dc);
        }
        int x=D(8);bool overflow=false;
        for (std::size_t i=0;i<bookmark_buttons_.size();++i) {
            const int w=cache.bookmark_widths[i];
            const bool fits=!overflow && x+w<=width-D(38);
            batch.Place(bookmark_buttons_[i],x,D(87),w,D(28),chrome && show_bookmarks_ && fits);
            if(fits)x+=w+D(2);else overflow=true;
        }
        place(BookmarksOverflow,dip_width-34,87,28,28,show_bookmarks_ && overflow);
        const RECT content=ContentBounds();
        const auto internal_page=tab?InternalPageFor(tab->url):InternalPage::None;
        const bool searchable_native=tab && !tab->web && (internal_page==InternalPage::History || internal_page==InternalPage::Bookmarks);
        if(searchable_native && native_filter_page_!=internal_page){
            native_filter_page_=internal_page; SetWindowTextW(native_filter_,L""); native_scroll_=0;
            SendMessageW(native_filter_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(
                internal_page==InternalPage::History?L"Search history":L"Search bookmarks"));
        } else if(!searchable_native) native_filter_page_=InternalPage::None;
        const int page_width_dip=std::min(980,std::max(320,dip_width-48));
        const int page_left_dip=(dip_width-page_width_dip)/2;
        batch.Place(native_filter_,D(page_left_dip),content.top+D(98),D(std::min(460,page_width_dip)),D(32),chrome && searchable_native);
        const int dip_height=MulDiv(static_cast<int>(content.bottom-content.top),96,static_cast<int>(dpi_));
        if (tab && tab->private_mode && native_home) {
            const auto private_layout=MakeChromePrivateLayout(dip_width,dip_height);
            private_scroll_=std::clamp(private_scroll_,0,std::max(0,D(private_layout.extent)-static_cast<int>(content.bottom-content.top)));
        } else private_scroll_=0;
        const bool home=native_home && !tab->private_mode;
        const bool failed=tab && tab->failed && !tab->web && !IsHome(tab->url);
        const auto home_layout=MakeChromeHomeLayout(dip_width,dip_height);
        const int left=D(home_layout.search_left),top=content.top+D(home_layout.search_top);
        cache.home_search_rect=home?RECT{left,top,left+D(home_layout.search_width),top+D(52)}:RECT{};
        batch.Place(home_search_,left+D(48),top+D(14),D(home_layout.search_width-103),D(24),home);
        if (home) {
            if(ControlText(home_buttons_[0])!=L"Search Google")SetWindowTextW(home_buttons_[0],L"Search Google");
            batch.Place(home_buttons_[0],left+D(home_layout.search_width-44),top+D(8),D(36),D(36),true);
        } else {
            if(failed && ControlText(home_buttons_[0])!=L"Reload tab")SetWindowTextW(home_buttons_[0],L"Reload tab");
            batch.Place(home_buttons_[0],width/2-D(68),content.top+D(std::max(270,std::min(350,dip_height-64))),D(136),D(38),failed);
        }
        const int shortcut_width=dip_width<420?92:104;
        for(int i=1;i<4;++i)batch.Place(home_buttons_[i],width/2-D(shortcut_width*3)/2+D(shortcut_width)*(i-1),
            content.top+D(home_layout.shortcut_top),D(shortcut_width),D(92),home);
        if (toast_ && GetTickCount64()<note_until_) {
            const int toast_width=std::min(D(620),width-D(48));
            batch.Place(toast_,(width-toast_width)/2,static_cast<int>(client.bottom)-D(54),toast_width,D(36),true);
        }
        const bool size_changed=!EqualRect(&cache.client,&client) || cache.dpi!=dpi_;
        const bool content_changed=!EqualRect(&cache.content,&content);
        const bool focus_changed=cache.address_focused!=focused;
        const int native_page=!tab?0:tab->web?0:
            internal_page==InternalPage::NewTab?(tab->private_mode?2:1):
            internal_page==InternalPage::History?5:
            internal_page==InternalPage::Downloads?6:
            internal_page==InternalPage::Settings?7:
            internal_page==InternalPage::Bookmarks?8:
            internal_page==InternalPage::Profiles?9:
            tab->failed?4:3;
        const auto tab_id=tab?tab->id:0;
        const bool page_changed=cache.native_page!=native_page || (native_page && cache.native_tab!=tab_id);
        if(native_page==0 && cache.native_page!=0)cache.frame.Reset();
        cache.client=client;cache.content=content;cache.dpi=dpi_;
        cache.address_focused=focused;cache.native_page=native_page;cache.native_tab=tab_id;
        batch.Commit();
        if(suggestion_panel_ && IsWindowVisible(suggestion_panel_)) {
            const int suggestion_width=std::max(D(320),static_cast<int>(cache.omnibox.right-cache.omnibox.left));
            const int suggestion_height=D(10)+static_cast<int>(omnibox_suggestions_.size())*D(52);
            SetWindowPos(suggestion_panel_,HWND_TOP,cache.omnibox.left,cache.omnibox.bottom+D(4),suggestion_width,suggestion_height,
                SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOREDRAW);
            InvalidateRect(suggestion_panel_,nullptr,FALSE);
        }
        if(toast_ && GetTickCount64()<note_until_ && GetWindow(toast_,GW_HWNDPREV)) {
            SetWindowPos(toast_,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOREDRAW);
            InvalidateRect(toast_,nullptr,FALSE);
        }
        if (content_changed)if(auto* current=Active();current && current->controller)SetVisible(*current,true);
        if (size_changed || content_changed) InvalidateRect(window_,nullptr,FALSE);
        else if (page_changed || native_page>=3) InvalidateRect(window_,&content,FALSE);
        if (focus_changed) {
            InvalidateRect(window_,&cache.omnibox,FALSE);InvalidateRect(address_,nullptr,FALSE);
            InvalidateRect(GetDlgItem(window_,AiMode),nullptr,FALSE);
            InvalidateRect(GetDlgItem(window_,BookmarkPage),nullptr,FALSE);
        }
        if (!cache.layout_again)break;
    }
}

namespace {
COLORREF MixChromeColor(COLORREF base,COLORREF tint,unsigned percent) {
    const unsigned rest=100-std::min(percent,100u);percent=100-rest;
    return RGB((GetRValue(base)*rest+GetRValue(tint)*percent)/100,
               (GetGValue(base)*rest+GetGValue(tint)*percent)/100,
               (GetBValue(base)*rest+GetBValue(tint)*percent)/100);
}
void ChromeGoogleMark(HDC dc,RECT r) {
    using namespace Gdiplus;
    Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.TranslateTransform(static_cast<REAL>(r.left),static_cast<REAL>(r.top));
    g.ScaleTransform(static_cast<REAL>(r.right-r.left)/24,static_cast<REAL>(r.bottom-r.top)/24);
    const Color blue(255,138,180,248),red(255,242,139,130),yellow(255,253,214,99),green(255,129,201,149);
    Pen p(blue,3.6f);
    g.DrawArc(&p,4.0f,4.0f,16.0f,16.0f,-12.0f,62.0f);
    p.SetColor(green);g.DrawArc(&p,4.0f,4.0f,16.0f,16.0f,50.0f,95.0f);
    p.SetColor(yellow);g.DrawArc(&p,4.0f,4.0f,16.0f,16.0f,145.0f,75.0f);
    p.SetColor(red);g.DrawArc(&p,4.0f,4.0f,16.0f,16.0f,220.0f,95.0f);
    p.SetColor(blue);g.DrawLine(&p,12.0f,12.0f,20.0f,12.0f);
}
void ChromeYouTubeMark(HDC dc,RECT r) {
    Rounded(dc,r,RGB(235,68,76),RGB(235,68,76),std::max(2,static_cast<int>(r.bottom-r.top)/2));
    using namespace Gdiplus;
    Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);
    const REAL x=static_cast<REAL>(r.left),y=static_cast<REAL>(r.top);
    const REAL w=static_cast<REAL>(r.right-r.left),h=static_cast<REAL>(r.bottom-r.top);
    const PointF points[]={{x+w*.41f,y+h*.27f},{x+w*.41f,y+h*.73f},{x+w*.70f,y+h*.50f}};
    SolidBrush ink(Color(255,255,255,255));g.FillPolygon(&ink,points,3);
}
}

void BrowserApp::Impl::PaintInternalPage(HDC dc, const RECT& content, Tab& tab) {
    native_actions_.clear();
    const auto page = InternalPageFor(tab.url);
    if (page == InternalPage::None || page == InternalPage::NewTab) { native_page_extent_ = 0; return; }
    const int content_width = static_cast<int>(content.right - content.left);
    const int page_width = std::min(D(980), std::max(D(320), content_width - D(48)));
    const int left = content.left + (content_width - page_width) / 2;
    const int right = left + page_width;
    const int origin = content.top - native_scroll_;
    int y = origin + D(34);
    const auto add_action = [&](RECT rect, UINT command) {
        if (rect.bottom > content.top && rect.top < content.bottom)
            native_actions_.push_back({rect, command});
    };
    const auto draw_button = [&](RECT rect, const std::wstring& label, UINT command) {
        if (rect.bottom <= content.top || rect.top >= content.bottom) return;
        const bool hot = hot_native_command_ == command;
        Rounded(dc, rect, hot ? MixChromeColor(kSurface, kAccent, 12) : kSurface,
                hot ? MixChromeColor(kBorder, kAccent, 34) : kBorder, D(22));
        Text(dc, label, rect, small_font_, hot ? kInk : kMuted,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        add_action(rect, command);
    };
    const auto draw_header = [&](const std::wstring& title, const std::wstring& subtitle) {
        Text(dc, title, {left, y, right, y + D(42)}, heading_font_, kInk,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += D(46);
        Text(dc, subtitle, {left, y, right, y + D(28)}, font_, kMuted,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        y += D(48);
    };
    const std::wstring filter_text = native_filter_ && IsWindowVisible(native_filter_) ? ControlText(native_filter_) : L"";
    const auto contains_ci = [](std::wstring_view haystack, std::wstring_view needle) {
        if (needle.empty()) return true;
        return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
            [](wchar_t a, wchar_t b){ return towlower(a) == towlower(b); }) != haystack.end();
    };

    if (page == InternalPage::History) {
        draw_header(L"History", L"Pages visited in this WebCom profile. Incognito activity is never added here.");
        draw_button({right - D(170), origin + D(34), right, origin + D(68)}, L"Clear history", ClearHistory);
        y += D(42);
        if(!filter_text.empty()) draw_button({left, y, left + D(142), y + D(32)}, L"Delete matches", HistoryDeleteMatches);
        draw_button({left + D(150), y, left + D(296), y + D(32)}, L"Delete last 7 days", HistoryDeleteWeek);
        draw_button({left + D(304), y, left + D(486), y + D(32)}, L"Delete older than 30d", HistoryDeleteOlderMonth);
        y += D(48);
        std::size_t matches = 0;
        for (const auto& item : history_entries_)
            if (HistoryMatches(item,filter_text)) ++matches;
        if (history_entries_.empty() || matches==0) {
            const RECT card{left, y, right, y + D(150)};
            if (card.bottom > content.top && card.top < content.bottom) {
                Rounded(dc, card, kSurface, kBorder, D(18));
                Icon(dc, {card.left + D(28), card.top + D(28), card.left + D(54), card.top + D(54)}, Glyph::Globe, kMuted);
                const bool filtered=!filter_text.empty() && !history_entries_.empty();
                Text(dc, filtered?L"No matching history":L"No browsing history yet",
                     {card.left + D(72), card.top + D(18), card.right - D(24), card.top + D(54)},
                     font_, kInk, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                Text(dc, filtered?L"Try a different page title or address.":L"Normal web pages you visit will appear here. Private tabs stay private.",
                     {card.left + D(72), card.top + D(54), card.right - D(24), card.bottom - D(18)}, small_font_, kMuted,
                     DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            }
            y = card.bottom + D(24);
        } else {
            const int row_height = D(66), heading_height=D(34);
            std::wstring group; int cursor=y;
            for (std::size_t i = 0; i < history_entries_.size(); ++i) {
                const auto& item=history_entries_[i];
                if(!HistoryMatches(item,filter_text)) continue;
                const auto label=HistoryDateLabel(item.visited_ms);
                if(label!=group) {
                    group=label; const RECT heading{left,cursor,right,cursor+heading_height};
                    if(heading.bottom>content.top && heading.top<content.bottom)
                        Text(dc,group,heading,font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                    cursor+=heading_height;
                }
                const RECT row{left,cursor,right,cursor+row_height-D(4)}; cursor+=row_height;
                if (row.bottom <= content.top || row.top >= content.bottom) continue;
                const UINT command = HistoryBase + static_cast<UINT>(i);
                const bool hot = hot_native_command_ == command;
                Rounded(dc, row, hot ? kHover : kSurface, hot ? MixChromeColor(kBorder, kAccent, 25) : kBorder, D(14));
                Icon(dc, {row.left + D(16), row.top + D(18), row.left + D(38), row.top + D(40)}, Glyph::Globe, kMuted);
                const auto title = item.title.empty() ? item.url : item.title;
                Text(dc, title, {row.left + D(50), row.top + D(7), row.right - D(108), row.top + D(31)},
                     font_, kInk, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                Text(dc, item.url, {row.left + D(50), row.top + D(31), row.right - D(108), row.bottom - D(7)},
                     small_font_, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                Text(dc, HistoryTimeLabel(item.visited_ms), {row.right - D(142), row.top, row.right - D(50), row.bottom},
                     small_font_, kMuted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                add_action(row, command);
                const RECT remove{row.right-D(42),row.top+D(13),row.right-D(10),row.bottom-D(13)};
                if(hot_native_command_==DeleteHistoryBase+static_cast<UINT>(i))Rounded(dc,remove,kHover,kHover,D(16));
                RECT remove_icon=remove;InflateRect(&remove_icon,-D(8),-D(8));Icon(dc,remove_icon,Glyph::Close,kMuted);
                add_action(remove,DeleteHistoryBase+static_cast<UINT>(i));
            }
            y=cursor+D(16);
        }
    } else if (page == InternalPage::Downloads) {
        for (auto& entry : download_entries_) RefreshDownload(*entry);
        draw_header(L"Downloads", L"Pause, resume, cancel, reopen and review downloads for this profile.");
        draw_button({right - D(188), origin + D(34), right, origin + D(68)}, L"Open Downloads folder", OpenDownloadsFolder);
        draw_button({right - D(344), origin + D(34), right - D(198), origin + D(68)}, L"Clear completed", ClearDownloadsList);
        if (download_entries_.empty()) {
            const RECT card{left, y, right, y + D(150)};
            if (card.bottom > content.top && card.top < content.bottom) {
                Rounded(dc, card, kSurface, kBorder, D(18));
                Icon(dc, {card.left + D(28), card.top + D(28), card.left + D(54), card.top + D(54)}, Glyph::Down, kMuted);
                Text(dc, L"No downloads yet", {card.left + D(72), card.top + D(18), card.right - D(24), card.top + D(54)},
                     font_, kInk, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                Text(dc, L"Downloads started from web pages will appear here and remain in this profile’s list.",
                     {card.left + D(72), card.top + D(54), card.right - D(24), card.bottom - D(18)}, small_font_, kMuted,
                     DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            }
            y = card.bottom + D(24);
        } else {
            const int row_height = D(96);
            for (std::size_t i = 0; i < download_entries_.size(); ++i) {
                const RECT row{left, y + static_cast<int>(i) * row_height, right,
                               y + static_cast<int>(i + 1) * row_height - D(6)};
                if (row.bottom <= content.top || row.top >= content.bottom) continue;
                auto& item = *download_entries_[i];
                const UINT command = DownloadBase + static_cast<UINT>(i);
                const bool hot = hot_native_command_ == command;
                Rounded(dc, row, hot ? kHover : kSurface, hot ? MixChromeColor(kBorder, kAccent, 25) : kBorder, D(14));
                Icon(dc, {row.left + D(17), row.top + D(20), row.left + D(41), row.top + D(44)}, Glyph::Down,
                     item.state == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED ? kAccent : kMuted);
                std::wstring name = item.path.empty() ? item.uri : std::filesystem::path(item.path).filename().wstring();
                if (name.empty()) name = L"Download";
                Text(dc, name, {row.left + D(52), row.top + D(8), row.right - D(230), row.top + D(32)},
                     font_, kInk, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                std::wstring status = item.paused ? L"Paused" : DownloadStateLabel(item.state);
                if (item.state == COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS) {
                    status += L" • " + FormatBytes(item.received);
                    if (item.total > 0) status += L" of " + FormatBytes(item.total);
                } else if (item.total > 0) status += L" • " + FormatBytes(item.total);
                Text(dc, status, {row.left + D(52), row.top + D(33), row.right - D(230), row.top + D(56)},
                     small_font_, item.paused ? kAccent : kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                Text(dc, HistoryTimeLabel(item.started_ms), {row.right - D(150), row.top + D(8), row.right - D(14), row.top + D(32)},
                     small_font_, kMuted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                if ((item.state == COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS || item.paused) && item.total > 0) {
                    const RECT track{row.left + D(52), row.bottom - D(18), row.right - D(230), row.bottom - D(13)};
                    Rounded(dc, track, kBorder, kBorder, D(5));
                    const auto clamped = std::clamp<std::int64_t>(item.received, 0, item.total);
                    const int fill = static_cast<int>((track.right - track.left) * clamped / std::max<std::int64_t>(1, item.total));
                    if (fill > 0) Rounded(dc, {track.left, track.top, track.left + fill, track.bottom}, kAccent, kAccent, D(5));
                }
                add_action(row, command);

                const auto small_action = [&](RECT rect, std::wstring_view label, UINT action, bool accent = false) {
                    const bool action_hot = hot_native_command_ == action;
                    Rounded(dc, rect,
                            action_hot ? MixChromeColor(kSurface, kAccent, 18) : kFrame,
                            action_hot || accent ? MixChromeColor(kBorder, kAccent, 45) : kBorder, D(16));
                    Text(dc, std::wstring(label), rect, small_font_, accent ? kAccent : (action_hot ? kInk : kMuted),
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    add_action(rect, action);
                };
                const int button_top = row.top + D(43);
                if (item.state == COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS || item.paused) {
                    small_action({row.right-D(218),button_top,row.right-D(116),button_top+D(32)},
                                 item.paused ? L"Resume" : L"Pause",
                                 PauseDownloadBase + static_cast<UINT>(i), item.paused);
                    small_action({row.right-D(108),button_top,row.right-D(14),button_top+D(32)},
                                 L"Cancel", CancelDownloadBase + static_cast<UINT>(i));
                } else if (item.state == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED) {
                    small_action({row.right-D(108),button_top,row.right-D(14),button_top+D(32)},
                                 L"Open", command, true);
                } else {
                    small_action({row.right-D(108),button_top,row.right-D(14),button_top+D(32)},
                                 L"Retry", RetryDownloadBase + static_cast<UINT>(i), true);
                }
            }
            y += static_cast<int>(download_entries_.size()) * row_height + D(16);
        }
    } else if (page == InternalPage::Bookmarks) {
        draw_header(L"Bookmarks", L"Saved pages in the " + Wide(profile_name_) + L" profile. Folders and HTML import/export stay local to this profile.");
        draw_button({right-D(104),origin+D(34),right,origin+D(68)},L"Export",ExportBookmarks);
        draw_button({right-D(216),origin+D(34),right-D(112),origin+D(68)},L"Import",ImportBookmarks);
        y += D(42);
        std::size_t matches=0;
        for(const auto& b:bookmarks_) if(contains_ci(Wide(b.title),filter_text)||contains_ci(Wide(b.url),filter_text)||contains_ci(Wide(b.folder),filter_text)) ++matches;
        if (bookmarks_.empty() || matches==0) {
            const RECT card{left, y, right, y + D(150)};
            if (card.bottom > content.top && card.top < content.bottom) {
                Rounded(dc, card, kSurface, kBorder, D(18));
                Icon(dc, {card.left + D(28), card.top + D(28), card.left + D(54), card.top + D(54)}, Glyph::Star, kMuted);
                const bool filtered=!filter_text.empty() && !bookmarks_.empty();
                Text(dc, filtered?L"No matching bookmarks":L"No bookmarks yet", {card.left + D(72), card.top + D(18), card.right - D(24), card.top + D(54)},
                     font_, kInk, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                Text(dc, filtered?L"Try another title, address, or folder name.":L"Save a page with Ctrl+D or import bookmark HTML from another browser.",
                     {card.left + D(72), card.top + D(54), card.right - D(24), card.bottom - D(18)}, small_font_, kMuted,
                     DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            }
            y = card.bottom + D(24);
        } else {
            const int row_height=D(72), heading_height=D(30); int cursor=y; std::wstring folder_marker=L"\x01";
            for(std::size_t i=0;i<bookmarks_.size();++i) {
                const auto title=Wide(bookmarks_[i].title),url=Wide(bookmarks_[i].url),folder=Wide(bookmarks_[i].folder);
                if(!contains_ci(title,filter_text)&&!contains_ci(url,filter_text)&&!contains_ci(folder,filter_text)) continue;
                if(folder!=folder_marker){folder_marker=folder;if(!folder.empty()){
                    const RECT heading{left,cursor,right,cursor+heading_height};
                    if(heading.bottom>content.top&&heading.top<content.bottom)Text(dc,folder,heading,font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                    cursor+=heading_height;
                }}
                const RECT row{left,cursor,right,cursor+row_height-D(6)};cursor+=row_height;
                if(row.bottom<=content.top || row.top>=content.bottom) continue;
                const UINT open_command=BookmarkBase+static_cast<UINT>(i),remove_command=RemoveBookmarkBase+static_cast<UINT>(i);
                const bool hot=hot_native_command_==open_command;
                const bool dragging=bookmark_dragging_ && bookmark_drag_source_==static_cast<int>(i);
                const bool target=bookmark_dragging_ && bookmark_drag_target_==static_cast<int>(i);
                const COLORREF border=target?MixChromeColor(kBorder,kAccent,62):(hot?MixChromeColor(kBorder,kAccent,25):kBorder);
                Rounded(dc,row,dragging?MixChromeColor(kSurface,kAccent,8):(hot?kHover:kSurface),border,D(14));
                Text(dc,L"\u2261",{row.left+D(9),row.top+D(16),row.left+D(31),row.bottom-D(16)},font_,kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                Icon(dc,{row.left+D(35),row.top+D(20),row.left+D(59),row.top+D(44)},Glyph::Star,kAccent);
                Text(dc,title,{row.left+D(70),row.top+D(7),row.right-D(118),row.top+D(32)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                std::wstring detail=url; if(!folder.empty())detail=folder+L"  •  "+url;
                Text(dc,detail,{row.left+D(70),row.top+D(33),row.right-D(118),row.bottom-D(7)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                const RECT remove{row.right-D(102),row.top+D(16),row.right-D(14),row.bottom-D(16)};
                const bool remove_hot=hot_native_command_==remove_command;
                Rounded(dc,remove,remove_hot?MixChromeColor(kSurface,kAccent,12):kSurface,remove_hot?MixChromeColor(kBorder,kAccent,34):kBorder,D(18));
                Text(dc,L"Remove",remove,small_font_,remove_hot?kInk:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                add_action(row,open_command);add_action(remove,remove_command);
            }
            y=cursor+D(16);
        }
    } else if (page == InternalPage::Profiles) {
        draw_header(L"Profiles", L"Keep work, personal browsing, cookies, history and bookmarks separated in independent WebCom profiles.");
        draw_button({right-D(156),origin+D(34),right,origin+D(68)},L"Add profile",AddProfile);
        if(state_.profiles.size()>1)draw_button({right-D(326),origin+D(34),right-D(166),origin+D(68)},L"Delete current",DeleteCurrentProfileCmd);
        draw_button({left,y,left+D(150),y+D(32)},L"Open all profiles",OpenAllProfiles);
        draw_button({left+D(158),y,left+D(332),y+D(32)},L"Close other windows",CloseOtherProfiles);
        y += D(48);
        const int row_height=D(76);
        for(std::size_t i=0;i<state_.profiles.size();++i){
            const RECT row{left,y+static_cast<int>(i)*row_height,right,y+static_cast<int>(i+1)*row_height-D(8)};
            if(row.bottom<=content.top || row.top>=content.bottom)continue;
            const auto& saved=state_.profiles[i];
            const bool current=saved.name==profile_name_;
            const auto session=session_.lock();const bool running=session && session->IsProfileOpen(saved.name);
            const UINT command=ProfileBase+static_cast<UINT>(i);const bool hot=hot_native_command_==command;
            Rounded(dc,row,hot?kHover:kSurface,hot?MixChromeColor(kBorder,kAccent,25):kBorder,D(16));
            const auto appearance=current?appearance_:LoadAppearance(root_,saved.name);
            const auto badge_rgb=ProfileColor(appearance,saved.name);const auto badge=NativeColor(badge_rgb);
            Rounded(dc,{row.left+D(18),row.top+D(18),row.left+D(52),row.top+D(52)},badge,badge,D(22));
            std::wstring initial=Wide(saved.name).substr(0,1);
            Text(dc,initial,{row.left+D(18),row.top+D(18),row.left+D(52),row.top+D(52)},font_,NativeColor(ContrastInk(badge_rgb)),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,Wide(saved.name),{row.left+D(66),row.top+D(9),row.right-D(160),row.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            const auto status=current?L"This window":running?L"Running":L"Closed";
            const auto saved_tabs=std::to_wstring(saved.tabs.size())+L" saved tab"+(saved.tabs.size()==1?L"":L"s");
            const auto saved_groups=std::to_wstring(saved.groups.size())+L" group"+(saved.groups.size()==1?L"":L"s");
            const auto detail=std::wstring(status)+L" • "+saved_tabs+L" • "+saved_groups;
            Text(dc,detail,{row.left+D(66),row.top+D(34),row.right-D(160),row.bottom-D(9)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            const std::wstring action=current?L"Current":running?L"Focus":L"Open";
            Text(dc,action,{row.right-D(138),row.top+D(16),row.right-D(42),row.bottom-D(16)},small_font_,current?kMuted:kAccent,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            if(!current)Icon(dc,{row.right-D(30),row.top+D(26),row.right-D(14),row.top+D(42)},Glyph::Forward,kMuted);
            add_action(row,command);
        }
        y+=static_cast<int>(state_.profiles.size())*row_height+D(24);
    } else if (page == InternalPage::Settings) {
        draw_header(L"Settings", L"WebCom settings for the " + Wide(profile_name_) + L" profile. Changes save automatically.");
        struct SettingRow { const wchar_t* title; std::wstring detail; UINT command; Glyph glyph; };
        const std::array<SettingRow, 10> rows{{
            {L"Appearance", L"Theme, profile color and visual preferences", AppearanceMenu, Glyph::Spark},
            {L"Performance", state_.memory_mode == MemoryMode::Saver ? L"Memory Saver • sleeps eligible tabs after 1 minute" : L"Balanced • sleeps eligible tabs after 5 minutes", ToggleSaver, Glyph::Reload},
            {L"Passwords and autofill", state_.save_passwords ? L"Password-save prompts are on" : L"New password-save prompts are off", TogglePasswords, Glyph::Shield},
            {L"Profiles", std::to_wstring(state_.profiles.size()) + L" local profile" + (state_.profiles.size()==1?L"":L"s") + L" • multiple windows can run together", ProfileManager, Glyph::User},
            {L"Session recovery", L"Keeps a last known-good session backup and restores it automatically if the current session is unreadable", 0, Glyph::Reload},
            {L"Site permissions", std::to_wstring(site_permissions_.size()) + L" saved override" + (site_permissions_.size()==1?L"":L"s") + L" • shield shows connection security, cookies and permissions", 0, Glyph::Shield},
            {L"Bookmark manager", std::to_wstring(bookmarks_.size()) + L" saved bookmark" + (bookmarks_.size()==1?L"":L"s"), BookmarkManager, Glyph::Star},
            {L"Bookmarks bar", show_bookmarks_ ? L"Shown below the address bar" : L"Hidden", ToggleBookmarks, Glyph::Star},
            {L"Profile color", L"Change the color used to identify this profile", ProfileColorMenu, Glyph::User},
            {L"Privacy and browsing data", L"Clear cookies, cache, site storage and saved passwords", ClearData, Glyph::Globe}
        }};
        const int row_height = D(78);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const RECT row{left, y + static_cast<int>(i) * row_height, right,
                           y + static_cast<int>(i + 1) * row_height - D(8)};
            if (row.bottom <= content.top || row.top >= content.bottom) continue;
            const bool hot = hot_native_command_ == rows[i].command;
            Rounded(dc, row, hot ? kHover : kSurface, hot ? MixChromeColor(kBorder, kAccent, 25) : kBorder, D(16));
            Icon(dc, {row.left + D(20), row.top + D(22), row.left + D(46), row.top + D(48)}, rows[i].glyph, kAccent);
            Text(dc, rows[i].title, {row.left + D(60), row.top + D(9), row.right - D(42), row.top + D(34)},
                 font_, kInk, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            Text(dc, rows[i].detail, {row.left + D(60), row.top + D(34), row.right - D(42), row.bottom - D(9)},
                 small_font_, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (rows[i].command) {
                Icon(dc, {row.right - D(32), row.top + D(26), row.right - D(16), row.top + D(42)}, Glyph::Forward, kMuted);
                add_action(row, rows[i].command);
            }
        }
        y += static_cast<int>(rows.size()) * row_height + D(24);
    }

    native_page_extent_ = std::max(0, y - origin + D(24));
    const int available = static_cast<int>(content.bottom - content.top);
    const int maximum = std::max(0, native_page_extent_ - available);
    if (maximum > 0) {
        const int track_h = std::max(D(32), available * available / std::max(available, native_page_extent_));
        const int travel = std::max(1, available - track_h - D(16));
        const int thumb_y = content.top + D(8) + travel * native_scroll_ / maximum;
        Rounded(dc, {content.right - D(7), thumb_y, content.right - D(3), thumb_y + track_h},
                MixChromeColor(kMuted, kFrame, 55), MixChromeColor(kMuted, kFrame, 55), D(4));
    }
}

bool BrowserApp::Impl::ScrollInternal(int delta) {
    const auto* tab = Active();
    if (!tab || tab->web) return false;
    const auto page = InternalPageFor(tab->url);
    if (page == InternalPage::None || page == InternalPage::NewTab) return false;
    const RECT content = ContentBounds();
    const int maximum = std::max(0, native_page_extent_ - static_cast<int>(content.bottom - content.top));
    if (!maximum) return false;
    const int previous = native_scroll_;
    native_scroll_ = std::clamp(native_scroll_ + delta, 0, maximum);
    if (previous != native_scroll_) { InvalidateRect(window_, &content, FALSE); return true; }
    return false;
}

bool BrowserApp::Impl::HitNativeAction(POINT point) {
    const auto* tab = Active();
    if (!tab || tab->web || InternalPageFor(tab->url) == InternalPage::NewTab || InternalPageFor(tab->url) == InternalPage::None) return false;
    for (auto it=native_actions_.rbegin();it!=native_actions_.rend();++it) {
        if (it->command && PtInRect(&it->rect, point)) { Dispatch(it->command); return true; }
    }
    return false;
}

void BrowserApp::Impl::Paint() {
    PAINTSTRUCT paint{};HDC output=BeginPaint(window_,&paint);
    RECT r{};GetClientRect(window_,&r);
    auto* tab=Active();
    const int height=tab && tab->web?std::min(static_cast<int>(r.bottom),ContentTop()):static_cast<int>(r.bottom);
    if (height<=0) { native_chrome_.frame.Reset();EndPaint(window_,&paint);return; }
    HDC buffered=native_chrome_.frame.Begin(output,static_cast<int>(r.right),height);
    HDC dc=buffered?buffered:output;
    if(buffered)IntersectClipRect(dc,paint.rcPaint.left,paint.rcPaint.top,paint.rcPaint.right,paint.rcPaint.bottom);
    Fill(dc,r,kFrame);
    const RECT content=ContentBounds();
    const int dip_width=MulDiv(static_cast<int>(r.right),96,static_cast<int>(dpi_));
    const int dip_height=MulDiv(static_cast<int>(content.bottom-content.top),96,static_cast<int>(dpi_));
    if (!fullscreen_) {
        Fill(dc,{0,0,r.right,D(40)},kCaption);
        const RECT omni=native_chrome_.omnibox;
        const bool focused=GetFocus()==address_;
        Rounded(dc,omni,focused?kFrame:kSurface,focused?kFrame:kSurface,D(36));
        if (focused) FocusOutline(dc,omni,D(36),D(2),kAccent);
        const RECT site{omni.left+D(10),D(53),omni.left+D(28),D(71)};
        if(!tab || IsHome(tab->url))ChromeGoogleMark(dc,site);
        else Icon(dc,site,Glyph::Globe,kMuted);
        Fill(dc,{0,ContentTop()-D(1),r.right,ContentTop()},kBorder);
    }
    if (tab && !tab->web) {
        const int saved_dc=SaveDC(dc);
        IntersectClipRect(dc,content.left,content.top,content.right,content.bottom);
        if (IsHome(tab->url) && !tab->creating && tab->private_mode) {
            const auto layout=MakeChromePrivateLayout(dip_width,dip_height);
            const int left=D(layout.left),width=D(layout.width),origin=content.top-private_scroll_;
            auto y=[&](int dip){return origin+D(dip);};
            PrivateIcon(dc,{r.right/2-D(layout.icon)/2,y(layout.top),r.right/2+D(layout.icon)/2,y(layout.top+layout.icon)},true);
            Text(dc,L"You’ve gone Incognito",{left,y(layout.title_top),left+width,y(layout.title_top+32)},
                heading_font_,kInk,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,L"Others who use this device won’t see your activity, so you can browse more privately. "
                L"This won’t change how data is collected by websites you visit and the services they use, including Google. "
                L"Downloads and bookmarks you create will be saved.",
                {left,y(layout.intro_top),left+width,y(layout.intro_top+layout.intro_height)},font_,kMuted,
                DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            const int second_left=layout.narrow?left:left+width/2+D(15);
            const int column_width=layout.narrow?width:width/2-D(15);
            Text(dc,L"WebCom won’t restore:",{left,y(layout.columns_top),left+column_width,y(layout.columns_top+22)},font_,kInk);
            Text(dc,L"Your activity may still be visible to:",{second_left,y(layout.second_column_top),second_left+column_width,y(layout.second_column_top+22)},font_,kInk);
            constexpr const wchar_t* a[]={L"Private tabs after closing the app",L"Private browsing history",L"Cookies after the private session"};
            constexpr const wchar_t* b[]={L"Websites you visit",L"Your employer or school",L"Your internet service provider"};
            for(int i=0;i<3;++i){
                const int first_y=y(layout.columns_top+26+i*21),second_y=y(layout.second_column_top+26+i*21);
                Text(dc,L"•",{left,first_y,left+D(10),first_y+D(21)},font_,kMuted);
                Text(dc,a[i],{left+D(16),first_y,left+column_width,first_y+D(21)},small_font_,kMuted);
                Text(dc,L"•",{second_left,second_y,second_left+D(10),second_y+D(21)},font_,kMuted);
                Text(dc,b[i],{second_left+D(16),second_y,second_left+column_width,second_y+D(21)},small_font_,kMuted);
            }
            const RECT card{left,y(layout.card_top),left+width,y(layout.card_top+layout.card_height)};
            Rounded(dc,card,kSurface,kBorder,D(16));
            Icon(dc,{left+D(18),card.top+D(18),left+D(36),card.top+D(36)},Glyph::Shield,kMuted);
            Text(dc,L"Private browsing has limits",{left+D(48),card.top+D(12),card.right-D(16),card.top+D(42)},font_,kInk);
            Text(dc,L"Incognito separates this session from your normal profile. It does not hide your identity or "
                L"IP address from websites. Downloaded files and saved bookmarks remain on this device.",
                {left+D(18),card.top+D(48),card.right-D(18),card.bottom-D(12)},font_,kMuted,
                DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
        } else if(IsHome(tab->url) && !tab->creating) {
            const auto layout=MakeChromeHomeLayout(dip_width,dip_height);
            const int top=content.top+D(layout.logo_top);
            if(google_logo_){
                Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.DrawImage(google_logo_.get(),r.right/2-D(layout.logo_width)/2,top,D(layout.logo_width),D(layout.logo_height));
            }else Text(dc,L"Google",{0,top,r.right,top+D(layout.logo_height)},logo_font_,kInk,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            const RECT search=native_chrome_.home_search_rect;
            Rounded(dc,search,kSearch,GetFocus()==home_search_?kAccent:kBorder,D(52));
            if(GetFocus()==home_search_)FocusOutline(dc,search,D(52),D(1),kAccent);
            Icon(dc,{search.left+D(18),search.top+D(17),search.left+D(36),search.top+D(35)},Glyph::Search,kMuted);
        } else if (IsInternalPage(tab->url) && !tab->creating) {
            PaintInternalPage(dc, content, *tab);
        } else {
            const int card_width=std::min(D(600),static_cast<int>(r.right)-D(64));
            const int left=(r.right-card_width)/2,top=content.top+D(std::clamp((dip_height-300)/4,36,96));
            const RECT disc{r.right/2-D(28),top,r.right/2+D(28),top+D(56)};
            Rounded(dc,disc,kSurface,kBorder,D(56));
            Icon(dc,{disc.left+D(15),disc.top+D(15),disc.right-D(15),disc.bottom-D(15)},
                tab->failed?Glyph::Globe:Glyph::Reload,tab->failed?kMuted:kAccent);
            Text(dc,tab->failed?L"This tab couldn’t open":L"Opening your page…",
                {left,top+D(80),left+card_width,top+D(118)},heading_font_,kInk,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            Text(dc,tab->failed?tab->error:L"Your page is loading. You can keep using the address bar and other tabs.",
                {left,top+D(138),left+card_width,content.top+D(std::max(250,std::min(330,dip_height-84)))},font_,kMuted,
                DT_CENTER|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
        }
        RestoreDC(dc,saved_dc);
    }
    if(buffered)native_chrome_.frame.Present(output,paint.rcPaint);
    EndPaint(window_,&paint);
}

bool BrowserApp::Impl::ScrollPrivate(int delta) {
    const auto* tab=Active();
    if(!tab || !tab->private_mode || !IsHome(tab->url) || tab->web)return false;
    const RECT content=ContentBounds();
    const int available=static_cast<int>(content.bottom-content.top);
    const auto layout=MakeChromePrivateLayout(MulDiv(static_cast<int>(content.right),96,static_cast<int>(dpi_)),
        MulDiv(available,96,static_cast<int>(dpi_)));
    const int maximum=std::max(0,D(layout.extent)-available);
    if(!maximum)return false;
    const int previous=private_scroll_;
    private_scroll_=std::clamp(private_scroll_+delta,0,maximum);
    if(previous!=private_scroll_)InvalidateRect(window_,&content,FALSE);
    return true;
}

void BrowserApp::Impl::DrawItem(const DRAWITEMSTRUCT& source) {
    if(source.CtlType!=ODT_BUTTON)return;
    DRAWITEMSTRUCT draw=source;
    HDC buffered=native_chrome_.button.Begin(source.hDC,static_cast<int>(source.rcItem.right),
        static_cast<int>(source.rcItem.bottom),true);
    if(buffered){draw.hDC=buffered;IntersectClipRect(buffered,source.rcItem.left,source.rcItem.top,source.rcItem.right,source.rcItem.bottom);}
    RECT r=draw.rcItem;
    const UINT id=draw.CtlID;
    const bool pressed=(draw.itemState&ODS_SELECTED)!=0;
    const bool disabled=(draw.itemState&ODS_DISABLED)!=0;
    const bool hot=!disabled && (hovered_button_==draw.hwndItem || (id==More && main_menu_active_));
    const COLORREF ink=disabled?MixChromeColor(kFrame,kMuted,60):kInk;
    const bool caption=id==TabList || id==NewTab || id==MinimizeWindow || id==MaximizeWindow || id==CloseWindow;
    COLORREF background=caption?kCaption:kFrame;
    if(id==AiMode || id==BookmarkPage || id==SiteInfo)background=GetFocus()==address_?kFrame:kSurface;
    Fill(draw.hDC,r,background);
    auto glyph=[&](Glyph kind,int size=18,COLORREF color=CLR_INVALID){
        if(color==CLR_INVALID)color=ink;
        const int x=(r.left+r.right-D(size))/2,y=(r.top+r.bottom-D(size))/2;
        Icon(draw.hDC,{x,y,x+D(size),y+D(size)},kind,color);
    };
    if(id==MinimizeWindow || id==MaximizeWindow || id==CloseWindow){
        if(hot || pressed)Fill(draw.hDC,r,id==CloseWindow?(pressed?RGB(170,26,18):RGB(196,43,28)):kHover);
        glyph(id==MinimizeWindow?Glyph::Minimize:id==CloseWindow?Glyph::Close:
            IsZoomed(window_)?Glyph::Restore:Glyph::Maximize,14,id==CloseWindow && hot?RGB(255,255,255):ink);
    }else if(id==Profile){
        const auto* tab=Active();const bool private_mode=tab && tab->private_mode;
        const bool compact=r.right-r.left<D(70);
        const COLORREF fill=private_mode?(hot?kHover:kSurface):MixChromeColor(hot?kHover:kSurface,profile_color_,hot?16:10);
        const COLORREF border=private_mode?kBorder:MixChromeColor(kBorder,profile_color_,40);
        Rounded(draw.hDC,r,fill,border,D(34));
        const int avatar_left=compact?(r.right-D(22))/2:D(6);
        const RECT avatar{avatar_left,D(6),avatar_left+D(22),D(28)};
        if(private_mode){
            PrivateIcon(draw.hDC,{avatar.left-D(2),avatar.top-D(2),avatar.right+D(2),avatar.bottom+D(2)},false);
            Rounded(draw.hDC,{avatar.right-D(6),avatar.bottom-D(5),avatar.right+D(1),avatar.bottom+D(2)},profile_color_,kBorder,D(7));
        }else{
            Rounded(draw.hDC,avatar,profile_color_,MixChromeColor(kBorder,kInk,12),D(22));
            std::wstring initial=Wide(profile_name_).substr(0,1);
            if(!initial.empty())CharUpperBuffW(initial.data(),1);
            Text(draw.hDC,initial,avatar,small_font_,profile_ink_,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }
        if(!compact)Text(draw.hDC,private_mode?L"Incognito":Wide(profile_name_),
            {D(36),0,r.right-D(10),r.bottom},small_font_,ink);
    }else if(id==AiMode){
        const COLORREF fill=MixChromeColor(kSurface,kAccent,hot?24:12);
        Rounded(draw.hDC,r,fill,MixChromeColor(kBorder,kAccent,20),D(24));
        Icon(draw.hDC,{D(7),D(4),D(23),D(20)},Glyph::Spark,kAccent);
        Text(draw.hDC,L"AI Mode",{D(29),0,r.right-D(6),r.bottom},small_font_,ink);
    }else if(id==Google || id==YouTube || id==GitHub){
        if(hot || pressed)Rounded(draw.hDC,{D(3),0,r.right-D(3),r.bottom},pressed?kHover:MixChromeColor(kFrame,kHover,60),kFrame,D(16));
        const RECT circle{r.right/2-D(23),D(8),r.right/2+D(23),D(54)};
        Rounded(draw.hDC,circle,kSurface,kBorder,D(46));
        if(id==Google)ChromeGoogleMark(draw.hDC,{circle.left+D(12),circle.top+D(12),circle.right-D(12),circle.bottom-D(12)});
        else if(id==YouTube)ChromeYouTubeMark(draw.hDC,{circle.left+D(11),circle.top+D(15),circle.right-D(11),circle.bottom-D(15)});
        else Text(draw.hDC,L"GH",circle,font_,kInk,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        Text(draw.hDC,ControlText(draw.hwndItem),{D(4),D(62),r.right-D(4),D(86)},small_font_,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    }else if(id==HomeGo){
        if(const auto* tab=Active();tab && tab->failed && !IsHome(tab->url)){
            const COLORREF fill=hot?MixChromeColor(kAccent,RGB(255,255,255),12):kAccent;
            Rounded(draw.hDC,r,fill,fill,D(16));
            const std::uint32_t rgb=(GetRValue(fill)<<16)|(GetGValue(fill)<<8)|GetBValue(fill);
            Text(draw.hDC,L"Reload tab",r,font_,NativeColor(ContrastInk(rgb)),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }else{
            Fill(draw.hDC,r,kSearch);
            if(hot || pressed)Rounded(draw.hDC,r,MixChromeColor(kSearch,kAccent,12),kSearch,D(36));
            glyph(Glyph::Forward,18,kInk);
        }
    }else if(id>=BookmarkBarFolderBase && id<BookmarkBarFolderBase+bookmark_folders_.size()){
        if(hot || pressed)Rounded(draw.hDC,r,kHover,kHover,D(10));
        Icon(draw.hDC,{D(5),D(6),D(21),D(22)},Glyph::Folder,kMuted);
        Text(draw.hDC,ControlText(draw.hwndItem),{D(28),0,r.right-D(6),r.bottom},small_font_,ink);
    }else if(id>=BookmarkBase && id<BookmarkBase+bookmarks_.size()){
        if(hot || pressed)Rounded(draw.hDC,r,kHover,kHover,D(10));
        const auto& bookmark=bookmarks_[id-BookmarkBase];
        const RECT icon{D(5),D(6),D(21),D(22)};
        if(bookmark.url=="https://www.google.com/")ChromeGoogleMark(draw.hDC,icon);
        else if(bookmark.url=="https://www.youtube.com/")ChromeYouTubeMark(draw.hDC,{D(5),D(8),D(21),D(20)});
        else Icon(draw.hDC,icon,Glyph::Globe,kMuted);
        Text(draw.hDC,Wide(bookmark.title),{D(28),0,r.right-D(6),r.bottom},small_font_,ink);
    }else{
        if(id==TabList || hot || pressed){
            RECT fill=r;InflateRect(&fill,-D(1),-D(1));
            const COLORREF color=pressed?MixChromeColor(kHover,kInk,9):hot?kHover:kFrame;
            Rounded(draw.hDC,fill,color,color,D(id==TabList?18:32));
        }
        switch(id){
        case Back:glyph(Glyph::Back,18);break;
        case Forward:glyph(Glyph::Forward,18);break;
        case Reload:glyph(Active() && Active()->activity.loading?Glyph::Stop:Glyph::Reload,18);break;
        case NewTab:glyph(Glyph::Plus,18);break;
        case TabList:glyph(Glyph::Down,16);break;
        case More:glyph(Glyph::More,18);break;
        case SiteInfo:{
            bool customized=false;
            COLORREF security_color=ink;
            if(const auto* current=Active()) {
                if(current->certificate_error)security_color=RGB(242,139,130);
                else if(current->url.size()>=8 && _wcsnicmp(current->url.c_str(),L"https://",8)==0)security_color=kAccent;
                else if(current->url.size()>=7 && _wcsnicmp(current->url.c_str(),L"http://",7)==0)security_color=RGB(253,214,99);
                if(!current->private_mode){
                    const auto origin=SiteOrigin(Utf8(current->url));
                    customized=std::any_of(site_permissions_.begin(),site_permissions_.end(),[&](const SitePermissionRule& rule){return rule.origin==origin;});
                }
            }
            glyph(Glyph::Shield,18,customized?kAccent:security_color);break;
        }
        case DownloadsQuick:{
            glyph(Glyph::Download,18);
            const bool active_download=std::any_of(download_entries_.begin(),download_entries_.end(),[](const auto& item){
                return item && item->state==COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS && !item->paused;
            });
            if(active_download){const RECT dot{r.right-D(9),D(5),r.right-D(4),D(10)};Rounded(draw.hDC,dot,kAccent,kAccent,D(5));}
            break;
        }
        case BookmarkPage:{
            auto* tab=Active();const bool saved=tab && std::any_of(bookmarks_.begin(),bookmarks_.end(),
                [&](const Bookmark& b){return b.url==Utf8(tab->url);});
            const int x=(r.right-D(18))/2,y=(r.bottom-D(18))/2;
            Icon(draw.hDC,{x,y,x+D(18),y+D(18)},Glyph::Star,saved?kAccent:ink,saved);break;
        }
        case BookmarksOverflow:Text(draw.hDC,L"»",r,heading_font_,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);break;
        default:break;
        }
    }
    if((draw.itemState&ODS_FOCUS) && !(SendMessageW(draw.hwndItem,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){
        RECT focus=r;InflateRect(&focus,-D(3),-D(3));FocusOutline(draw.hDC,focus,D(8),D(1),kAccent);
    }
    if(buffered)native_chrome_.button.Present(source.hDC,source.rcItem);
}

RECT BrowserApp::Impl::DisplayTabRect(std::size_t index) const {
    if (index >= tab_rects_.size() || index >= tabs_.size()) return {};
    const RECT target = tab_rects_[index];
    if (!tab_reflow_start_ || tab_reflow_from_.empty()) return target;
    const auto now = GetTickCount64();
    const auto elapsed = now > tab_reflow_start_ ? now - tab_reflow_start_ : 0;
    constexpr std::uint64_t duration = 160;
    if (elapsed >= duration) return target;
    const auto found = std::find_if(tab_reflow_from_.begin(), tab_reflow_from_.end(),
        [&](const TabReflowOrigin& item){ return item.id == tabs_[index]->id; });
    if (found == tab_reflow_from_.end()) return target;
    const double eased = static_cast<double>(ChromeReflowAmount(elapsed, static_cast<unsigned>(duration))) / 100.0;
    const auto lerp = [&](LONG a, LONG b) -> LONG { return a + static_cast<LONG>((b - a) * eased); };
    return {lerp(found->rect.left,target.left),lerp(found->rect.top,target.top),
            lerp(found->rect.right,target.right),lerp(found->rect.bottom,target.bottom)};
}

int BrowserApp::Impl::HitTab(POINT point) const {
    for (std::size_t i=0;i<tab_rects_.size();++i) {
        const RECT rect=DisplayTabRect(i);
        if (PtInRect(&rect,point)) return static_cast<int>(i);
    }
    return -1;
}

void BrowserApp::Impl::PaintTabs() {
    PAINTSTRUCT paint{};HDC output=BeginPaint(tab_strip_,&paint);
    RECT bounds{};GetClientRect(tab_strip_,&bounds);
    HDC buffered=native_chrome_.tabs.Begin(output,static_cast<int>(bounds.right),static_cast<int>(bounds.bottom));
    HDC dc=buffered?buffered:output;
    if(buffered)IntersectClipRect(dc,paint.rcPaint.left,paint.rcPaint.top,paint.rcPaint.right,paint.rcPaint.bottom);
    Fill(dc,bounds,kCaption);
    auto paint_tab=[&](std::size_t i){
        if(i>=tab_rects_.size())return;
        const RECT r=DisplayTabRect(i);if(r.right<=r.left || r.left>=bounds.right)return;
        const auto& tab=*tabs_[i];const bool selected=i==active_,hot=static_cast<int>(i)==hot_tab_;
        unsigned hover_amount=hot?100u:0u;
        if(hover_animation_start_)
            hover_amount=ChromeHoverAmount(static_cast<int>(i),hover_animation_from_,hover_animation_to_,
                GetTickCount64()-hover_animation_start_);
        const bool dragged=tab_dragging_ && tab.id==dragged_tab_id_;
        if(selected){
            using namespace Gdiplus;
            Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);
            const REAL l=static_cast<REAL>(r.left),t=static_cast<REAL>(r.top),right=static_cast<REAL>(r.right),b=static_cast<REAL>(r.bottom);
            const REAL c=static_cast<REAL>(D(10));
            GraphicsPath p;
            p.AddBezier(l-c,b,l,b,l,b-c,l,b-c);
            p.AddLine(l,b-c,l,t+c);p.AddArc(l,t,2*c,2*c,180,90);
            p.AddLine(l+c,t,right-c,t);p.AddArc(right-2*c,t,2*c,2*c,270,90);
            p.AddLine(right,t+c,right,b-c);p.AddBezier(right,b-c,right,b,right+c,b,right+c,b);
            p.CloseFigure();
            COLORREF fill=kFrame;
            if(dragged) fill=MixChromeColor(kFrame,kCaption,28);
            else if(tab_transition_until_){
                const auto now=GetTickCount64();
                const auto remaining=tab_transition_until_>now?tab_transition_until_-now:0;
                const unsigned progress=static_cast<unsigned>(100-std::min<std::uint64_t>(100,remaining*100/140));
                fill=MixChromeColor(kCaption,kFrame,std::max(18u,progress));
            }
            SolidBrush brush(Color(255,GetRValue(fill),GetGValue(fill),GetBValue(fill)));g.FillPath(&brush,&p);
        }else if(hover_amount || dragged){
            RECT hover=r;hover.bottom-=D(3);
            const COLORREF fill=dragged?MixChromeColor(kCaption,kAccent,10):
                MixChromeColor(kCaption,kHover,std::max(1u,hover_amount*60/100));
            Rounded(dc,hover,fill,fill,D(16));
        }else if(i+1<tab_rects_.size() && i+1!=active_ && tab_rects_[i+1].right>tab_rects_[i+1].left){
            Fill(dc,{r.right+D(1),D(14),r.right+D(2),D(30)},kBorder);
        }
        const bool compact_pinned=tab.activity.pinned && (r.right-r.left)<=D(52);
        const int icon_left=compact_pinned ? r.left + std::max(0,static_cast<int>(r.right-r.left-D(18))/2) : r.left+D(10);
        const RECT icon{icon_left,D(13),icon_left+D(18),D(31)};
        if(tab.muted)Icon(dc,icon,Glyph::Muted,kMuted);
        else if(tab.activity.audible)Icon(dc,icon,Glyph::Speaker,kAccent);
        else if(tab.private_mode)PrivateIcon(dc,{icon.left-D(2),icon.top-D(2),icon.right+D(2),icon.bottom+D(2)},false);
        else if(tab.activity.loading || tab.creating)Icon(dc,icon,Glyph::Reload,kAccent);
        else if(tab.favicon){
            Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(tab.favicon.get(),static_cast<int>(icon.left),static_cast<int>(icon.top),
                static_cast<int>(icon.right-icon.left),static_cast<int>(icon.bottom-icon.top));
        }else if(IsHome(tab.url))ChromeGoogleMark(dc,icon);
        else if(tab.activity.pinned)Icon(dc,icon,Glyph::Star,kMuted,true);
        else Icon(dc,icon,Glyph::Globe,tab.activity.suspended?MixChromeColor(kCaption,kMuted,70):kMuted);
        const bool show_close=!compact_pinned && (selected || hot || r.right-r.left>=D(144));
        const std::wstring title=IsHome(tab.url)?(tab.private_mode?L"New Incognito Tab":L"New Tab"):tab.title;
        bool group_start=!tab.group.empty();
        if(group_start)for(std::size_t previous=i;previous>0;){
            --previous;if(previous<tab_rects_.size() && !IsRectEmpty(&tab_rects_[previous])){
                group_start=tabs_[previous]->group!=tab.group;break;
            }
        }
        int text_left=r.left+D(34);
        std::wstring display_title=title;
        if(group_start && !compact_pinned){
            std::size_t group_count=0;for(const auto& item:tabs_)if(item->group==tab.group)++group_count;
            const auto* meta=FindTabGroup(tab.group);
            const auto hash=std::hash<std::wstring>{}(tab.group);
            const auto color_index=meta?meta->color:static_cast<std::uint8_t>(hash%kProfileColors.size());
            const COLORREF group_color=NativeColor(kProfileColors[color_index%kProfileColors.size()]);
            const bool collapsed=IsGroupCollapsed(tab.group);
            const std::wstring chip_text=(collapsed?L"▸ ":L"▾ ")+tab.group;
            SIZE chip_size{};const auto previous=SelectObject(dc,small_font_);
            GetTextExtentPoint32W(dc,chip_text.c_str(),static_cast<int>(chip_text.size()),&chip_size);SelectObject(dc,previous);
            const int available=std::max(D(52),static_cast<int>(r.right-r.left)-D(show_close?82:58));
            const int chip_width=std::clamp(static_cast<int>(chip_size.cx)+D(18),D(52),std::min(D(150),available));
            const RECT chip{text_left,D(9),text_left+chip_width,D(35)};
            Rounded(dc,chip,MixChromeColor(selected?kFrame:kCaption,group_color,18),MixChromeColor(kBorder,group_color,55),D(13));
            Text(dc,chip_text,{chip.left+D(8),chip.top,chip.right-D(8),chip.bottom},small_font_,selected?kInk:MixChromeColor(kMuted,group_color,35),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            text_left=chip.right+D(6);
            if(collapsed)display_title=L"("+std::to_wstring(group_count)+L")";
        }
        if(!compact_pinned && text_left<r.right-D(show_close?30:8)) Text(dc,display_title,{text_left,D(8),r.right-D(show_close?32:10),D(36)},small_font_,selected && !dragged?kInk:kMuted);
        if(show_close && i<native_chrome_.tab_close_rects.size()){
            RECT close{r.right-D(28),D(10),r.right-D(4),D(34)};
            if((hot && hot_close_ && !tab_dragging_) || pressed_close_tab_id_==tab.id)
                Rounded(dc,close,pressed_close_tab_id_==tab.id?MixChromeColor(kHover,kInk,12):kHover,kHover,D(24));
            InflateRect(&close,-D(6),-D(6));Icon(dc,close,Glyph::Close,selected?kInk:kMuted);
        }
        if(!tab.group.empty()){
            const auto* meta=FindTabGroup(tab.group);
            const auto hash=std::hash<std::wstring>{}(tab.group);
            const auto color_index=meta?meta->color:static_cast<std::uint8_t>(hash%kProfileColors.size());
            const COLORREF group_color=NativeColor(kProfileColors[color_index%kProfileColors.size()]);
            Rounded(dc,{r.left+D(7),r.bottom-D(4),r.right-D(7),r.bottom-D(2)},group_color,group_color,D(2));
        }
        if(selected && GetFocus()==tab_strip_){
            RECT focus=r;InflateRect(&focus,-D(3),-D(3));FocusOutline(dc,focus,D(12),D(1),kAccent);
        }
    };
    for(std::size_t i=0;i<tabs_.size();++i)if(i!=active_ && i<tab_rects_.size() && !IsRectEmpty(&tab_rects_[i]))paint_tab(i);
    if(active_<tab_rects_.size())paint_tab(active_);
    if(tab_dragging_ && drag_target_index_>=0 && static_cast<std::size_t>(drag_target_index_)<tab_rects_.size()){
        std::size_t source=tabs_.size();
        for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->id==dragged_tab_id_){source=i;break;}
        const auto target=static_cast<std::size_t>(drag_target_index_);
        const RECT target_rect=DisplayTabRect(target);
        if(source<tabs_.size() && source!=target && target_rect.right>target_rect.left){
            const int x=target<source?target_rect.left-D(3):target_rect.right+D(3);
            Rounded(dc,{x-D(1),D(10),x+D(2),D(35)},kAccent,kAccent,D(3));
        }
    }
    if(buffered)native_chrome_.tabs.Present(output,paint.rcPaint);
    EndPaint(tab_strip_,&paint);
}

void BrowserApp::Impl::SyncTabs() {
    if (!tab_strip_) return;
    if (auto* tab=Active()) {
        std::wstring name=L"Tabs: "+std::to_wstring(tabs_.size())+L". Selected: "+tab->title+L". Ctrl+Tab changes tab.";
        if(name!=native_chrome_.tab_accessible_label) {
            native_chrome_.tab_accessible_label=std::move(name);
            SetWindowTextW(tab_strip_,native_chrome_.tab_accessible_label.c_str());
        }
    }
    InvalidateRect(tab_strip_,nullptr,FALSE);
}

void BrowserApp::Impl::RebuildBookmarks() {
    native_chrome_.bookmark_widths.clear();
    for (HWND h : bookmark_buttons_) {
        TOOLINFOW tool{};tool.cbSize=sizeof(tool);tool.hwnd=window_;tool.uId=reinterpret_cast<UINT_PTR>(h);
        tool.uFlags=TTF_IDISHWND;if(tooltips_)SendMessageW(tooltips_,TTM_DELTOOLW,0,reinterpret_cast<LPARAM>(&tool));
        if (hovered_button_==h)hovered_button_=nullptr;DestroyWindow(h);
    }
    bookmark_buttons_.clear();bookmark_folders_.clear();
    for(std::size_t i=0;i<bookmarks_.size();++i)if(bookmarks_[i].folder.empty())
        bookmark_buttons_.push_back(Button(BookmarkBase+static_cast<UINT>(i),Wide(bookmarks_[i].title).c_str()));
    for(const auto& folder_path:webcom::BookmarkFolderPaths(bookmarks_,true)){
        if(folder_path.find('/')!=std::string::npos)continue;
        const auto folder=Wide(folder_path);
        bookmark_folders_.push_back(folder);
        bookmark_buttons_.push_back(Button(BookmarkBarFolderBase+static_cast<UINT>(bookmark_folders_.size()-1),folder.c_str()));
    }
    Layout();
}

void BrowserApp::Impl::BookmarkCurrent() {
    const auto* tab=Active();
    if (!tab || !IsWebAddress(tab->url)) { Note(L"Open a website to bookmark it.");return; }
    auto next=bookmarks_;
    const std::string url=Utf8(tab->url);
    const auto it=std::find_if(next.begin(),next.end(),[&](const Bookmark& b){return b.url==url;});
    const bool remove=it!=next.end();
    if (remove)next.erase(it);
    else {
        if (next.size()>=100) { Notice(L"This profile has 100 bookmarks. Remove one to add another.");return; }
        std::string title=Utf8(SafeTitle(tab->title));
        if (title.empty())title=url;
        next.push_back({std::move(title),url});
    }
    std::string error;
    if (!SaveBookmarks(root_,profile_name_,next,&error)) { Notice(L"The bookmark could not be saved. "+Wide(error));return; }
    bookmarks_=std::move(next);RebuildBookmarks();
    Note(remove?L"Bookmark removed.":L"Bookmark saved to this profile.");
}

void BrowserApp::Impl::MoveBookmarkToFolder(std::size_t index, std::wstring folder) {
    if(index>=bookmarks_.size()) return;
    auto next=bookmarks_;
    if(!webcom::MoveBookmarkToFolder(next,index,Utf8(folder)))return;
    std::string error;
    if(!SaveBookmarks(root_,profile_name_,next,&error)){Notice(L"The bookmark folder could not be saved. "+Wide(error));return;}
    bookmarks_=std::move(next);RebuildBookmarks();QueueUI();
}

void BrowserApp::Impl::RenameBookmarkFolderTree(std::wstring folder) {
    if (folder.empty()) return;
    const auto next_name = AskName(window_, instance_, &palette_, L"Rename bookmark folder", L"&Folder path",
        L"Use / to create nested folders, for example Work/Research.", folder.c_str(), L"Rename");
    if (!next_name || next_name->empty()) return;
    auto next = bookmarks_;
    if (!webcom::RenameBookmarkFolder(next, Utf8(folder), Utf8(*next_name))) {
        Note(L"Choose a different valid folder path."); return;
    }
    std::string error;
    if (!SaveBookmarks(root_, profile_name_, next, &error)) { Notice(L"The folder could not be renamed. " + Wide(error)); return; }
    bookmarks_ = std::move(next); RebuildBookmarks(); native_scroll_ = 0; QueueUI(); Note(L"Bookmark folder renamed.");
}

void BrowserApp::Impl::MoveBookmark(std::size_t source,std::size_t target) {
    if(source>=bookmarks_.size() || target>=bookmarks_.size() || source==target)return;
    auto next=bookmarks_;
    if(!ReorderBookmark(next,source,target)){Note(L"Drag bookmarks within the same folder. Use the bookmark menu to move between folders.");return;}
    std::string error;
    if(!SaveBookmarks(root_,profile_name_,next,&error)){Notice(L"The bookmark order could not be saved. "+Wide(error));return;}
    bookmarks_=std::move(next);RebuildBookmarks();native_scroll_=0;QueueUI();Note(L"Bookmark order updated.");
}

void BrowserApp::Impl::ImportBookmarksFromFile() {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"Bookmark HTML (*.html;*.htm)\0*.html;*.htm\0All files\0*.*\0";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog))return;
    std::vector<Bookmark> imported;std::string error;
    if(!ImportBookmarksHtml(file,imported,&error)){Notice(L"Bookmarks could not be imported. "+Wide(error));return;}
    auto next=bookmarks_;std::size_t added=0;
    for(auto& item:imported){
        if(next.size()>=100)break;
        if(std::none_of(next.begin(),next.end(),[&](const Bookmark& b){return b.url==item.url;})){next.push_back(std::move(item));++added;}
    }
    std::stable_sort(next.begin(),next.end(),[](const Bookmark& a,const Bookmark& b){if(a.folder!=b.folder)return a.folder<b.folder;return a.title<b.title;});
    if(!SaveBookmarks(root_,profile_name_,next,&error)){Notice(L"Imported bookmarks could not be saved. "+Wide(error));return;}
    bookmarks_=std::move(next);RebuildBookmarks();QueueUI();Note(L"Imported "+std::to_wstring(added)+L" bookmark"+(added==1?L".":L"s."));
}

void BrowserApp::Impl::ExportBookmarksToFile() {
    wchar_t file[MAX_PATH]=L"WebCom-bookmarks.html";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"Bookmark HTML (*.html)\0*.html\0All files\0*.*\0";dialog.lpstrDefExt=L"html";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    std::string error;
    if(!ExportBookmarksHtml(file,bookmarks_,&error)){Notice(L"Bookmarks could not be exported. "+Wide(error));return;}
    Note(L"Bookmarks exported.");
}

void BrowserApp::Impl::BookmarksMenu(POINT anchor,int context) {
    if (menu_open_ || closing_ || !IsWindow(window_)) return;
    const auto keep_alive=shared_from_this(); const HWND owner=window_;
    const std::string profile=profile_name_; const auto snapshot=bookmarks_;
    std::vector<MenuItem> items; std::vector<std::wstring> folders;
    for(const auto& path:webcom::BookmarkFolderPaths(snapshot,true))folders.push_back(Wide(path));
    const bool context_menu=context>=0 && static_cast<std::size_t>(context)<snapshot.size();
    if (context_menu) {
        items={{BookmarkBase+static_cast<UINT>(context),L"Open bookmark",L"",L"\uE8A7"},
               {NewBookmarkFolder,L"Move to new folder...",L"",L"\uE8B7"}};
        for(std::size_t i=0;i<folders.size();++i)
            items.push_back({BookmarkFolderBase+static_cast<UINT>(i),L"Move to "+folders[i],L"",L"\uE8B7"});
        if(!snapshot[static_cast<std::size_t>(context)].folder.empty()) {
            items.push_back({BookmarkFolderBase+999,L"Move to bookmarks bar",L"",L"\uE8B7"});
            items.push_back({RenameBookmarkFolderCmd,L"Rename folder...",L"",L"\uE8AC"});
        }
        items.push_back({0,L"",L"",L"",false,false,true});
        items.push_back({RemoveBookmark,L"Remove bookmark",L"",L"\uE74D"});
    } else {
        items.push_back({BookmarkPage,L"Bookmark this page",L"Ctrl+D",L"\uE734",Active() && IsWebAddress(Active()->url)});
        items.push_back({BookmarkManager,L"Bookmark manager",L"Ctrl+Shift+O",L"\uE8A7"});
        items.push_back({ImportBookmarks,L"Import bookmarks...",L"",L"\uE8B5"});
        items.push_back({ExportBookmarks,L"Export bookmarks...",L"",L"\uE8B5",!snapshot.empty()});
        items.push_back({0,L"",L"",L"",true,false,true});
        std::string last_folder="\x01";
        for (std::size_t i=0;i<snapshot.size();++i){
            const auto detail=snapshot[i].folder.empty()?L"":Wide(snapshot[i].folder);
            items.push_back({BookmarkBase+static_cast<UINT>(i),Wide(snapshot[i].title),detail,L"\uE774"});
        }
        if (snapshot.empty())items.push_back({0,L"No saved bookmarks",L"",L"",false});
    }
    UINT selected=0;
    { menu_open_=true;struct Reset{bool& value;~Reset(){value=false;}}reset{menu_open_};
      selected=ShowDarkMenu(owner,anchor,items,dpi_,context_menu?330:350,!context_menu,&palette_); }
    if (!selected || closing_ || !IsWindow(owner) || window_!=owner || profile_name_!=profile) return;
    if(context_menu){
        const auto index=static_cast<std::size_t>(context);
        if(selected==RemoveBookmark){
            const auto& saved=snapshot[index];auto next=bookmarks_;std::erase_if(next,[&](const Bookmark& b){return b.url==saved.url;});
            std::string error;if(!SaveBookmarks(root_,profile,next,&error)){Notice(Wide(error));return;}
            bookmarks_=std::move(next);RebuildBookmarks();Note(L"Bookmark removed.");return;
        }
        if(selected==NewBookmarkFolder){
            const auto name=AskName(window_,instance_,&palette_,L"Bookmark folder",L"&Folder path",L"Use / for nested folders, for example Work/Research.",L"",L"Move");
            if(name && !name->empty())MoveBookmarkToFolder(index,*name);return;
        }
        if(selected==BookmarkFolderBase+999){MoveBookmarkToFolder(index,L"");return;}
        if(selected==RenameBookmarkFolderCmd){RenameBookmarkFolderTree(Wide(snapshot[index].folder));return;}
        if(selected>=BookmarkFolderBase && selected<BookmarkFolderBase+folders.size()){
            MoveBookmarkToFolder(index,folders[selected-BookmarkFolderBase]);return;
        }
    }
    if (selected>=BookmarkBase && selected<BookmarkBase+snapshot.size()) Navigate(Wide(snapshot[selected-BookmarkBase].url));
    else Dispatch(selected);
}

void BrowserApp::Impl::OpenTabsMenu() {
    if(menu_open_ || closing_ || !IsWindow(window_))return;
    const auto keep_alive=shared_from_this();
    const HWND owner=window_;
    std::vector<TabSearchItem> items;
    items.reserve(tabs_.size());
    for(const auto& tab:tabs_){
        std::wstring detail=IsHome(tab->url)?L"New tab":tab->url;
        if(!tab->group.empty())detail=tab->group+L"  •  "+detail;
        items.push_back({tab->id,tab->title,detail,tab->private_mode,tab->activity.audible,tab->muted,tab->activity.suspended,IsActive(*tab)});
    }
    RECT r{};GetWindowRect(GetDlgItem(owner,TabList),&r);
    std::uint64_t selected=0;
    {
        menu_open_=true;
        struct Reset {bool& value;~Reset(){value=false;}} reset{menu_open_};
        selected=ShowTabSearch(owner,{r.left,r.bottom+D(6)},items,dpi_,&palette_);
    }
    if(closing_ || !IsWindow(owner) || !selected)return;
    for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->id==selected){Select(i);break;}
}

void BrowserApp::Impl::ConfigureTheme(Tab& tab) {
    ComPtr<ICoreWebView2Controller2> controller;
    if (tab.controller && SUCCEEDED(tab.controller.As(&controller)))
        controller->put_DefaultBackgroundColor({255,GetRValue(kFrame),GetGValue(kFrame),GetBValue(kFrame)});
    ComPtr<ICoreWebView2_13> access;
    ComPtr<ICoreWebView2Profile> profile;
    if (tab.web && SUCCEEDED(tab.web.As(&access)) && SUCCEEDED(access->get_Profile(&profile)))
        profile->put_PreferredColorScheme(COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK);
}
