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
    if (auto* accessible = new (std::nothrow) TabStripAccessible(this)) {
        tab_accessible_impl_ = accessible;
        tab_accessible_.Attach(accessible);
    }
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
    r.left = (!fullscreen_ && vertical_tabs_) ? D(vertical_tabs_collapsed_ ? 64 : 244) : 0;
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
        tab_rects_.assign(tabs_.size(),RECT{});
        cache.tab_close_rects.assign(tabs_.size(),RECT{});
        if(vertical_tabs_) {
            const int rail_width=vertical_tabs_collapsed_?64:244;
            const int panel_height=std::max(0,static_cast<int>(client.bottom)-ContentTop());
            const int panel_height_dip=MulDiv(panel_height,96,static_cast<int>(dpi_));
            const auto vertical=MakeChromeVerticalTabPlan(panel_height_dip,visible_indices.size(),display_active,first_visible_tab_);
            first_visible_tab_=vertical.first;
            for(const auto& slot:vertical.slots){
                if(slot.index>=visible_indices.size())continue;
                const auto actual=visible_indices[slot.index];
                const int top=D(slot.top),bottom=D(slot.bottom);
                tab_rects_[actual]={D(vertical_tabs_collapsed_?6:8),top,D(rail_width-(vertical_tabs_collapsed_?6:8)),bottom};
                if(!vertical_tabs_collapsed_ && !tabs_[actual]->activity.pinned)cache.tab_close_rects[actual]={D(204),top+D(6),D(228),bottom-D(6)};
            }
            batch.Place(tab_strip_,0,ContentTop(),D(rail_width),panel_height,chrome && !visible_indices.empty());
            place(TabList,6,6,28,28);place(NewTab,40,4,32,32);
        } else {
            std::size_t pinned_count=0;
            while(pinned_count<visible_indices.size() && tabs_[visible_indices[pinned_count]]->activity.pinned) ++pinned_count;
            const auto strip=MakeChromeTabStripPlan(dip_width,visible_indices.size(),pinned_count,display_active,first_visible_tab_);
            first_visible_tab_=strip.first_unpinned;
            for (const auto& slot : strip.slots) {
                if(slot.index>=visible_indices.size())continue;
                const auto actual=visible_indices[slot.index];
                tab_rects_[actual]={D(slot.left),D(6),D(slot.right),D(40)};
                if(!slot.pinned) cache.tab_close_rects[actual]={D(slot.right-28),D(10),D(slot.right-4),D(34)};
            }
            batch.Place(tab_strip_,D(strip.strip_left),0,D(strip.strip_width),D(40),chrome && !strip.slots.empty());
            place(TabList,6,6,28,28);place(NewTab,strip.new_tab_left,4,32,32);
        }
        place(MinimizeWindow,dip_width-138,0,46,40);
        place(MaximizeWindow,dip_width-92,0,46,40);
        place(CloseWindow,dip_width-46,0,46,40);
        const int profile_width=static_cast<int>(std::min<std::size_t>(140,48+profile_name_.size()*7));
        const auto tools=MakeChromeToolbarLayout(dip_width,profile_width,native_home,!download_entries_.empty());
        place(Back,6,47,32,32);place(Forward,43,47,32,32,tools.show_forward);
        place(Reload,tools.show_forward?79:43,47,32,32);
        cache.omnibox=chrome?RECT{D(tools.omnibox_left),D(45),D(tools.omnibox_right),D(81)}:RECT{};
        batch.Place(address_,D(tools.edit_left),D(53),D(tools.edit_right-tools.edit_left),D(23),chrome);
        place(SiteInfo,tools.omnibox_left+4,49,28,28,tab && (IsWebAddress(tab->url) || IsLocalFileUrl(tab->url)));
        place(AiMode,tools.action_left,51,82,24,tools.show_ai);
        place(BookmarkPage,tools.omnibox_right-34,49,28,28,tab && IsWebAddress(tab->url));
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
        const bool searchable_native=tab && !tab->web && (internal_page==InternalPage::History || internal_page==InternalPage::Bookmarks || internal_page==InternalPage::Shortcuts);
        if(searchable_native && native_filter_page_!=internal_page){
            native_filter_page_=internal_page; SetWindowTextW(native_filter_,L""); native_scroll_=0;
            SendMessageW(native_filter_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(
                internal_page==InternalPage::History?L"Search history":internal_page==InternalPage::Bookmarks?L"Search bookmarks":L"Search shortcuts"));
        } else if(!searchable_native) native_filter_page_=InternalPage::None;
        const int content_left_dip=MulDiv(static_cast<int>(content.left),96,static_cast<int>(dpi_));
        const int content_width_dip=MulDiv(static_cast<int>(content.right-content.left),96,static_cast<int>(dpi_));
        const int page_width_dip=std::min(980,std::max(320,content_width_dip-48));
        const int page_left_dip=content_left_dip+(content_width_dip-page_width_dip)/2;
        batch.Place(native_filter_,D(page_left_dip),content.top+D(98),D(std::min(460,page_width_dip)),D(32),chrome && searchable_native);
        const int dip_height=MulDiv(static_cast<int>(content.bottom-content.top),96,static_cast<int>(dpi_));
        if (tab && tab->private_mode && native_home) {
            const auto private_layout=MakeChromePrivateLayout(content_width_dip,dip_height);
            private_scroll_=std::clamp(private_scroll_,0,std::max(0,D(private_layout.extent)-static_cast<int>(content.bottom-content.top)));
        } else private_scroll_=0;
        const bool home=native_home && !tab->private_mode;
        const bool failed=tab && tab->failed && !tab->web && !IsHome(tab->url);
        const auto home_layout=MakeChromeHomeLayout(content_width_dip,dip_height);
        const int left=content.left+D(home_layout.search_left),top=content.top+D(home_layout.search_top);
        cache.home_search_rect=home?RECT{left,top,left+D(home_layout.search_width),top+D(52)}:RECT{};
        batch.Place(home_search_,left+D(48),top+D(14),D(home_layout.search_width-103),D(24),home);
        if (home) {
            if(ControlText(home_buttons_[0])!=L"Search Google")SetWindowTextW(home_buttons_[0],L"Search Google");
            batch.Place(home_buttons_[0],left+D(home_layout.search_width-44),top+D(8),D(36),D(36),true);
        } else {
            if(failed && ControlText(home_buttons_[0])!=L"Reload tab")SetWindowTextW(home_buttons_[0],L"Reload tab");
            const int content_center=content.left+static_cast<int>(content.right-content.left)/2;
            batch.Place(home_buttons_[0],content_center-D(68),content.top+D(std::max(270,std::min(350,dip_height-64))),D(136),D(38),failed);
        }
        const int shortcut_width=content_width_dip<420?92:104;
        const int content_center=content.left+static_cast<int>(content.right-content.left)/2;
        for(int i=1;i<4;++i)batch.Place(home_buttons_[i],content_center-D(shortcut_width*3)/2+D(shortcut_width)*(i-1),
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
            internal_page==InternalPage::Shortcuts?13:
            internal_page==InternalPage::Diagnostics?14:
            internal_page==InternalPage::Recovery?15:
            internal_page==InternalPage::Extensions?16:
            internal_page==InternalPage::Bookmarks?8:
            internal_page==InternalPage::Profiles?9:
            internal_page==InternalPage::Sessions?10:
            internal_page==InternalPage::Privacy?11:
            internal_page==InternalPage::Reader?12:
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
        if (content_changed)ApplyTabVisibility();
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

    if (page == InternalPage::Reader) {
        const std::wstring title = tab.title.starts_with(L"Reader: ") ? tab.title.substr(8) : tab.title;
        draw_header(title.empty()?L"Reader Mode":title, tab.reader_source.empty()?L"Distraction-free reading":tab.reader_source);
        draw_button({right-D(156),origin+D(34),right,origin+D(68)},L"Open original",ReaderBackToPage);
        const int controls_top=y-D(8);
        const wchar_t* width_label=tab.reader_width_mode==0?L"Width: Narrow":tab.reader_width_mode==2?L"Width: Wide":L"Width: Comfortable";
        draw_button({left,controls_top,left+D(54),controls_top+D(32)},L"A−",ReaderTextSmaller);
        draw_button({left+D(62),controls_top,left+D(116),controls_top+D(32)},L"A+",ReaderTextLarger);
        draw_button({left+D(124),controls_top,std::min(right,left+D(286)),controls_top+D(32)},width_label,ReaderWidthCycle);
        if(right-left>=D(620)){
            draw_button({left+D(294),controls_top,left+D(390),controls_top+D(32)},tab.reader_serif?L"Use sans":L"Use serif",ReaderFontToggle);
            draw_button({left+D(398),controls_top,left+D(486),controls_top+D(32)},L"Copy text",ReaderCopyText);
            draw_button({left+D(494),controls_top,std::min(right,left+D(590)),controls_top+D(32)},L"Export...",ReaderExportMenu);
            y+=D(40);
        }else{
            const int second=controls_top+D(40);
            draw_button({left,second,left+D(112),second+D(32)},tab.reader_serif?L"Use sans":L"Use serif",ReaderFontToggle);
            draw_button({left+D(120),second,left+D(224),second+D(32)},L"Copy text",ReaderCopyText);
            draw_button({left+D(232),second,std::min(right,left+D(344)),second+D(32)},L"Export...",ReaderExportMenu);
            y+=D(80);
        }
        const int requested_width=tab.reader_width_mode==0?D(620):tab.reader_width_mode==2?D(980):D(820);
        const int body_width=std::min(requested_width,right-left);
        const int body_left=left+std::max(0,(right-left-body_width)/2);
        y+=D(8);
        const int reader_size=std::clamp(17+tab.reader_text_scale*2,13,25);
        HFONT reader_font=CreateFontW(-D(reader_size),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,tab.reader_serif?L"Georgia":L"Segoe UI");
        if(!reader_font)reader_font=font_;
        if(tab.reader_text.empty()){
            const RECT card{body_left,y,body_left+body_width,y+D(120)};
            Rounded(dc,card,kSurface,kBorder,D(18));
            Text(dc,L"Reader content is unavailable. Return to the original page and try Reader Mode again.",
                 {card.left+D(24),card.top+D(24),card.right-D(24),card.bottom-D(24)},reader_font,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            y=card.bottom+D(24);
        }else{
            RECT measure{0,0,body_width-D(64),0};
            const auto previous=SelectObject(dc,reader_font);
            DrawTextW(dc,tab.reader_text.c_str(),static_cast<int>(tab.reader_text.size()),&measure,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);
            SelectObject(dc,previous);
            const int text_height=std::max(D(80),static_cast<int>(measure.bottom-measure.top));
            const RECT card{body_left,y,body_left+body_width,y+text_height+D(72)};
            if(card.bottom>content.top && card.top<content.bottom){
                Rounded(dc,card,kSurface,kBorder,D(20));
                Text(dc,tab.reader_text,{card.left+D(32),card.top+D(32),card.right-D(32),card.bottom-D(32)},reader_font,kInk,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);
            }
            y=card.bottom+D(24);
        }
        if(reader_font!=font_)DeleteObject(reader_font);
    } else if (page == InternalPage::History) {
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
        draw_header(L"Bookmarks", L"Saved pages in the " + ProfileDisplayName(profile_name_,&appearance_) + L" profile. Folders and HTML import/export stay local to this profile.");
        draw_button({right-D(104),origin+D(34),right,origin+D(68)},L"Export",ExportBookmarks);
        draw_button({right-D(216),origin+D(34),right-D(112),origin+D(68)},L"Import",ImportBookmarks);
        draw_button({right-D(336),origin+D(34),right-D(224),origin+D(68)},L"New folder",CreateBookmarkFolderCmd);
        y += D(42);
        const auto manager_folders=webcom::BookmarkFolderPaths(bookmarks_,explicit_bookmark_folders_,true);
        if(bookmark_dragging_){
            const RECT root_drop{left,y-D(38),right,y-D(4)};
            const bool root_target=bookmark_drag_folder_target_==-2;
            Rounded(dc,root_drop,root_target?MixChromeColor(kSurface,kAccent,16):kSurface,
                    root_target?MixChromeColor(kBorder,kAccent,62):kBorder,D(12));
            Text(dc,L"Drop here for bookmarks bar",{root_drop.left+D(12),root_drop.top,root_drop.right-D(12),root_drop.bottom},
                 small_font_,root_target?kInk:kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            add_action(root_drop,BookmarkFolderDropBase+999);
        }
        std::size_t matches=0;
        for(const auto& b:bookmarks_) if(contains_ci(Wide(b.title),filter_text)||contains_ci(Wide(b.url),filter_text)||contains_ci(Wide(b.folder),filter_text)) ++matches;
        std::size_t visible_empty_folders=0;
        for(std::size_t f=0;f<manager_folders.size();++f){
            const auto& folder_utf8=manager_folders[f];
            if(std::find(explicit_bookmark_folders_.begin(),explicit_bookmark_folders_.end(),folder_utf8)==explicit_bookmark_folders_.end())continue;
            bool has_bookmark=false;for(const auto& saved:bookmarks_)if(saved.folder==folder_utf8 || (saved.folder.size()>folder_utf8.size() && saved.folder.starts_with(folder_utf8) && saved.folder[folder_utf8.size()]=='/')){has_bookmark=true;break;}
            if(has_bookmark || (!filter_text.empty() && !contains_ci(Wide(folder_utf8),filter_text)))continue;
            const RECT folder_row{left,y,right,y+D(54)};y+=D(62);++visible_empty_folders;
            if(folder_row.bottom<=content.top || folder_row.top>=content.bottom)continue;
            Rounded(dc,folder_row,kSurface,kBorder,D(14));
            Icon(dc,{folder_row.left+D(18),folder_row.top+D(15),folder_row.left+D(42),folder_row.top+D(39)},Glyph::Folder,kAccent);
            Text(dc,Wide(folder_utf8),{folder_row.left+D(54),folder_row.top+D(5),folder_row.right-D(106),folder_row.top+D(30)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            Text(dc,L"Empty bookmark folder",{folder_row.left+D(54),folder_row.top+D(28),folder_row.right-D(106),folder_row.bottom-D(5)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            const RECT remove{folder_row.right-D(92),folder_row.top+D(10),folder_row.right-D(12),folder_row.bottom-D(10)};
            Text(dc,L"Remove",remove,small_font_,kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            add_action(remove,DeleteBookmarkFolderBase+static_cast<UINT>(f));
        }
        if (bookmarks_.empty() || matches==0) {
            const RECT card{left, y, right, y + D(150)};
            if (card.bottom > content.top && card.top < content.bottom) {
                Rounded(dc, card, kSurface, kBorder, D(18));
                Icon(dc, {card.left + D(28), card.top + D(28), card.left + D(54), card.top + D(54)}, Glyph::Star, kMuted);
                const bool filtered=!filter_text.empty() && (!bookmarks_.empty() || !manager_folders.empty());
                Text(dc, filtered?L"No matching bookmarks":visible_empty_folders?L"No bookmarks inside these folders yet":L"No bookmarks yet", {card.left + D(72), card.top + D(18), card.right - D(24), card.top + D(54)},
                     font_, kInk, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                Text(dc, filtered?L"Try another title, address, or folder name.":L"Save a page with Ctrl+D or import bookmark HTML from another browser.",
                     {card.left + D(72), card.top + D(54), card.right - D(24), card.bottom - D(18)}, small_font_, kMuted,
                     DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            }
            y = card.bottom + D(24);
        } else {
            const int row_height=D(72), heading_height=D(30); int cursor=y; std::wstring folder_marker=L"\x01";
            for(std::size_t i=0;i<bookmarks_.size();++i) {
                const auto title = Wide(bookmarks_[i].title);
                const auto url = Wide(bookmarks_[i].url);
                const auto folder = Wide(bookmarks_[i].folder);
                if(!contains_ci(title,filter_text)&&!contains_ci(url,filter_text)&&!contains_ci(folder,filter_text)) continue;
                if(folder!=folder_marker){folder_marker=folder;if(!folder.empty()){
                    const RECT heading{left,cursor,right,cursor+heading_height};
                    if(heading.bottom>content.top&&heading.top<content.bottom){
                        std::size_t folder_index=manager_folders.size();
                        const auto folder_utf8=Utf8(folder);
                        for(std::size_t f=0;f<manager_folders.size();++f)if(manager_folders[f]==folder_utf8){folder_index=f;break;}
                        const bool drop_target=bookmark_dragging_ && folder_index<manager_folders.size() && bookmark_drag_folder_target_==static_cast<int>(folder_index);
                        if(drop_target)Rounded(dc,heading,MixChromeColor(kSurface,kAccent,16),MixChromeColor(kBorder,kAccent,62),D(10));
                        const auto explicit_it=std::find(explicit_bookmark_folders_.begin(),explicit_bookmark_folders_.end(),folder_utf8);
                        const bool explicit_folder=explicit_it!=explicit_bookmark_folders_.end();
                        bool has_bookmark=false;
                        for(const auto& saved:bookmarks_)if(saved.folder==folder_utf8 || (saved.folder.size()>folder_utf8.size() && saved.folder.starts_with(folder_utf8) && saved.folder[folder_utf8.size()]=='/')){has_bookmark=true;break;}
                        const int title_right=explicit_folder && !has_bookmark ? heading.right-D(78) : heading.right-D(8);
                        Text(dc,folder,{heading.left+D(drop_target?10:0),heading.top,title_right,heading.bottom},font_,kInk,
                             DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                        if(explicit_folder && !has_bookmark && folder_index<manager_folders.size()){
                            const RECT remove_folder{heading.right-D(72),heading.top+D(2),heading.right,heading.bottom-D(2)};
                            Text(dc,L"Remove",remove_folder,small_font_,kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                            add_action(remove_folder,DeleteBookmarkFolderBase+static_cast<UINT>(folder_index));
                        }
                        if(bookmark_dragging_ && folder_index<manager_folders.size())add_action(heading,BookmarkFolderDropBase+static_cast<UINT>(folder_index));
                    }
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
    } else if (page == InternalPage::Privacy) {
        draw_header(L"Privacy Manager", L"Review cookie domains and saved site-permission exceptions for this profile. Cookie values are never displayed.");
        draw_button({left,y,left+D(112),y+D(32)},privacy_loading_?L"Refreshing…":L"Refresh",PrivacyRefresh);
        draw_button({left+D(120),y,left+D(300),y+D(32)},L"Clear all site data",PrivacyClearAllSiteData);
        const auto permission_origins=PrivacyPermissionOrigins();
        draw_button({left+D(308),y,left+D(500),y+D(32)},L"Reset all permissions",PrivacyResetAllPermissions);
        y+=D(52);

        Text(dc,L"Cookies and site data",{left,y,right,y+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);y+=D(40);
        if(!privacy_loaded_ && !privacy_loading_){
            const RECT card{left,y,right,y+D(112)};
            if(card.bottom>content.top && card.top<content.bottom){
                Rounded(dc,card,kSurface,kBorder,D(16));
                Text(dc,L"Open a normal website tab, then choose Refresh to enumerate cookie domains in this profile.",
                     {card.left+D(22),card.top+D(20),card.right-D(22),card.bottom-D(20)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            }
            y=card.bottom+D(22);
        }else if(privacy_loaded_ && privacy_cookie_domains_.empty()){
            Text(dc,L"No cookies are currently stored for this profile.",{left,y,right,y+D(38)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);y+=D(54);
        }else{
            const int row_height=D(62);
            for(std::size_t i=0;i<privacy_cookie_domains_.size();++i){
                const auto& item=privacy_cookie_domains_[i];
                const RECT row{left,y+static_cast<int>(i)*row_height,right,y+static_cast<int>(i+1)*row_height-D(6)};
                if(row.bottom<=content.top || row.top>=content.bottom)continue;
                const UINT command=PrivacyCookieBase+static_cast<UINT>(i);const bool hot=hot_native_command_==command;
                Rounded(dc,row,hot?kHover:kSurface,hot?MixChromeColor(kBorder,kAccent,25):kBorder,D(14));
                Icon(dc,{row.left+D(16),row.top+D(17),row.left+D(38),row.top+D(39)},Glyph::Globe,kMuted);
                Text(dc,item.domain,{row.left+D(50),row.top+D(7),row.right-D(116),row.top+D(31)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                Text(dc,std::to_wstring(item.count)+L" cookie"+(item.count==1?L"":L"s"),{row.left+D(50),row.top+D(31),row.right-D(116),row.bottom-D(7)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                const RECT clear{row.right-D(96),row.top+D(12),row.right-D(12),row.bottom-D(12)};
                Rounded(dc,clear,hot_native_command_==command?MixChromeColor(kSurface,kAccent,12):kSurface,kBorder,D(18));
                Text(dc,L"Clear",clear,small_font_,kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                add_action(row,command);add_action(clear,command);
            }
            y+=static_cast<int>(privacy_cookie_domains_.size())*row_height+D(24);
        }

        Text(dc,L"Permission exceptions",{left,y,right,y+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);y+=D(40);
        if(permission_origins.empty()){
            Text(dc,L"No saved Camera, Microphone, Location or Notifications exceptions.",{left,y,right,y+D(38)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);y+=D(54);
        }else{
            const int row_height=D(62);
            for(std::size_t i=0;i<permission_origins.size();++i){
                const auto& permission_origin=permission_origins[i];unsigned rule_count=0;
                for(const auto& rule:site_permissions_)if(rule.origin==permission_origin)++rule_count;
                const RECT row{left,y+static_cast<int>(i)*row_height,right,y+static_cast<int>(i+1)*row_height-D(6)};
                if(row.bottom<=content.top || row.top>=content.bottom)continue;
                const UINT command=PrivacyPermissionBase+static_cast<UINT>(i);const bool hot=hot_native_command_==command;
                Rounded(dc,row,hot?kHover:kSurface,hot?MixChromeColor(kBorder,kAccent,25):kBorder,D(14));
                Icon(dc,{row.left+D(16),row.top+D(17),row.left+D(38),row.top+D(39)},Glyph::Shield,kMuted);
                Text(dc,Wide(permission_origin),{row.left+D(50),row.top+D(7),row.right-D(116),row.top+D(31)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                Text(dc,std::to_wstring(rule_count)+L" saved exception"+(rule_count==1?L"":L"s"),{row.left+D(50),row.top+D(31),row.right-D(116),row.bottom-D(7)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                const RECT reset{row.right-D(96),row.top+D(12),row.right-D(12),row.bottom-D(12)};
                Rounded(dc,reset,hot?kHover:kSurface,kBorder,D(18));Text(dc,L"Reset",reset,small_font_,kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                add_action(row,command);add_action(reset,command);
            }
            y+=static_cast<int>(permission_origins.size())*row_height+D(24);
        }
    } else if (page == InternalPage::Sessions) {
        draw_header(L"Saved Sessions", L"Named sets of website tabs saved separately from crash recovery. Reopening a session creates new tabs.");
        draw_button({right-D(230),origin+D(34),right,origin+D(68)},L"Save current workspace",SaveWorkspaceSession);
        if(named_sessions_.empty()){
            const RECT card{left,y,right,y+D(150)};
            if(card.bottom>content.top && card.top<content.bottom){
                Rounded(dc,card,kSurface,kBorder,D(18));
                Icon(dc,{card.left+D(28),card.top+D(28),card.left+D(54),card.top+D(54)},Glyph::Folder,kMuted);
                Text(dc,L"No named sessions yet",{card.left+D(72),card.top+D(18),card.right-D(24),card.top+D(54)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                Text(dc,L"Save the current workspace when you want to reopen the same research, work or browsing set later without relying on crash recovery.",
                     {card.left+D(72),card.top+D(54),card.right-D(24),card.bottom-D(18)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            }
            y=card.bottom+D(24);
        } else {
            const int row_height=D(82);
            for(std::size_t i=0;i<named_sessions_.size();++i){
                const auto& session=named_sessions_[i];
                const RECT row{left,y+static_cast<int>(i)*row_height,right,y+static_cast<int>(i+1)*row_height-D(8)};
                if(row.bottom<=content.top || row.top>=content.bottom)continue;
                const UINT command=SessionBase+static_cast<UINT>(i);const bool hot=hot_native_command_==command;
                Rounded(dc,row,hot?kHover:kSurface,hot?MixChromeColor(kBorder,kAccent,25):kBorder,D(16));
                Icon(dc,{row.left+D(18),row.top+D(22),row.left+D(44),row.top+D(48)},Glyph::Folder,kAccent);
                Text(dc,Wide(session.name),{row.left+D(58),row.top+D(8),row.right-D(120),row.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                std::wstring detail=std::to_wstring(session.tabs.size())+L" tab"+(session.tabs.size()==1?L"":L"s");
                if(session.created_ms)detail+=L" • saved "+HistoryDateLabel(session.created_ms)+L" "+HistoryTimeLabel(session.created_ms);
                Text(dc,detail,{row.left+D(58),row.top+D(34),row.right-D(120),row.bottom-D(8)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                const RECT remove{row.right-D(102),row.top+D(18),row.right-D(14),row.bottom-D(18)};
                const UINT remove_command=DeleteSessionBase+static_cast<UINT>(i);const bool remove_hot=hot_native_command_==remove_command;
                Rounded(dc,remove,remove_hot?MixChromeColor(kSurface,kAccent,12):kSurface,remove_hot?MixChromeColor(kBorder,kAccent,34):kBorder,D(18));
                Text(dc,L"Delete",remove,small_font_,remove_hot?kInk:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                add_action(row,command);add_action(remove,remove_command);
            }
            y+=static_cast<int>(named_sessions_.size())*row_height+D(24);
        }
    } else if (page == InternalPage::Profiles) {
        draw_header(L"Profiles", L"Keep work, personal browsing, cookies, history and bookmarks separated in independent WebCom profiles.");
        draw_button({right-D(156),origin+D(34),right,origin+D(68)},L"Add profile",AddProfile);
        draw_button({left,y,left+D(150),y+D(32)},L"Open all profiles",OpenAllProfiles);
        draw_button({left+D(158),y,std::min(right,left+D(332)),y+D(32)},L"Close other windows",CloseOtherProfiles);
        y += D(40);
        draw_button({left,y,left+D(150),y+D(32)},L"Rename current",RenameProfileDisplayNameCmd);
        draw_button({left+D(158),y,std::min(right,left+D(310)),y+D(32)},L"Export setup",ExportProfileSetupCmd);
        if(state_.profiles.size()>1)draw_button({left+D(318),y,std::min(right,left+D(478)),y+D(32)},L"Delete current",DeleteCurrentProfileCmd);
        y += D(40);
        draw_button({left,y,left+D(150),y+D(32)},L"Import setup",ImportProfileSetupCmd);
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
            const auto display_name=ProfileDisplayName(saved.name,&appearance);
            const auto badge_rgb=ProfileColor(appearance,saved.name);const auto badge=NativeColor(badge_rgb);
            Rounded(dc,{row.left+D(18),row.top+D(18),row.left+D(52),row.top+D(52)},badge,badge,D(22));
            std::wstring initial=display_name.empty()?L"?":display_name.substr(0,1);
            Text(dc,initial,{row.left+D(18),row.top+D(18),row.left+D(52),row.top+D(52)},font_,NativeColor(ContrastInk(badge_rgb)),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,display_name,{row.left+D(66),row.top+D(9),row.right-D(160),row.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            const auto status=current?L"This window":running?L"Running":L"Closed";
            const auto saved_tabs=std::to_wstring(saved.tabs.size())+L" saved tab"+(saved.tabs.size()==1?L"":L"s");
            const auto saved_groups=std::to_wstring(saved.groups.size())+L" group"+(saved.groups.size()==1?L"":L"s");
            const auto saved_workspaces=std::to_wstring(saved.workspaces.empty()?1:saved.workspaces.size())+L" workspace"+((saved.workspaces.empty()?1:saved.workspaces.size())==1?L"":L"s");
            auto detail=std::wstring(status)+L" • "+saved_tabs+L" • "+saved_groups+L" • "+saved_workspaces;
            if(display_name!=Wide(saved.name))detail+=L" • key "+Wide(saved.name);
            Text(dc,detail,{row.left+D(66),row.top+D(34),row.right-D(160),row.bottom-D(9)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            const std::wstring action=current?L"Current":running?L"Focus":L"Open";
            Text(dc,action,{row.right-D(138),row.top+D(16),row.right-D(42),row.bottom-D(16)},small_font_,current?kMuted:kAccent,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            if(!current)Icon(dc,{row.right-D(30),row.top+D(26),row.right-D(14),row.top+D(42)},Glyph::Forward,kMuted);
            add_action(row,command);
        }
        y+=static_cast<int>(state_.profiles.size())*row_height+D(24);
    } else if (page == InternalPage::Shortcuts) {
        draw_header(L"Keyboard Shortcuts", L"WebCom's built-in keyboard reference. Browser-chrome shortcuts work without adding another renderer.");
        struct ShortcutRow { const wchar_t* keys; const wchar_t* action; };
        const std::array<ShortcutRow, 22> shortcuts{{
            {L"Ctrl+L",L"Focus address bar"},{L"Ctrl+T",L"New tab"},{L"Ctrl+W",L"Close current tab"},
            {L"Ctrl+Shift+T",L"Reopen closed tab"},{L"Ctrl+Tab",L"Next tab in workspace"},{L"Ctrl+Shift+Tab",L"Previous tab in workspace"},
            {L"Ctrl+1 … Ctrl+9",L"Jump to tab"},{L"Ctrl+Alt+Left / Right",L"Previous / next workspace"},{L"Ctrl+Alt+1 … 9",L"Jump to workspace"},
            {L"Ctrl+Shift+A",L"Search open tabs"},{L"Ctrl+Shift+P",L"Quick Commands"},{L"Ctrl+Shift+B",L"Toggle bookmarks bar"},
            {L"Ctrl+H",L"History"},{L"Ctrl+J",L"Downloads"},{L"Ctrl+O",L"Open local HTML or PDF"},{L"Ctrl+Shift+O",L"Bookmark manager"},
            {L"Ctrl+Shift+S",L"Screenshot capture menu"},{L"F6 / Shift+F6",L"Cycle focus through tabs, address bar and page"},{L"F9",L"Reader Mode"},{L"F11",L"Full screen"},
            {L"F12",L"Developer tools"},{L"Shift+Esc",L"Memory / performance snapshot"}
        }};
        const int row_h=D(48);
        int visible_rows=0;
        for(const auto& shortcut:shortcuts){
            if(!contains_ci(shortcut.keys,filter_text) && !contains_ci(shortcut.action,filter_text))continue;
            const RECT row{left,y+visible_rows*row_h,right,y+(visible_rows+1)*row_h-D(4)};
            ++visible_rows;
            if(row.bottom<=content.top || row.top>=content.bottom)continue;
            Rounded(dc,row,kSurface,kBorder,D(12));
            Text(dc,shortcut.keys,{row.left+D(16),row.top,row.left+D(260),row.bottom},font_,kAccent,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,shortcut.action,{row.left+D(276),row.top,row.right-D(16),row.bottom},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        }
        if(!filter_text.empty() && visible_rows==0)
            Text(dc,L"No matching shortcuts",{left,y,right,y+D(54)},font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        y+=std::max(1,visible_rows)*row_h+D(24);
    } else if (page == InternalPage::Welcome) {
        draw_header(L"Welcome to WebCom", L"A fast native Windows browser shell with profiles, workspaces, privacy controls and a Chromium/WebView2 engine.");
        const bool default_browser = IsDefaultBrowserNow();
        const RECT status{left,y,right,y+D(92)};
        Rounded(dc,status,default_browser?MixChromeColor(kSurface,kAccent,8):kSurface,kBorder,D(16));
        Icon(dc,{status.left+D(20),status.top+D(30),status.left+D(46),status.top+D(56)},Glyph::Globe,default_browser?kAccent:kMuted);
        Text(dc,default_browser?L"WebCom is your HTTP/HTTPS browser":L"Choose your default browser in Windows",
             {status.left+D(60),status.top+D(12),status.right-D(18),status.top+D(40)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        Text(dc,default_browser?L"Windows currently opens web links with WebCom.":L"WebCom never silently takes over defaults. Windows Settings keeps the final choice under your control.",
             {status.left+D(60),status.top+D(40),status.right-D(18),status.bottom-D(12)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_WORDBREAK|DT_NOPREFIX);
        y=status.bottom+D(12);
        const RECT defaults{left,y,std::min(right,left+D(240)),y+D(42)};
        Rounded(dc,defaults,hot_native_command_==OpenDefaultApps?kHover:kSurface,kBorder,D(12));
        Text(dc,default_browser?L"Open Default Apps":L"Set WebCom as default",defaults,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        add_action(defaults,OpenDefaultApps);
        const RECT updates{defaults.right+D(10),y,std::min(right,static_cast<int>(defaults.right)+D(180)),y+D(42)};
        if(updates.right>updates.left+D(40)){Rounded(dc,updates,hot_native_command_==UpdatesPage?kHover:kSurface,kBorder,D(12));Text(dc,L"Update choices",updates,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(updates,UpdatesPage);}
        const RECT shortcuts{updates.right+D(10),y,std::min(right,static_cast<int>(updates.right)+D(190)),y+D(42)};
        if(shortcuts.right>shortcuts.left+D(40)){Rounded(dc,shortcuts,hot_native_command_==ShortcutsPage?kHover:kSurface,kBorder,D(12));Text(dc,L"Keyboard shortcuts",shortcuts,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(shortcuts,ShortcutsPage);}
        y+=D(64);
        const std::array<std::pair<std::wstring,std::wstring>,3> highlights{{
            {L"Profiles and Workspaces",L"Keep work, personal browsing and projects visually separated without running separate browser products."},
            {L"Privacy controls",L"Inspect cookies, site data and permission exceptions from native WebCom surfaces without exposing secret values."},
            {L"Low-overhead browser chrome",L"History, bookmarks, settings, Reader Mode and management pages are native surfaces instead of permanent web renderers."}
        }};
        for(const auto& item:highlights){
            const RECT card{left,y,right,y+D(82)};Rounded(dc,card,kSurface,kBorder,D(14));
            Text(dc,item.first,{card.left+D(18),card.top+D(8),card.right-D(18),card.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,item.second,{card.left+D(18),card.top+D(34),card.right-D(18),card.bottom-D(10)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            y=card.bottom+D(10);
        }
        y+=D(6);
        const RECT start{left,y,std::min(right,left+D(220)),y+D(44)};
        Rounded(dc,start,hot_native_command_==WelcomeStartBrowsing?kHover:kAccent,kAccent,D(14));
        Text(dc,L"Start browsing",start,font_,kCaption,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        add_action(start,WelcomeStartBrowsing);
        y=start.bottom+D(24);
    } else if (page == InternalPage::Recovery) {
        draw_header(L"Recovery & Reliability", L"Crash evidence, update health, rollback readiness and an explicit local recovery report. Nothing on this page is uploaded automatically.");
        const auto reliability=ReliabilitySnapshotNow();
        const std::wstring previous_session=reliability.previous_session_unclean
            ? L"Unclean shutdown evidence detected on this launch" : L"No unclean-shutdown evidence detected on this launch";
        const std::wstring update_state=Wide(reliability.update_phase.empty()?std::string("none"):reliability.update_phase);
        const std::wstring recovery_state=reliability.recovery_version.empty()?L"No verified recovery installer preserved":L"Verified recovery installer • "+Wide(reliability.recovery_version);
        const std::wstring retry_state=reliability.update_retry_remaining_seconds
            ? L"Temporarily rate-limited • retry in "+std::to_wstring(reliability.update_retry_remaining_seconds)+L" seconds"
            : reliability.update_retry_failures?L"Retry window ready • previous failures: "+std::to_wstring(reliability.update_retry_failures):L"No update-check backoff active";
        const std::array<std::pair<std::wstring,std::wstring>,5> facts{{
            {L"Previous session",previous_session},
            {L"Update transaction",update_state},
            {L"Update health failures",std::to_wstring(reliability.failed_health_starts)+(reliability.rollback_recommended?L" • rollback recommended":L"")},
            {L"Recovery package",recovery_state},
            {L"Update retry",retry_state}
        }};
        for(const auto& fact:facts){
            const RECT row{left,y,right,y+D(64)};Rounded(dc,row,kSurface,kBorder,D(14));
            Text(dc,fact.first,{row.left+D(18),row.top+D(7),row.right-D(18),row.top+D(31)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,fact.second,{row.left+D(18),row.top+D(31),row.right-D(18),row.bottom-D(7)},small_font_,
                reliability.rollback_recommended&&fact.first==L"Update health failures"?kAccent:kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            y=static_cast<int>(row.bottom)+D(8);
        }
        const RECT report{left,y,std::min(right,left+D(220)),y+D(40)};
        Rounded(dc,report,hot_native_command_==ExportRecoveryReport?kHover:kSurface,kBorder,D(12));
        Text(dc,L"Export recovery report",report,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(report,ExportRecoveryReport);
        const RECT diagnostics{static_cast<int>(report.right)+D(10),y,std::min(right,static_cast<int>(report.right)+D(190)),y+D(40)};
        if(diagnostics.right>diagnostics.left+D(40)){Rounded(dc,diagnostics,hot_native_command_==DiagnosticsPage?kHover:kSurface,kBorder,D(12));Text(dc,L"Support diagnostics",diagnostics,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(diagnostics,DiagnosticsPage);}
        const RECT updates{static_cast<int>(diagnostics.right)+D(10),y,std::min(right,static_cast<int>(diagnostics.right)+D(160)),y+D(40)};
        if(updates.right>updates.left+D(40)){Rounded(dc,updates,hot_native_command_==UpdatesPage?kHover:kSurface,kBorder,D(12));Text(dc,L"Updates",updates,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(updates,UpdatesPage);}
        y+=D(56);
        if(reliability.rollback_recommended&&recovery_package_&&recovery_package_->version==update_transaction_.previous_version){
            const RECT rollback{left,y,std::min(right,left+D(300)),y+D(42)};
            Rounded(dc,rollback,hot_native_command_==RollbackPreviousVersion?kHover:kSurface,kAccent,D(12));
            Text(dc,L"Rollback to verified WebCom "+Wide(recovery_package_->version)+L"...",rollback,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            add_action(rollback,RollbackPreviousVersion);y+=D(58);
        }
        const RECT privacy{left,y,right,y+D(112)};Rounded(dc,privacy,MixChromeColor(kSurface,kAccent,5),kBorder,D(14));
        Text(dc,L"Local evidence only",{privacy.left+D(18),privacy.top+D(8),privacy.right-D(18),privacy.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        Text(dc,L"WebCom keeps a bounded rotating runtime log and a tiny clean/unclean session marker. Export recovery report creates a local ZIP only when you ask for it; WebCom has no automatic crash-report upload endpoint in this release.",
             {privacy.left+D(18),privacy.top+D(36),privacy.right-D(18),privacy.bottom-D(10)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
        y=static_cast<int>(privacy.bottom)+D(24);
    } else if (page == InternalPage::Diagnostics) {
        draw_header(L"Support Diagnostics", L"Copyable browser/runtime information for troubleshooting. Browsing URLs, cookies and password values are not included.");
        const RECT copy_button{left,y,right < left+D(156) ? right : left+D(156),y+D(38)};
        const RECT bundle_button{copy_button.right+D(10),y,std::min(right,static_cast<int>(copy_button.right)+D(180)),y+D(38)};
        const RECT folder_button{bundle_button.right+D(10),y,std::min(right,static_cast<int>(bundle_button.right)+D(170)),y+D(38)};
        Rounded(dc,copy_button,hot_native_command_==CopyDiagnostics?kHover:kSurface,kBorder,D(12));
        Text(dc,L"Copy diagnostics",copy_button,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(copy_button,CopyDiagnostics);
        if(bundle_button.right>bundle_button.left+D(40)){
            Rounded(dc,bundle_button,hot_native_command_==ExportDiagnosticsBundle?kHover:kSurface,kBorder,D(12));
            Text(dc,L"Export support ZIP",bundle_button,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(bundle_button,ExportDiagnosticsBundle);
        }
        if(folder_button.right>folder_button.left+D(40)){
            Rounded(dc,folder_button,hot_native_command_==OpenDataFolder?kHover:kSurface,kBorder,D(12));
            Text(dc,L"Open data folder",folder_button,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(folder_button,OpenDataFolder);
        }
        y+=D(54);
        const auto diagnostic_text=DiagnosticsTextNow();
        const RECT panel{left,y,right,y+D(420)};
        Rounded(dc,panel,kSurface,kBorder,D(16));
        Text(dc,diagnostic_text,{panel.left+D(18),panel.top+D(14),panel.right-D(18),panel.bottom-D(14)},small_font_,kInk,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
        y=panel.bottom+D(24);
        const RECT channel_row{left,y,right,y+D(68)};
        Rounded(dc,channel_row,hot_native_command_==UpdateChannelMenu?kHover:kSurface,kBorder,D(14));
        Text(dc,L"Release channel",{channel_row.left+D(18),channel_row.top+D(8),channel_row.right-D(42),channel_row.top+D(32)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        const auto channel=Wide(ReleaseChannelName(update_settings_.channel));
        Text(dc,channel==L"beta"?L"Beta • preview releases":L"Stable • recommended public releases",{channel_row.left+D(18),channel_row.top+D(32),channel_row.right-D(42),channel_row.bottom-D(8)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        Icon(dc,{channel_row.right-D(32),channel_row.top+D(26),channel_row.right-D(16),channel_row.top+D(42)},Glyph::Forward,kMuted);add_action(channel_row,UpdateChannelMenu);
        y=channel_row.bottom+D(24);
    } else if (page == InternalPage::Updates) {
        draw_header(L"Updates", L"User-triggered release checks, verified staging and rollback-safe health state. WebCom never installs silently.");
        const std::wstring channel = Wide(ReleaseChannelName(update_settings_.channel));
        const std::wstring phase = Wide(UpdateTransactionPhaseName(update_transaction_.phase));
        const bool endpoint_ready = ManifestUrlForChannel(update_runtime_config_, update_settings_.channel).has_value();
        const bool publisher_ready = IsTrustedPublisherFingerprint(update_runtime_config_.publisher_sha256);
        const std::array<std::pair<std::wstring,std::wstring>,8> facts{{
            {L"Release channel", channel==L"beta"?L"Beta • preview releases":L"Stable • recommended public releases"},
            {L"Update checks", update_settings_.checks_enabled?L"Enabled by your consent":L"Disabled • no release endpoint is contacted"},
            {L"Trusted endpoint", endpoint_ready && publisher_ready ? L"Configured • HTTPS manifest + pinned publisher" : L"Not configured in this build"},
            {L"Automatic downloads", L"Disabled • package download requires an explicit Stage action"},
            {L"Automatic installation", L"Disabled • verified installer launch requires confirmation"},
            {L"Recovery installer", recovery_package_ ? L"Preserved • "+Wide(recovery_package_->version) : L"Not available • created after a successful update health check"},
            {L"Retry / backoff", UpdateRetryRemainingNow()?L"Retry in "+std::to_wstring(UpdateRetryRemainingNow())+L" seconds":update_retry_.consecutive_failures?L"Ready • previous failures: "+std::to_wstring(update_retry_.consecutive_failures):L"No backoff active"},
            {L"Transaction state", phase}
        }};
        for(const auto& fact:facts){
            const RECT row{left,y,right,y+D(60)}; Rounded(dc,row,kSurface,kBorder,D(14));
            Text(dc,fact.first,{row.left+D(18),row.top+D(6),row.right-D(18),row.top+D(29)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,fact.second,{row.left+D(18),row.top+D(29),row.right-D(18),row.bottom-D(6)},small_font_,kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            y=static_cast<int>(row.bottom)+D(8);
        }
        const RECT checks{left,y,std::min(right,left+D(240)),y+D(40)};
        Rounded(dc,checks,hot_native_command_==ToggleUpdateChecks?kHover:kSurface,kBorder,D(12));
        Text(dc,update_settings_.checks_enabled?L"Disable update checks":L"Enable update checks",checks,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        add_action(checks,ToggleUpdateChecks);
        const RECT channel_button{static_cast<int>(checks.right)+D(10),y,std::min(right,static_cast<int>(checks.right)+D(190)),y+D(40)};
        if(channel_button.right>channel_button.left+D(40)){Rounded(dc,channel_button,hot_native_command_==UpdateChannelMenu?kHover:kSurface,kBorder,D(12));Text(dc,L"Release channel",channel_button,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(channel_button,UpdateChannelMenu);}
        const int check_left=static_cast<int>(channel_button.right)+D(10);
        const RECT check_button{check_left,y,std::min(right,check_left+D(190)),y+D(40)};
        const auto retry_remaining=UpdateRetryRemainingNow();
        const bool can_check=update_settings_.checks_enabled && endpoint_ready && publisher_ready && !update_checking_ && !update_staging_ && retry_remaining==0;
        if(check_button.right>check_button.left+D(40)){
            Rounded(dc,check_button,hot_native_command_==CheckForUpdates&&can_check?kHover:kSurface,kBorder,D(12));
            Text(dc,update_checking_?L"Checking...":retry_remaining?L"Retry in "+std::to_wstring(retry_remaining)+L"s":L"Check for updates",check_button,font_,can_check?kAccent:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            if(can_check)add_action(check_button,CheckForUpdates);
        }
        y+=D(56);
        if(!update_runtime_status_.empty()){
            const RECT status{left,y,right,y+D(72)};Rounded(dc,status,MixChromeColor(kSurface,kAccent,4),kBorder,D(14));
            Text(dc,L"Update status",{status.left+D(18),status.top+D(7),status.right-D(18),status.top+D(32)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,update_runtime_status_,{status.left+D(18),status.top+D(32),status.right-D(18),status.bottom-D(8)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            y=static_cast<int>(status.bottom)+D(10);
        }
        if(available_update_ && update_transaction_.phase==UpdateTransactionPhase::None){
            const RECT available{left,y,right,y+D(92)};Rounded(dc,available,kSurface,kBorder,D(14));
            Text(dc,L"Available: WebCom "+Wide(available_update_->version),{available.left+D(18),available.top+D(8),available.right-D(18),available.top+D(35)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,L"Download is still optional. Stage downloads the installer and verifies exact size, SHA-256, Authenticode trust and the pinned publisher before keeping it.",{available.left+D(18),available.top+D(36),available.right-D(210),available.bottom-D(8)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            const RECT stage{std::max(left,static_cast<int>(available.right)-D(190)),available.top+D(26),available.right-D(18),available.top+D(66)};
            const bool can_stage=!update_staging_&&!update_checking_;
            Rounded(dc,stage,hot_native_command_==StageAvailableUpdate&&can_stage?kHover:kSurface,kBorder,D(12));
            Text(dc,update_staging_?L"Verifying...":L"Stage verified update",stage,font_,can_stage?kAccent:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            if(can_stage)add_action(stage,StageAvailableUpdate);
            y=static_cast<int>(available.bottom)+D(10);
        }
        if(update_transaction_.phase!=UpdateTransactionPhase::None){
            const RECT state_panel{left,y,right,y+D(142)}; Rounded(dc,state_panel,kSurface,kBorder,D(14));
            std::wstring detail=L"Target: "+Wide(update_transaction_.target_version)+L"  •  Previous: "+Wide(update_transaction_.previous_version)+L"\n";
            detail+=L"Package: "+std::to_wstring(update_transaction_.package_size)+L" bytes";
            if(update_transaction_.phase==UpdateTransactionPhase::AwaitingHealthCheck)detail+=L"  •  Failed health starts: "+std::to_wstring(update_transaction_.failed_health_starts);
            if(UpdateRollbackRecommended(update_transaction_))detail+=L"\nRollback is recommended before another update attempt.";
            Text(dc,L"Staged update transaction",{state_panel.left+D(18),state_panel.top+D(8),state_panel.right-D(18),state_panel.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,detail,{state_panel.left+D(18),state_panel.top+D(36),state_panel.right-D(18),state_panel.bottom-D(46)},small_font_,UpdateRollbackRecommended(update_transaction_)?kAccent:kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            const int button_top=static_cast<int>(state_panel.bottom)-D(40);
            if(update_transaction_.phase==UpdateTransactionPhase::VerifiedPackage){
                const RECT install{state_panel.left+D(18),button_top,state_panel.left+D(210),state_panel.bottom-D(8)};
                Rounded(dc,install,hot_native_command_==InstallStagedUpdate?kHover:kSurface,kBorder,D(12));
                Text(dc,L"Install verified update...",install,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(install,InstallStagedUpdate);
             } else if(EvaluateRecoveryReadiness(update_transaction_,recovery_package_,kWebComVersion,update_runtime_config_.publisher_sha256).ready()) {
                const RECT rollback{state_panel.left+D(18),button_top,state_panel.left+D(230),state_panel.bottom-D(8)};
                Rounded(dc,rollback,hot_native_command_==RollbackPreviousVersion?kHover:kSurface,kBorder,D(12));
                Text(dc,L"Rollback to "+Wide(recovery_package_->version)+L"...",rollback,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(rollback,RollbackPreviousVersion);
            }
            const RECT clear_button{std::max(left,static_cast<int>(state_panel.right)-D(220)),button_top,state_panel.right-D(18),state_panel.bottom-D(8)};
            Rounded(dc,clear_button,hot_native_command_==ClearUpdateTransaction?kHover:kSurface,kBorder,D(12));
            Text(dc,L"Clear staged update state",clear_button,font_,kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);add_action(clear_button,ClearUpdateTransaction);
            y=static_cast<int>(state_panel.bottom)+D(12);
        }
        const RECT security{left,y,right,y+D(118)}; Rounded(dc,security,MixChromeColor(kSurface,kAccent,5),kBorder,D(14));
        Text(dc,L"Security boundary",{security.left+D(18),security.top+D(8),security.right-D(18),security.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        Text(dc,L"A release is accepted only when the HTTPS manifest matches this build's selected channel and pinned publisher certificate. Stable additionally rejects prerelease version labels. The downloaded installer is then checked again for exact size, SHA-256, Windows Authenticode trust, product identity and the same publisher before staging; it is re-verified once more before an explicit installer launch.",{security.left+D(18),security.top+D(36),security.right-D(18),security.bottom-D(10)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
        y=static_cast<int>(security.bottom)+D(24);
    } else if (page == InternalPage::Extensions) {
        draw_header(L"Extensions", L"Chromium/WebView2 extensions for the " + ProfileDisplayName(profile_name_,&appearance_) + L" profile. Extension state is isolated per profile.");
        const RECT load{left,y,std::min(right,left+D(190)),y+D(40)};
        Rounded(dc,load,hot_native_command_==ExtensionsLoadUnpacked?kHover:kSurface,kBorder,D(12));
        Text(dc,L"Load unpacked...",load,font_,extensions_supported_?kAccent:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        if(extensions_supported_)add_action(load,ExtensionsLoadUnpacked);
        const RECT refresh{static_cast<int>(load.right)+D(10),y,std::min(right,static_cast<int>(load.right)+D(150)),y+D(40)};
        if(refresh.right>refresh.left+D(40)){
            Rounded(dc,refresh,hot_native_command_==ExtensionsRefresh?kHover:kSurface,kBorder,D(12));
            Text(dc,extensions_loading_?L"Loading...":L"Refresh",refresh,font_,extensions_loading_?kMuted:kAccent,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            if(!extensions_loading_)add_action(refresh,ExtensionsRefresh);
        }
        y+=D(56);
        const RECT notice{left,y,right,y+D(112)};
        Rounded(dc,notice,MixChromeColor(kSurface,kAccent,5),kBorder,D(14));
        Text(dc,L"WebView2 extension compatibility",{notice.left+D(18),notice.top+D(8),notice.right-D(18),notice.top+D(34)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        Text(dc,L"WebCom can load unpacked Chromium/Chrome extensions and persist them in this profile. Content scripts, background/service-worker behavior and extension APIs supported by WebView2 can run. Chrome-style toolbar icons, badges and popup entry points are not supplied by WebView2, so extensions that depend on that UI can be partially usable.",
             {notice.left+D(18),notice.top+D(36),notice.right-D(18),notice.bottom-D(10)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
        y=static_cast<int>(notice.bottom)+D(12);
        if(!extensions_status_.empty()){
            const RECT status{left,y,right,y+D(58)};Rounded(dc,status,kSurface,kBorder,D(12));
            Text(dc,extensions_status_,{status.left+D(16),status.top+D(8),status.right-D(16),status.bottom-D(8)},small_font_,extensions_supported_?kMuted:kAccent,DT_LEFT|DT_VCENTER|DT_WORDBREAK|DT_NOPREFIX);
            y=static_cast<int>(status.bottom)+D(10);
        }
        if(extensions_.empty() && !extensions_loading_){
            const RECT empty{left,y,right,y+D(82)};Rounded(dc,empty,kSurface,kBorder,D(14));
            Text(dc,L"No installed extensions to show",{empty.left+D(18),empty.top+D(10),empty.right-D(18),empty.top+D(36)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            Text(dc,extension_sources_.empty()?L"Choose Load unpacked and select the top-level folder containing manifest.json. WebView2 references that folder directly, so keep it in place.":L"WebView2 currently reports no installed extensions. Saved unpacked sources are shown below so you can reload them if their files changed.",{empty.left+D(18),empty.top+D(38),empty.right-D(18),empty.bottom-D(10)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);
            y=static_cast<int>(empty.bottom)+D(12);
        } else {
            for(std::size_t i=0;i<extensions_.size();++i){
                const auto& extension=extensions_[i];
                int source_index=-1;
                for(std::size_t source=0;source<extension_sources_.size();++source)
                    if(extension_sources_[source].extension_id==Utf8(extension.id)){source_index=static_cast<int>(source);break;}
                const bool compact=(right-left)<D(690);
                const RECT row{left,y,right,y+D(compact?174:116)};Rounded(dc,row,kSurface,kBorder,D(14));
                const int text_right=static_cast<int>(row.right)-D(18);
                Text(dc,extension.name,{row.left+D(18),row.top+D(8),text_right,row.top+D(35)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                const std::wstring state=extension.enabled?L"Enabled":L"Disabled";
                std::wstring source_state=L"  •  source not recorded";
                bool source_needs_attention=false;
                if(source_index>=0 && static_cast<std::size_t>(source_index)<extension_source_health_.size()){
                    const auto& health=extension_source_health_[static_cast<std::size_t>(source_index)];
                    if(!health.manifest.valid){source_state=L"  •  source unavailable";source_needs_attention=true;}
                    else if(!health.snapshot.available){source_state=L"  •  source scan unavailable";source_needs_attention=true;}
                    else if(health.changed){source_state=L"  •  source changed • reload recommended";source_needs_attention=true;}
                    else if(!health.baseline_known)source_state=L"  •  source baseline pending • reload once";
                    else source_state=L"  •  source unchanged";
                }
                Text(dc,state+L"  •  ID: "+extension.id+source_state,{row.left+D(18),row.top+D(36),text_right,row.top+D(64)},small_font_,source_needs_attention?kInk:(extension.enabled?kAccent:kMuted),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                const int first_button_top=compact?static_cast<int>(row.bottom)-D(94):static_cast<int>(row.bottom)-D(47);
                int x=static_cast<int>(row.left)+D(18), button_top=first_button_top;
                const auto draw_extension_button=[&](UINT command,const wchar_t* label,int width,bool enabled=true){
                    if(compact && x+D(width)>static_cast<int>(row.right)-D(18)){x=static_cast<int>(row.left)+D(18);button_top+=D(44);}
                    const RECT button{x,button_top,x+D(width),button_top+D(36)};x=static_cast<int>(button.right)+D(8);
                    Rounded(dc,button,enabled&&hot_native_command_==command?kHover:kSurface,kBorder,D(10));
                    Text(dc,label,button,font_,enabled?kAccent:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                    if(enabled)add_action(button,command);
                };
                draw_extension_button(ExtensionDetailsBase+static_cast<UINT>(i),L"Details",92);
                draw_extension_button(ExtensionToggleBase+static_cast<UINT>(i),extension.enabled?L"Disable":L"Enable",92);
                if(source_index>=0)draw_extension_button(ExtensionReloadBase+static_cast<UINT>(source_index),L"Reload",92);
                if(source_index>=0)draw_extension_button(ExtensionOpenFolderBase+static_cast<UINT>(source_index),L"Open folder",110);
                draw_extension_button(ExtensionRemoveBase+static_cast<UINT>(i),L"Remove",92);
                y=static_cast<int>(row.bottom)+D(10);
            }
        }
        std::vector<std::size_t> missing_sources;
        for(std::size_t source=0;source<extension_sources_.size();++source){
            bool installed=false;
            for(const auto& extension:extensions_)if(Utf8(extension.id)==extension_sources_[source].extension_id){installed=true;break;}
            if(!installed)missing_sources.push_back(source);
        }
        if(!missing_sources.empty()){
            y+=D(8);
            Text(dc,L"Known unpacked sources not currently installed",{left,y,right,y+D(30)},heading_font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);y+=D(36);
            Text(dc,L"WebView2 can remove an unpacked extension when files inside its source folder change. WebCom keeps the source mapping so you can inspect or reload it explicitly.",{left,y,right,y+D(48)},small_font_,kMuted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_NOPREFIX);y+=D(56);
            for(const auto source_index:missing_sources){
                const auto& source=extension_sources_[source_index];
                const ExtensionSourceHealth* health=source_index<extension_source_health_.size()?&extension_source_health_[source_index]:nullptr;
                const bool loadable=health&&health->manifest.valid;
                const bool compact=(right-left)<D(620);
                const RECT row{left,y,right,y+D(compact?184:146)};Rounded(dc,row,kSurface,kBorder,D(14));
                const std::wstring label=loadable?Wide(health->manifest.name)+L"  •  "+Wide(health->manifest.version):source.folder.filename().wstring();
                Text(dc,label,{row.left+D(18),row.top+D(8),row.right-D(18),row.top+D(35)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                Text(dc,L"ID: "+Wide(source.extension_id)+L"  •  "+source.folder.wstring(),{row.left+D(18),row.top+D(36),row.right-D(18),row.top+D(62)},small_font_,loadable?kMuted:kAccent,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                std::wstring health_text=L"Source diagnostics unavailable";
                if(health){
                    if(!health->manifest.valid)health_text=L"Not loadable: "+Wide(health->manifest.error);
                    else if(!health->snapshot.available)health_text=L"Change scan unavailable: "+Wide(health->snapshot.error);
                    else if(health->changed)health_text=L"Source changed since the last successful load • reload recommended";
                    else if(!health->baseline_known)health_text=L"No change baseline recorded yet • reload once to establish it";
                    else health_text=L"Source unchanged since the last successful load";
                }
                Text(dc,health_text,{row.left+D(18),row.top+D(63),row.right-D(18),row.top+D(90)},small_font_,health&&health->changed?kInk:kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                int x=static_cast<int>(row.left)+D(18),button_top=static_cast<int>(row.top)+D(96);
                const auto draw_source_button=[&](UINT command,const wchar_t* text,int width,bool enabled=true){
                    if(compact&&x+D(width)>static_cast<int>(row.right)-D(18)){x=static_cast<int>(row.left)+D(18);button_top+=D(42);}
                    const RECT button{x,button_top,x+D(width),button_top+D(34)};x=static_cast<int>(button.right)+D(8);
                    Rounded(dc,button,enabled&&hot_native_command_==command?kHover:kSurface,kBorder,D(10));
                    Text(dc,text,button,font_,enabled?kAccent:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                    if(enabled)add_action(button,command);
                };
                draw_source_button(ExtensionSourceDetailsBase+static_cast<UINT>(source_index),L"Details",92);
                draw_source_button(ExtensionReloadBase+static_cast<UINT>(source_index),L"Reload source",112,loadable);
                draw_source_button(ExtensionOpenFolderBase+static_cast<UINT>(source_index),L"Open folder",110);
                draw_source_button(ExtensionForgetSourceBase+static_cast<UINT>(source_index),L"Forget source",110);
                y=static_cast<int>(row.bottom)+D(10);
            }
        }
        y+=D(12);
    } else if (page == InternalPage::Settings) {
        draw_header(L"Settings", L"WebCom settings for the " + ProfileDisplayName(profile_name_,&appearance_) + L" profile. Changes save automatically.");
        struct SettingRow { const wchar_t* title; std::wstring detail; UINT command; Glyph glyph; };
        const auto* credential_preferences = CurrentSavedProfile();
        const bool save_passwords = credential_preferences ? credential_preferences->save_passwords : state_.save_passwords;
        const bool general_autofill = credential_preferences ? credential_preferences->general_autofill : true;
        const std::wstring credential_detail =
            std::wstring(save_passwords ? L"Save passwords on" : L"Save passwords off") +
            L" • " + (general_autofill ? L"form autofill on" : L"form autofill off");
        std::wstring startup_detail=L"Continue where you left off";
        if(credential_preferences){
            if(credential_preferences->startup_mode==StartupMode::NewTab)startup_detail=L"Open a clean New Tab";
            else if(credential_preferences->startup_mode==StartupMode::SpecificPages)
                startup_detail=credential_preferences->startup_pages.empty()?L"Specific pages • none configured":
                    L"Open "+std::to_wstring(credential_preferences->startup_pages.size())+L" specific page"+(credential_preferences->startup_pages.size()==1?L"":L"s");
        }
        const std::array<SettingRow, 20> rows{{
            {L"Appearance", appearance_.high_contrast ? L"High contrast chrome • profile appearance" : appearance_.reduce_motion ? L"Theme and profile color • reduced motion on" : L"Theme, profile color and visual preferences", AppearanceMenu, Glyph::Spark},
            {L"Workspaces", std::to_wstring(workspaces_.size()) + L" workspace" + (workspaces_.size()==1?L"":L"s") + L" • current: " + active_workspace_, WorkspaceMenu, Glyph::Folder},
            {L"Tab layout", vertical_tabs_ ? (vertical_tabs_collapsed_ ? L"Vertical tabs • collapsed rail" : L"Vertical tabs • expanded rail") : L"Horizontal tabs • classic browser layout", ToggleVerticalTabs, Glyph::Folder},
            {L"Performance", state_.memory_mode == MemoryMode::Saver ? L"Memory Saver • sleeps eligible tabs after 1 minute" : L"Balanced • sleeps eligible tabs after 5 minutes", ToggleSaver, Glyph::Reload},
            {L"On startup", startup_detail, StartupMenu, Glyph::Reload},
            {L"Passwords and autofill", credential_detail, PasswordMenu, Glyph::Shield},
            {L"Profiles", std::to_wstring(state_.profiles.size()) + L" local profile" + (state_.profiles.size()==1?L"":L"s") + L" • multiple windows can run together", ProfileManager, Glyph::User},
            {L"Extensions", extensions_.empty()?std::wstring(L"Chromium extensions • per-profile • load unpacked folders"):std::to_wstring(extensions_.size())+L" installed extension"+(extensions_.size()==1?L"":L"s")+L" • per-profile", ExtensionsPage, Glyph::Spark},
            {L"Default browser", IsDefaultBrowserNow()?L"WebCom currently handles HTTP and HTTPS links":L"Choose WebCom from Windows Default Apps", OpenDefaultApps, Glyph::Globe},
            {L"Session recovery", L"Keeps a last known-good backup and asks whether to keep recovered tabs or start fresh", 0, Glyph::Reload},
            {L"Saved sessions", std::to_wstring(named_sessions_.size()) + L" named session" + (named_sessions_.size()==1?L"":L"s") + L" • independent from crash recovery", SessionsManager, Glyph::Folder},
            {L"Site permissions", std::to_wstring(site_permissions_.size()) + L" saved override" + (site_permissions_.size()==1?L"":L"s") + L" • shield shows connection security, cookies and permissions", 0, Glyph::Shield},
            {L"Bookmark manager", std::to_wstring(bookmarks_.size()) + L" saved bookmark" + (bookmarks_.size()==1?L"":L"s"), BookmarkManager, Glyph::Star},
            {L"Bookmarks bar", show_bookmarks_ ? L"Shown below the address bar" : L"Hidden", ToggleBookmarks, Glyph::Star},
            {L"Profile color", L"Change the color used to identify this profile", ProfileColorMenu, Glyph::User},
            {L"Privacy and site data", std::to_wstring(privacy_cookie_domains_.size()) + L" cookie domain" + (privacy_cookie_domains_.size()==1?L"":L"s") + L" • " + std::to_wstring(PrivacyPermissionOrigins().size()) + L" permission origin" + (PrivacyPermissionOrigins().size()==1?L"":L"s"), PrivacyManager, Glyph::Globe},
            {L"Updates", (update_settings_.channel==ReleaseChannel::Beta ? L"Beta" : L"Stable") + std::wstring(L" • checks ") + (update_settings_.checks_enabled?L"enabled":L"disabled") + L" • installs explicit", UpdatesPage, Glyph::Reload},
            {L"Recovery & reliability", UpdateRollbackRecommended(update_transaction_)?L"Rollback recommended • verified recovery controls":(session_.lock()&&session_.lock()->PreviousRuntimeUnclean()?L"Previous unclean shutdown evidence • local recovery report":L"Crash evidence, recovery installer and local reliability report"), RecoveryPage, Glyph::Shield},
            {L"Support diagnostics", L"Runtime, profile, workspace and browser-state information without browsing URLs or secrets", DiagnosticsPage, Glyph::Shield},
            {L"Keyboard shortcuts", L"A searchable reference for navigation, workspaces, tabs, Reader Mode and capture", ShortcutsPage, Glyph::Search}
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
    const int content_width_dip=MulDiv(static_cast<int>(content.right-content.left),96,static_cast<int>(dpi_));
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
            const auto layout=MakeChromePrivateLayout(content_width_dip,dip_height);
            const int left=content.left+D(layout.left),width=D(layout.width),origin=content.top-private_scroll_;
            auto y=[&](int dip){return origin+D(dip);};
            const int content_center=content.left+static_cast<int>(content.right-content.left)/2;
            PrivateIcon(dc,{content_center-D(layout.icon)/2,y(layout.top),content_center+D(layout.icon)/2,y(layout.top+layout.icon)},true);
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
            const auto layout=MakeChromeHomeLayout(content_width_dip,dip_height);
            const int top=content.top+D(layout.logo_top);
            const int content_center=content.left+static_cast<int>(content.right-content.left)/2;
            if(google_logo_){
                Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.DrawImage(google_logo_.get(),content_center-D(layout.logo_width)/2,top,D(layout.logo_width),D(layout.logo_height));
            }else Text(dc,L"Google",{content.left,top,content.right,top+D(layout.logo_height)},logo_font_,kInk,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
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
            const auto current_display=ProfileDisplayName(profile_name_,&appearance_);
            std::wstring initial=current_display.empty()?L"?":current_display.substr(0,1);
            if(!initial.empty())CharUpperBuffW(initial.data(),1);
            Text(draw.hDC,initial,avatar,small_font_,profile_ink_,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }
        if(!compact)Text(draw.hDC,private_mode?L"Incognito":ProfileDisplayName(profile_name_,&appearance_),
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
    if (appearance_.reduce_motion || !tab_reflow_start_ || tab_reflow_from_.empty()) return target;
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

std::vector<BrowserApp::Impl::WorkspaceDropTarget> BrowserApp::Impl::WorkspaceDropTargets(RECT bounds) const {
    std::vector<WorkspaceDropTarget> targets;
    if (!tab_dragging_ || workspaces_.size() < 2 || bounds.right <= bounds.left || bounds.bottom <= bounds.top) return targets;
    std::wstring source_workspace;
    for (const auto& tab : tabs_) if (tab->id == dragged_tab_id_) { source_workspace = tab->workspace; break; }
    std::vector<std::size_t> candidates;
    for (std::size_t i=0;i<workspaces_.size();++i)
        if (_wcsicmp(Wide(workspaces_[i].name).c_str(), source_workspace.c_str()) != 0) candidates.push_back(i);
    if (candidates.empty()) return targets;
    if (vertical_tabs_) {
        const int gap=D(5), height=D(vertical_tabs_collapsed_?30:32);
        const int available=std::max(0,static_cast<int>(bounds.bottom-bounds.top)-D(58));
        const std::size_t maximum=static_cast<std::size_t>(std::max(1,available/std::max(1,height+gap)));
        if(candidates.size()>maximum)candidates.resize(maximum);
        int y=static_cast<int>(bounds.bottom)-D(8)-static_cast<int>(candidates.size())*(height+gap)+gap;
        for(const auto index:candidates){
            RECT rect{D(7),y,static_cast<int>(bounds.right)-D(7),y+height};
            targets.push_back({index,rect});y+=height+gap;
        }
    } else {
        const int gap=D(5);const int available=std::max(D(72),static_cast<int>(bounds.right-bounds.left)-D(16));
        const std::size_t maximum=static_cast<std::size_t>(std::max(1,available/std::max(D(72),1)));
        if(candidates.size()>maximum)candidates.resize(maximum);
        const int count=static_cast<int>(candidates.size());
        const int width=std::clamp((available-gap*(count-1))/std::max(1,count),D(64),D(112));
        const int total=width*count+gap*std::max(0,count-1);
        int x=std::max(D(8),static_cast<int>(bounds.right)-D(8)-total);
        for(const auto index:candidates){
            targets.push_back({index,{x,D(7),x+width,D(35)}});x+=width+gap;
        }
    }
    return targets;
}

void BrowserApp::Impl::PaintWorkspaceDropTargets(HDC dc,const std::vector<WorkspaceDropTarget>& targets) const {
    for(const auto& target:targets){
        if(target.workspace_index>=workspaces_.size())continue;
        const auto& workspace=workspaces_[target.workspace_index];
        const COLORREF color=NativeColor(kProfileColors[workspace.color%kProfileColors.size()]);
        const bool hot=drag_workspace_target_==static_cast<int>(target.workspace_index);
        Rounded(dc,target.rect,hot?MixChromeColor(kCaption,color,28):MixChromeColor(kCaption,color,12),
                hot?color:MixChromeColor(kBorder,color,48),D(14));
        if(vertical_tabs_collapsed_){
            const std::wstring initial=Wide(workspace.name).empty()?L"W":Wide(workspace.name).substr(0,1);
            Text(dc,initial,target.rect,small_font_,hot?kInk:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }else{
            Text(dc,Wide(workspace.name),{target.rect.left+D(8),target.rect.top,target.rect.right-D(8),target.rect.bottom},
                 small_font_,hot?kInk:kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        }
    }
}

void BrowserApp::Impl::PaintTabs() {
    PAINTSTRUCT paint{};HDC output=BeginPaint(tab_strip_,&paint);
    RECT bounds{};GetClientRect(tab_strip_,&bounds);
    HDC buffered=native_chrome_.tabs.Begin(output,static_cast<int>(bounds.right),static_cast<int>(bounds.bottom));
    HDC dc=buffered?buffered:output;
    if(buffered)IntersectClipRect(dc,paint.rcPaint.left,paint.rcPaint.top,paint.rcPaint.right,paint.rcPaint.bottom);
    Fill(dc,bounds,kCaption);
    if(vertical_tabs_){
        const auto* workspace=FindWorkspace(active_workspace_);
        const auto color_index=workspace?workspace->color:0;
        const COLORREF workspace_color=NativeColor(kProfileColors[color_index%kProfileColors.size()]);
        if(vertical_tabs_collapsed_){
            Rounded(dc,{D(25),D(9),D(39),D(23)},workspace_color,workspace_color,D(14));
            std::wstring initial=active_workspace_.empty()?L"W":active_workspace_.substr(0,1);
            Text(dc,initial,{D(10),D(19),D(54),D(39)},small_font_,kMuted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }else{
            Rounded(dc,{D(10),D(10),D(16),D(32)},workspace_color,workspace_color,D(6));
            Text(dc,active_workspace_,{D(24),D(6),D(184),D(36)},font_,kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            const auto visible_count=VisibleTabIndices().size();
            Text(dc,std::to_wstring(visible_count),{D(188),D(6),D(226),D(36)},small_font_,kMuted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }
        Fill(dc,{D(8),D(42),bounds.right-D(8),D(43)},kBorder);
        for(std::size_t i=0;i<tabs_.size();++i){
            if(i>=tab_rects_.size())break;
            const RECT r=DisplayTabRect(i);if(IsRectEmpty(&r))continue;
            const auto& tab=*tabs_[i];const bool selected=i==active_,hot=static_cast<int>(i)==hot_tab_;
            const bool split=split_tab_id_==tab.id;
            if(selected || hot || split){
                COLORREF fill=selected?kFrame:hot?kHover:MixChromeColor(kCaption,kAccent,10);
                COLORREF edge=split?MixChromeColor(kBorder,kAccent,60):fill;
                Rounded(dc,r,fill,edge,D(12));
            }
            if(!tab.group.empty()){
                const auto* meta=FindTabGroup(tab.group);const auto hash=std::hash<std::wstring>{}(tab.group);
                const auto group_color_index=meta?meta->color:static_cast<std::uint8_t>(hash%kProfileColors.size());
                const COLORREF color=NativeColor(kProfileColors[group_color_index%kProfileColors.size()]);
                Rounded(dc,{r.left+D(2),r.top+D(7),r.left+D(5),r.bottom-D(7)},color,color,D(3));
            }
            const int icon_top=r.top+std::max(0,static_cast<int>(r.bottom-r.top-D(18))/2);
            const int icon_left=vertical_tabs_collapsed_ ? r.left+std::max(0,static_cast<int>(r.right-r.left-D(18))/2) : r.left+D(12);
            const RECT icon{icon_left,icon_top,icon_left+D(18),icon_top+D(18)};
            if(tab.muted)Icon(dc,icon,Glyph::Muted,kMuted);
            else if(tab.activity.audible)Icon(dc,icon,Glyph::Speaker,kAccent);
            else if(tab.private_mode)PrivateIcon(dc,{icon.left-D(2),icon.top-D(2),icon.right+D(2),icon.bottom+D(2)},false);
            else if(tab.activity.loading || tab.creating)Icon(dc,icon,Glyph::Reload,kAccent);
            else if(tab.favicon){Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.DrawImage(tab.favicon.get(),static_cast<int>(icon.left),static_cast<int>(icon.top),static_cast<int>(icon.right-icon.left),static_cast<int>(icon.bottom-icon.top));}
            else if(IsHome(tab.url))ChromeGoogleMark(dc,icon);
            else Icon(dc,icon,tab.activity.pinned?Glyph::Star:Glyph::Globe,tab.activity.suspended?MixChromeColor(kCaption,kMuted,70):kMuted,tab.activity.pinned);
            std::wstring title=IsHome(tab.url)?(tab.private_mode?L"New Incognito Tab":L"New Tab"):tab.title;
            if(!tab.group.empty())title=tab.group+L"  ·  "+title;
            const bool show_close=!vertical_tabs_collapsed_ && !tab.activity.pinned && (selected || hot || r.right-r.left>=D(180));
            if(!vertical_tabs_collapsed_)
                Text(dc,title,{r.left+D(40),r.top,r.right-D(show_close?38:12),r.bottom},small_font_,selected?kInk:kMuted);
            if(split){
                if(vertical_tabs_collapsed_)Rounded(dc,{r.right-D(10),r.top+D(8),r.right-D(5),r.top+D(13)},kAccent,kAccent,D(6));
                else Rounded(dc,{r.right-D(show_close?48:22),r.top+D(15),r.right-D(show_close?42:16),r.top+D(21)},kAccent,kAccent,D(6));
            }
            if(show_close){
                RECT close{r.right-D(32),r.top+D(6),r.right-D(8),r.bottom-D(6)};
                if((hot && hot_close_ && !tab_dragging_) || pressed_close_tab_id_==tab.id)
                    Rounded(dc,close,pressed_close_tab_id_==tab.id?MixChromeColor(kHover,kInk,12):kHover,kHover,D(24));
                InflateRect(&close,-D(6),-D(6));Icon(dc,close,Glyph::Close,selected?kInk:kMuted);
            }
        }
        if(tab_dragging_ && drag_target_index_>=0 && static_cast<std::size_t>(drag_target_index_)<tab_rects_.size()){
            std::size_t source=tabs_.size();for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->id==dragged_tab_id_){source=i;break;}
            const auto target=static_cast<std::size_t>(drag_target_index_);const RECT target_rect=DisplayTabRect(target);
            if(source<tabs_.size() && source!=target && !IsRectEmpty(&target_rect)){
                const int y=target<source?target_rect.top-D(2):target_rect.bottom+D(2);
                Rounded(dc,{target_rect.left+D(8),y-D(1),target_rect.right-D(8),y+D(2)},kAccent,kAccent,D(3));
            }
        }
        PaintWorkspaceDropTargets(dc,WorkspaceDropTargets(bounds));
        if(buffered)native_chrome_.tabs.Present(output,paint.rcPaint);
        EndPaint(tab_strip_,&paint);return;
    }
    auto paint_tab=[&](std::size_t i){
        if(i>=tab_rects_.size())return;
        const RECT r=DisplayTabRect(i);if(r.right<=r.left || r.left>=bounds.right)return;
        const auto& tab=*tabs_[i];const bool selected=i==active_,hot=static_cast<int>(i)==hot_tab_;
        unsigned hover_amount=hot?100u:0u;
        if(!appearance_.reduce_motion && hover_animation_start_)
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
            else if(!appearance_.reduce_motion && tab_transition_until_){
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
    PaintWorkspaceDropTargets(dc,WorkspaceDropTargets(bounds));
    if(buffered)native_chrome_.tabs.Present(output,paint.rcPaint);
    EndPaint(tab_strip_,&paint);
}

void BrowserApp::Impl::SyncTabs() {
    if (!tab_strip_) return;
    if (auto* tab=Active()) {
        const auto visible=VisibleTabIndices();
        std::size_t position=0;
        const auto found=std::find(visible.begin(),visible.end(),active_);
        if(found!=visible.end())position=static_cast<std::size_t>(found-visible.begin())+1;
        std::wstring name=L"Tabs. Workspace "+active_workspace_+L". Selected tab "+
            std::to_wstring(position?position:1)+L" of "+std::to_wstring(std::max<std::size_t>(1,visible.size()))+
            L": "+tab->title+L". Ctrl+Tab changes tab. F6 moves focus through browser chrome and page.";
        if(name!=native_chrome_.tab_accessible_label) {
            native_chrome_.tab_accessible_label=std::move(name);
            SetWindowTextW(tab_strip_,native_chrome_.tab_accessible_label.c_str());
            NotifyWinEvent(EVENT_OBJECT_NAMECHANGE,tab_strip_,OBJID_CLIENT,CHILDID_SELF);
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
    for(const auto& folder_path:webcom::BookmarkFolderPaths(bookmarks_,explicit_bookmark_folders_,true)){
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

void BrowserApp::Impl::CreateBookmarkFolder() {
    const auto name=AskName(window_,instance_,&palette_,L"New bookmark folder",L"&Folder path",
        L"Use / for nested folders, for example Work/Research.",L"",L"Create");
    if(!name || name->empty())return;
    auto next=explicit_bookmark_folders_;
    if(!AddBookmarkFolder(next,Utf8(*name))){Note(L"Choose a unique valid folder path.");return;}
    std::string error;
    if(!SaveBookmarkFolders(root_,profile_name_,next,&error)){Notice(L"The bookmark folder could not be saved. "+Wide(error));return;}
    explicit_bookmark_folders_=std::move(next);RebuildBookmarks();native_scroll_=0;QueueUI();Note(L"Bookmark folder created.");
}

void BrowserApp::Impl::DeleteBookmarkFolderTree(std::wstring folder) {
    auto next=explicit_bookmark_folders_;
    if(!RemoveBookmarkFolder(next,bookmarks_,Utf8(folder))){Note(L"Only empty explicit bookmark folders can be removed.");return;}
    std::string error;
    if(!SaveBookmarkFolders(root_,profile_name_,next,&error)){Notice(L"The bookmark folder could not be removed. "+Wide(error));return;}
    explicit_bookmark_folders_=std::move(next);RebuildBookmarks();native_scroll_=0;QueueUI();Note(L"Empty bookmark folder removed.");
}

void BrowserApp::Impl::RenameBookmarkFolderTree(std::wstring folder) {
    if (folder.empty()) return;
    const auto next_name = AskName(window_, instance_, &palette_, L"Rename bookmark folder", L"&Folder path",
        L"Use / to create nested folders, for example Work/Research.", folder.c_str(), L"Rename");
    if (!next_name || next_name->empty()) return;
    auto next_bookmarks = bookmarks_;
    auto next_folders = explicit_bookmark_folders_;
    if (!webcom::RenameBookmarkFolderPath(next_folders, next_bookmarks, Utf8(folder), Utf8(*next_name))) {
        Note(L"Choose a different valid folder path."); return;
    }
    std::string error;
    if (!SaveBookmarks(root_, profile_name_, next_bookmarks, &error)) { Notice(L"The folder could not be renamed. " + Wide(error)); return; }
    if (!SaveBookmarkFolders(root_, profile_name_, next_folders, &error)) { Notice(L"The empty-folder metadata could not be saved. " + Wide(error)); return; }
    bookmarks_ = std::move(next_bookmarks); explicit_bookmark_folders_ = std::move(next_folders);
    RebuildBookmarks(); native_scroll_ = 0; QueueUI(); Note(L"Bookmark folder renamed.");
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
    for(const auto& path:webcom::BookmarkFolderPaths(snapshot,explicit_bookmark_folders_,true))folders.push_back(Wide(path));
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

void BrowserApp::Impl::ShowQuickCommands() {
    if(menu_open_ || closing_ || !window_)return;
    const auto* tab=Active();const bool page=tab && tab->web;
    std::vector<TabSearchItem> items;
    auto add=[&](UINT command,const wchar_t* title,const wchar_t* detail,bool enabled=true){
        if(enabled)items.push_back({command,title,detail,false,false,false,false,false});
    };
    add(NewTab,L"New tab",L"Ctrl+T");
    add(PrivateTab,L"New Incognito tab",L"Ctrl+Shift+N");
    add(ReopenTab,L"Reopen closed tab",L"Ctrl+Shift+T",!closed_tabs_.empty());
    add(OpenFile,L"Open local HTML or PDF",L"Ctrl+O");
    add(TabList,L"Search open tabs",L"Ctrl+Shift+A");
    add(WorkspaceMenu,L"Switch or manage workspaces",active_workspace_.c_str());
    add(StartupMenu,L"On startup",L"Continue, New Tab, or specific pages");
    add(PreviousWorkspace,L"Previous workspace",L"Ctrl+Alt+Left",workspaces_.size()>1);
    add(NextWorkspace,L"Next workspace",L"Ctrl+Alt+Right",workspaces_.size()>1);
    add(SessionsManager,L"Open saved sessions",L"Named tab sets");
    add(SaveWorkspaceSession,L"Save current workspace as session",L"Named session",page);
    add(PrivacyManager,L"Open Privacy Manager",L"Cookies, site data and permissions");
    add(ToggleVerticalTabs,vertical_tabs_?L"Use horizontal tabs":L"Use vertical tabs",L"Tab layout");
    if(vertical_tabs_)add(ToggleVerticalTabsCollapse,vertical_tabs_collapsed_?L"Expand vertical tab rail":L"Collapse vertical tab rail",L"Tab layout");
    add(split_tab_id_?CloseSplitView:SplitView,split_tab_id_?L"Close split view":L"Open split view",L"Side-by-side browsing",tab && IsWebAddress(tab->url));
    add(ReaderMode,L"Open Reader Mode",L"Native distraction-free reading",tab && IsWebAddress(tab->url));
    add(CapturePage,L"Capture screenshot...",L"Visible area or full page • Ctrl+Shift+S",page);
    add(History,L"Open History",L"Ctrl+H");
    add(Downloads,L"Open Downloads",L"Ctrl+J");
    add(BookmarkManager,L"Open Bookmark manager",L"Ctrl+Shift+O");
    add(ProfileManager,L"Manage profiles",L"Profiles");
    add(ExtensionsPage,L"Extensions",L"Load/manage unpacked Chromium extensions");
    add(SettingsMenu,L"Open Settings",L"Settings");
    add(WelcomePage,L"Getting started",L"Default browser, privacy and shortcuts");
    add(OpenDefaultApps,L"Choose WebCom as default browser",L"Opens Windows Default Apps");
    add(DiagnosticsPage,L"Support diagnostics",L"Runtime and profile information");
    add(RecoveryPage,L"Recovery & reliability",L"Crash evidence, rollback and recovery report");
    add(ExportRecoveryReport,L"Export recovery report ZIP",L"Explicit local report with bounded redacted runtime log");
    add(UpdatesPage,L"Updates",L"Release channel, consent and staged update status");
    add(CheckForUpdates,L"Check for updates",L"User-triggered signed manifest check",update_settings_.checks_enabled&&!update_checking_&&!update_staging_&&UpdateRetryRemainingNow()==0&&ManifestUrlForChannel(update_runtime_config_,update_settings_.channel).has_value()&&IsTrustedPublisherFingerprint(update_runtime_config_.publisher_sha256));
    add(StageAvailableUpdate,L"Stage available update",L"Download and verify installer",available_update_.has_value()&&update_transaction_.phase==UpdateTransactionPhase::None&&!update_staging_);
    add(InstallStagedUpdate,L"Install staged update",L"Re-verify and launch signed installer",update_transaction_.phase==UpdateTransactionPhase::VerifiedPackage);
    add(RollbackPreviousVersion,L"Rollback to previous WebCom",L"Re-verify preserved known-good installer",EvaluateRecoveryReadiness(update_transaction_,recovery_package_,kWebComVersion,update_runtime_config_.publisher_sha256).ready());
    add(ExportDiagnosticsBundle,L"Export support diagnostics ZIP",L"Privacy-safe troubleshooting bundle");
    add(UpdateChannelMenu,L"Release channel",Wide(ReleaseChannelName(update_settings_.channel)).c_str());
    add(BookmarkPage,L"Bookmark current page",L"Ctrl+D",tab && IsWebAddress(tab->url));
    add(SearchInPage,L"Find in page",L"Ctrl+F",page);
    add(PrintPage,L"Print page",L"Ctrl+P",page);
    add(DeveloperTools,L"Developer tools",L"F12",page);
    add(FullScreen,L"Toggle full screen",L"F11");
    add(MemoryReport,L"Performance / memory report",L"Shift+Esc");
    add(ClearData,L"Delete browsing data",L"Ctrl+Shift+Del");
    RECT client{};GetClientRect(window_,&client);
    const int palette_x=std::max(D(8),static_cast<int>(client.right)/2-D(232));
    POINT anchor{palette_x,fullscreen_?D(20):D(86)};ClientToScreen(window_,&anchor);
    SearchPickerOptions options;options.title=L"Quick commands";options.placeholder=L"Type a command";
    options.empty_text=L"No matching commands.";options.noun_singular=L"command";options.noun_plural=L"commands";
    options.footer=L"↑ ↓ navigate   Enter run   Esc close";options.show_tab_state=false;
    const auto selected=ShowTabSearch(window_,anchor,items,dpi_,&palette_,&options);
    if(selected && selected<=std::numeric_limits<UINT>::max() && !closing_)Dispatch(static_cast<UINT>(selected));
}

namespace {
struct WebComCaptureSelectionState {
    POINT start{};
    POINT current{};
    bool dragging = false;
    bool done = false;
    bool cancelled = true;
    RECT result{};
};

LRESULT CALLBACK WebComCaptureSelectionProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<WebComCaptureSelectionState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        state = static_cast<WebComCaptureSelectionState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        state->start = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        state->current = state->start;
        state->dragging = true;
        SetCapture(window);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_MOUSEMOVE:
        if (state->dragging) {
            state->current = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state->dragging) {
            state->current = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            RECT client{}; GetClientRect(window, &client);
            const auto normalized = webcom::NormalizeCaptureSelection(
                state->start.x, state->start.y, state->current.x, state->current.y,
                static_cast<int>(client.right - client.left), static_cast<int>(client.bottom - client.top));
            if (GetCapture() == window) ReleaseCapture();
            state->dragging = false;
            if (normalized) {
                state->result = {normalized->left, normalized->top, normalized->right, normalized->bottom};
                state->cancelled = false;
            }
            state->done = true;
            DestroyWindow(window);
        }
        return 0;
    case WM_RBUTTONDOWN:
    case WM_CLOSE:
        if (GetCapture() == window) ReleaseCapture();
        state->cancelled = true;
        state->done = true;
        DestroyWindow(window);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            if (GetCapture() == window) ReleaseCapture();
            state->cancelled = true;
            state->done = true;
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
        RECT client{}; GetClientRect(window, &client);
        HBRUSH background = CreateSolidBrush(RGB(18,18,18));
        FillRect(dc, &client, background); DeleteObject(background);
        if (state->dragging) {
            RECT selected{std::min(state->start.x,state->current.x),std::min(state->start.y,state->current.y),
                          std::max(state->start.x,state->current.x),std::max(state->start.y,state->current.y)};
            HBRUSH fill = CreateSolidBrush(RGB(72,82,98)); FillRect(dc, &selected, fill); DeleteObject(fill);
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(168,199,250));
            HGDIOBJ old_pen = SelectObject(dc, pen); HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, selected.left, selected.top, selected.right, selected.bottom);
            SelectObject(dc, old_brush); SelectObject(dc, old_pen); DeleteObject(pen);
        }
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(232,234,237));
        RECT hint{12,10,client.right-12,38};
        DrawTextW(dc, L"Drag to select an area • Esc or right-click to cancel", -1, &hint,
                  DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        EndPaint(window, &paint); return 0;
    }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

std::optional<RECT> SelectVisibleCaptureRectangle(HWND owner, HINSTANCE instance, RECT screen_bounds) {
    static ATOM overlay_class = 0;
    if (!overlay_class) {
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = WebComCaptureSelectionProc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        window_class.lpszClassName = L"WebComCaptureSelectionOverlay";
        overlay_class = RegisterClassW(&window_class);
        if (!overlay_class) {
            if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return std::nullopt;
            overlay_class = 1;
        }
    }
    const int width = static_cast<int>(screen_bounds.right - screen_bounds.left);
    const int height = static_cast<int>(screen_bounds.bottom - screen_bounds.top);
    if (width < 4 || height < 4) return std::nullopt;
    WebComCaptureSelectionState state;
    HWND overlay = CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED,
        L"WebComCaptureSelectionOverlay", L"Select capture area", WS_POPUP,
        screen_bounds.left, screen_bounds.top, width, height, owner, nullptr, instance, &state);
    if (!overlay) return std::nullopt;
    SetLayeredWindowAttributes(overlay, 0, 150, LWA_ALPHA);
    const BOOL owner_enabled = IsWindowEnabled(owner);
    if (owner_enabled) EnableWindow(owner, FALSE);
    ShowWindow(overlay, SW_SHOWNORMAL); UpdateWindow(overlay); SetForegroundWindow(overlay); SetFocus(overlay);
    MSG message{};
    while (!state.done) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) PostQuitMessage(static_cast<int>(message.wParam));
            state.cancelled = true; state.done = true; break;
        }
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    if (IsWindow(overlay)) DestroyWindow(overlay);
    if (owner_enabled) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); SetFocus(owner); }
    if (state.cancelled) return std::nullopt;
    return state.result;
}

std::optional<CLSID> PngEncoderClsid() {
    UINT count = 0, bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || !count || !bytes) return std::nullopt;
    std::vector<std::byte> buffer(bytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, bytes, encoders) != Gdiplus::Ok) return std::nullopt;
    for (UINT index = 0; index < count; ++index)
        if (encoders[index].MimeType && _wcsicmp(encoders[index].MimeType, L"image/png") == 0)
            return encoders[index].Clsid;
    return std::nullopt;
}

bool PutBitmapOnClipboard(HWND owner, Gdiplus::Bitmap& bitmap) {
    HBITMAP handle = nullptr;
    if (bitmap.GetLastStatus() != Gdiplus::Ok ||
        bitmap.GetHBITMAP(Gdiplus::Color(255,255,255,255), &handle) != Gdiplus::Ok || !handle) return false;
    if (!OpenClipboard(owner)) { DeleteObject(handle); return false; }
    struct ClipboardCloser { ~ClipboardCloser(){ CloseClipboard(); } } closer;
    if (!EmptyClipboard()) { DeleteObject(handle); return false; }
    if (!SetClipboardData(CF_BITMAP, handle)) { DeleteObject(handle); return false; }
    // Clipboard owns the bitmap handle after SetClipboardData succeeds.
    return true;
}

bool CopyPngStreamToClipboard(HWND owner, IStream* stream) {
    if (!stream) return false;
    LARGE_INTEGER zero{};
    if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) return false;
    Gdiplus::Bitmap bitmap(stream, FALSE);
    return PutBitmapOnClipboard(owner, bitmap);
}

bool CopySelectionPngToClipboard(HWND owner, IStream* stream, RECT selection, SIZE viewport) {
    if (!stream || viewport.cx <= 0 || viewport.cy <= 0) return false;
    LARGE_INTEGER zero{};
    if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) return false;
    Gdiplus::Bitmap source(stream, FALSE);
    if (source.GetLastStatus() != Gdiplus::Ok || !source.GetWidth() || !source.GetHeight()) return false;
    const webcom::CaptureSelectionRect raw{selection.left,selection.top,selection.right,selection.bottom};
    const auto scaled = webcom::ScaleCaptureSelection(raw, viewport.cx, viewport.cy,
        static_cast<int>(source.GetWidth()), static_cast<int>(source.GetHeight()));
    const int width = scaled.right - scaled.left;
    const int height = scaled.bottom - scaled.top;
    if (width <= 0 || height <= 0) return false;
    Gdiplus::Bitmap cropped(width, height, PixelFormat32bppARGB);
    if (cropped.GetLastStatus() != Gdiplus::Ok) return false;
    Gdiplus::Graphics graphics(&cropped);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    if (graphics.DrawImage(&source, Gdiplus::Rect(0,0,width,height), scaled.left,scaled.top,width,height,
                           Gdiplus::UnitPixel) != Gdiplus::Ok) return false;
    return PutBitmapOnClipboard(owner, cropped);
}

bool SaveSelectionPng(IStream* stream, RECT selection, SIZE viewport, const std::wstring& path) {
    if (!stream || viewport.cx <= 0 || viewport.cy <= 0) return false;
    LARGE_INTEGER zero{};
    if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) return false;
    Gdiplus::Bitmap source(stream, FALSE);
    if (source.GetLastStatus() != Gdiplus::Ok || !source.GetWidth() || !source.GetHeight()) return false;
    const webcom::CaptureSelectionRect raw{selection.left,selection.top,selection.right,selection.bottom};
    const auto scaled = webcom::ScaleCaptureSelection(raw, viewport.cx, viewport.cy,
        static_cast<int>(source.GetWidth()), static_cast<int>(source.GetHeight()));
    const int width = scaled.right - scaled.left;
    const int height = scaled.bottom - scaled.top;
    if (width <= 0 || height <= 0) return false;
    Gdiplus::Bitmap cropped(width, height, PixelFormat32bppARGB);
    if (cropped.GetLastStatus() != Gdiplus::Ok) return false;
    Gdiplus::Graphics graphics(&cropped);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    if (graphics.DrawImage(&source, Gdiplus::Rect(0,0,width,height), scaled.left,scaled.top,width,height,
                           Gdiplus::UnitPixel) != Gdiplus::Ok) return false;
    const auto encoder = PngEncoderClsid();
    return encoder && cropped.Save(path.c_str(), &*encoder, nullptr) == Gdiplus::Ok;
}

ComPtr<IStream> CreateSelectionPngStream(IStream* stream, RECT selection, SIZE viewport) {
    if (!stream || viewport.cx <= 0 || viewport.cy <= 0) return {};
    LARGE_INTEGER zero{}; if (FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) return {};
    Gdiplus::Bitmap source(stream, FALSE);
    if (source.GetLastStatus() != Gdiplus::Ok || !source.GetWidth() || !source.GetHeight()) return {};
    const webcom::CaptureSelectionRect raw{selection.left,selection.top,selection.right,selection.bottom};
    const auto scaled = webcom::ScaleCaptureSelection(raw, viewport.cx, viewport.cy,
        static_cast<int>(source.GetWidth()), static_cast<int>(source.GetHeight()));
    const int width = scaled.right - scaled.left, height = scaled.bottom - scaled.top;
    if (width <= 0 || height <= 0) return {};
    Gdiplus::Bitmap cropped(width,height,PixelFormat32bppARGB); if(cropped.GetLastStatus()!=Gdiplus::Ok) return {};
    Gdiplus::Graphics graphics(&cropped); graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    if(graphics.DrawImage(&source,Gdiplus::Rect(0,0,width,height),scaled.left,scaled.top,width,height,Gdiplus::UnitPixel)!=Gdiplus::Ok)return {};
    const auto encoder=PngEncoderClsid(); if(!encoder)return {};
    ComPtr<IStream> output; if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&output))||!output)return {};
    if(cropped.Save(output.Get(),&*encoder,nullptr)!=Gdiplus::Ok)return {};
    output->Seek(zero,STREAM_SEEK_SET,nullptr); return output;
}
} // namespace

void BrowserApp::Impl::CaptureMenuPopup() {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    auto* tab = Active();
    const bool page = tab && tab->web && IsWebAddress(tab->url);
    std::vector<MenuItem> items{
        {CaptureViewport,L"Save visible area...",L"Current viewport • PNG",L"\uE91B",page},
        {CaptureSelection,L"Save selected area...",L"Drag over the visible page • PNG",L"\uE91B",page},
        {CaptureFullPage,L"Save full page...",L"Entire scrollable page • PNG",L"\uE91B",page},
        {0,L"",L"",L"",false,false,true},
        {AnnotateViewport,L"Annotate visible area...",L"Pen, rectangle, undo, copy or save",L"\uE70F",page},
        {AnnotateSelection,L"Annotate selected area...",L"Select an area, then mark it up",L"\uE70F",page},
        {0,L"",L"",L"",false,false,true},
        {CaptureViewportClipboard,L"Copy visible area",L"Copy image to Windows clipboard",L"\uE8C8",page},
        {CaptureSelectionClipboard,L"Copy selected area",L"Drag area, then copy image",L"\uE8C8",page}
    };
    RECT client{}; GetClientRect(window_, &client);
    POINT anchor{std::max(D(8), static_cast<int>(client.right)/2-D(190)), fullscreen_?D(20):D(86)};
    ClientToScreen(window_, &anchor);
    UINT selected=0;
    {
        menu_open_=true;
        struct Reset{bool& value;~Reset(){value=false;}} reset{menu_open_};
        selected=ShowDarkMenu(window_,anchor,items,dpi_,380,false,&palette_);
    }
    if(selected && !closing_) Dispatch(selected);
}

void BrowserApp::Impl::AnnotateVisibleCapture() {
    auto* tab=Active(); if(!tab||!tab->web||!IsWebAddress(tab->url)){Note(L"Open a website first.");return;}
    ComPtr<IStream> stream; if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))||!stream){Notice(L"WebCom could not allocate the annotation buffer.");return;}
    const auto weak=weak_from_this();
    const HRESULT capture=tab->web->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
        MakeCallback<ICoreWebView2CapturePreviewCompletedHandler>([weak,stream](HRESULT error)->HRESULT{
            if(auto app=weak.lock();app&&!app->closing_){
                if(FAILED(error)||!ShowScreenshotAnnotationEditor(app->window_,app->instance_,stream.Get(),app->dpi_))
                    app->Notice(L"The screenshot annotation editor could not be opened.");
            }
            return S_OK;
        }).Get());
    if(FAILED(capture))Notice(L"The visible-area screenshot could not start. "+HResultText(capture));
}

void BrowserApp::Impl::AnnotateSelectionCapture() {
    auto* tab=Active(); if(!tab||!tab->web||!tab->controller||!IsWebAddress(tab->url)){Note(L"Open a website first.");return;}
    const RECT controller=ControllerBoundsFor(*tab);const int viewport_width=static_cast<int>(controller.right-controller.left),viewport_height=static_cast<int>(controller.bottom-controller.top);
    if(viewport_width<4||viewport_height<4){Note(L"The visible page is too small to capture.");return;}
    POINT top_left{controller.left,controller.top},bottom_right{controller.right,controller.bottom};ClientToScreen(window_,&top_left);ClientToScreen(window_,&bottom_right);
    const auto selection=SelectVisibleCaptureRectangle(window_,instance_,{top_left.x,top_left.y,bottom_right.x,bottom_right.y});if(!selection||closing_)return;
    ComPtr<IStream> stream;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream))||!stream){Notice(L"WebCom could not allocate the annotation buffer.");return;}
    const SIZE viewport{viewport_width,viewport_height};const auto weak=weak_from_this();
    const HRESULT capture=tab->web->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
        MakeCallback<ICoreWebView2CapturePreviewCompletedHandler>([weak,stream,selection=*selection,viewport](HRESULT error)->HRESULT{
            if(auto app=weak.lock();app&&!app->closing_){
                if(FAILED(error)){app->Notice(L"The selected-area screenshot could not be captured.");return S_OK;}
                const auto cropped=CreateSelectionPngStream(stream.Get(),selection,viewport);
                if(!cropped||!ShowScreenshotAnnotationEditor(app->window_,app->instance_,cropped.Get(),app->dpi_))
                    app->Notice(L"The screenshot annotation editor could not be opened.");
            }
            return S_OK;
        }).Get());
    if(FAILED(capture))Notice(L"The selection screenshot could not start. "+HResultText(capture));
}

void BrowserApp::Impl::CopyVisibleCaptureToClipboard() {
    auto* tab=Active();
    if(!tab || !tab->web || !IsWebAddress(tab->url)){Note(L"Open a website first.");return;}
    ComPtr<IStream> stream;
    if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)) || !stream){
        Notice(L"WebCom could not allocate the screenshot clipboard buffer.");return;
    }
    const auto weak=weak_from_this();
    const HRESULT capture=tab->web->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
        MakeCallback<ICoreWebView2CapturePreviewCompletedHandler>([weak,stream](HRESULT error)->HRESULT{
            if(auto app=weak.lock();app && !app->closing_){
                if(SUCCEEDED(error) && CopyPngStreamToClipboard(app->window_,stream.Get()))
                    app->Note(L"Visible-area screenshot copied to the clipboard.");
                else app->Notice(L"The visible-area screenshot could not be copied to the clipboard.");
            }
            return S_OK;
        }).Get());
    if(FAILED(capture))Notice(L"The visible-area screenshot could not start. "+HResultText(capture));
}

void BrowserApp::Impl::CopySelectionCaptureToClipboard() {
    auto* tab = Active();
    if (!tab || !tab->web || !tab->controller || !IsWebAddress(tab->url)) { Note(L"Open a website first."); return; }
    const RECT controller = ControllerBoundsFor(*tab);
    const int viewport_width = static_cast<int>(controller.right - controller.left);
    const int viewport_height = static_cast<int>(controller.bottom - controller.top);
    if (viewport_width < 4 || viewport_height < 4) { Note(L"The visible page is too small to capture."); return; }
    POINT top_left{controller.left,controller.top}; POINT bottom_right{controller.right,controller.bottom};
    ClientToScreen(window_,&top_left); ClientToScreen(window_,&bottom_right);
    const auto selection = SelectVisibleCaptureRectangle(window_,instance_,
        {top_left.x,top_left.y,bottom_right.x,bottom_right.y});
    if (!selection || closing_) return;
    ComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)) || !stream) {
        Notice(L"WebCom could not allocate the selection clipboard buffer."); return;
    }
    const SIZE viewport{viewport_width,viewport_height};
    const auto weak=weak_from_this();
    const HRESULT capture=tab->web->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
        MakeCallback<ICoreWebView2CapturePreviewCompletedHandler>([weak,stream,selection=*selection,viewport](HRESULT error)->HRESULT{
            if(auto app=weak.lock();app && !app->closing_){
                if(SUCCEEDED(error) && CopySelectionPngToClipboard(app->window_,stream.Get(),selection,viewport))
                    app->Note(L"Selection screenshot copied to the clipboard.");
                else app->Notice(L"The selection screenshot could not be copied to the clipboard.");
            }
            return S_OK;
        }).Get());
    if(FAILED(capture))Notice(L"The selection screenshot could not start. "+HResultText(capture));
}

void BrowserApp::Impl::SaveSelectionCapture() {
    auto* tab = Active();
    if (!tab || !tab->web || !tab->controller || !IsWebAddress(tab->url)) { Note(L"Open a website first."); return; }
    const RECT controller = ControllerBoundsFor(*tab);
    const int viewport_width = static_cast<int>(controller.right - controller.left);
    const int viewport_height = static_cast<int>(controller.bottom - controller.top);
    if (viewport_width < 4 || viewport_height < 4) { Note(L"The visible page is too small to capture."); return; }
    POINT top_left{controller.left,controller.top}; POINT bottom_right{controller.right,controller.bottom};
    ClientToScreen(window_,&top_left); ClientToScreen(window_,&bottom_right);
    const auto selection = SelectVisibleCaptureRectangle(window_,instance_,
        {top_left.x,top_left.y,bottom_right.x,bottom_right.y});
    if (!selection || closing_) return;

    wchar_t file[MAX_PATH]=L"WebCom-selection.png";
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=file; dialog.nMaxFile=MAX_PATH; dialog.lpstrDefExt=L"png";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog)) return;
    const std::wstring path=file;
    ComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)) || !stream) {
        Notice(L"WebCom could not allocate the selection screenshot buffer."); return;
    }
    const SIZE viewport{viewport_width,viewport_height};
    const auto weak=weak_from_this();
    const HRESULT capture=tab->web->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
        MakeCallback<ICoreWebView2CapturePreviewCompletedHandler>([weak,stream,path,selection=*selection,viewport](HRESULT error)->HRESULT{
            if(auto app=weak.lock();app && !app->closing_){
                if(SUCCEEDED(error) && SaveSelectionPng(stream.Get(),selection,viewport,path))
                    app->Note(L"Selection screenshot saved: "+std::filesystem::path(path).filename().wstring());
                else { DeleteFileW(path.c_str()); app->Notice(L"The selection screenshot could not be saved."); }
            }
            return S_OK;
        }).Get());
    if(FAILED(capture)){DeleteFileW(path.c_str());Notice(L"The selection screenshot could not start. "+HResultText(capture));}
}

void BrowserApp::Impl::SavePageCapture(bool full_page) {
    auto* tab=Active();if(!tab || !tab->web || !IsWebAddress(tab->url)){Note(L"Open a website first.");return;}
    wchar_t file[MAX_PATH]{};
    wcscpy_s(file, full_page ? L"WebCom-full-page.png" : L"WebCom-visible-area.png");
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"png";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    const std::wstring path=file;
    const auto weak=weak_from_this();
    if(full_page){
        const wchar_t* parameters=L"{\"format\":\"png\",\"captureBeyondViewport\":true,\"fromSurface\":true}";
        const HRESULT capture=tab->web->CallDevToolsProtocolMethod(L"Page.captureScreenshot",parameters,
            MakeCallback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
                [weak,path](HRESULT error,LPCWSTR result)->HRESULT{
                    auto app=weak.lock();if(!app || app->closing_)return S_OK;
                    if(FAILED(error) || !result){DeleteFileW(path.c_str());app->Notice(L"The full-page screenshot failed. "+HResultText(error));return S_OK;}
                    const auto bytes=DecodeCaptureScreenshotResult(result);
                    if(!bytes){DeleteFileW(path.c_str());app->Notice(L"WebView2 returned an invalid or oversized full-page screenshot.");return S_OK;}
                    std::ofstream output(std::filesystem::path(path),std::ios::binary|std::ios::trunc);
                    if(!output || !output.write(reinterpret_cast<const char*>(bytes->data()),static_cast<std::streamsize>(bytes->size()))){
                        output.close();DeleteFileW(path.c_str());app->Notice(L"The full-page screenshot could not be written to disk.");return S_OK;
                    }
                    output.close();
                    app->Note(L"Full-page screenshot saved: "+std::filesystem::path(path).filename().wstring());
                    return S_OK;
                }).Get());
        if(FAILED(capture)){DeleteFileW(path.c_str());Notice(L"Full-page capture is unavailable in this WebView2 Runtime. "+HResultText(capture));}
        return;
    }
    ComPtr<IStream> stream;
    const HRESULT create=SHCreateStreamOnFileEx(path.c_str(),STGM_CREATE|STGM_WRITE|STGM_SHARE_EXCLUSIVE,FILE_ATTRIBUTE_NORMAL,TRUE,nullptr,&stream);
    if(FAILED(create) || !stream){Notice(L"WebCom could not create the screenshot file.");return;}
    const HRESULT capture=tab->web->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),
        MakeCallback<ICoreWebView2CapturePreviewCompletedHandler>([weak,stream,path](HRESULT error)->HRESULT{
            if(auto app=weak.lock();app && !app->closing_){
                if(SUCCEEDED(error)){stream->Commit(STGC_DEFAULT);app->Note(L"Visible-area screenshot saved: "+std::filesystem::path(path).filename().wstring());}
                else {DeleteFileW(path.c_str());app->Notice(L"The visible-area screenshot could not be saved. "+HResultText(error));}
            }
            return S_OK;
        }).Get());
    if(FAILED(capture)){DeleteFileW(path.c_str());Notice(L"The visible-area screenshot could not start. "+HResultText(capture));}
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
        if(_wcsicmp(tab->workspace.c_str(),active_workspace_.c_str())!=0)detail=tab->workspace+L" workspace  •  "+detail;
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
