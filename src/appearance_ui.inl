// Per-profile appearance belongs to the window, never to mutable global colors.
void BrowserApp::Impl::ApplyAppearance() {
    palette_=MakePalette(appearance_,profile_name_);
    kCaption=NativeColor(palette_.caption);kFrame=NativeColor(palette_.frame);
    kSurface=NativeColor(palette_.surface);kHover=NativeColor(palette_.hover);
    kInk=NativeColor(palette_.ink);kMuted=NativeColor(palette_.muted);
    kAccent=NativeColor(palette_.accent);kSearch=NativeColor(palette_.search);kBorder=NativeColor(palette_.border);
    const auto profile_rgb=ProfileColor(appearance_,profile_name_);
    profile_color_=NativeColor(profile_rgb);profile_ink_=NativeColor(ContrastInk(profile_rgb));
    show_bookmarks_=appearance_.show_bookmarks;
    if(!window_ || closing_)return;
    const auto brush=[&](HBRUSH& current,COLORREF color){
        if(HBRUSH next=CreateSolidBrush(color)){if(current)DeleteObject(current);current=next;}
    };
    brush(edit_brush_,kFrame);brush(idle_edit_brush_,kSurface);
    brush(search_brush_,kSearch);brush(toast_brush_,kSurface);
    native_chrome_.frame.Reset();native_chrome_.tabs.Reset();native_chrome_.button.Reset();
    native_chrome_.native_page=-1;
    for(auto& tab:tabs_)ConfigureTheme(*tab);
    UpdateUI();
    RedrawWindow(window_,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_NOERASE);
}

bool BrowserApp::Impl::CommitAppearance(const Appearance& next) {
    std::string error;
    if(!SaveAppearance(root_,profile_name_,next,&error)){
        Note(L"The appearance change could not be saved. "+Wide(error));return false;
    }
    appearance_=next;ApplyAppearance();return true;
}

void BrowserApp::Impl::AppearanceOptions(UINT section) {
    if(menu_open_ || closing_ || !window_ || !IsWindow(window_))return;
    const auto keep_alive=shared_from_this();
    const HWND owner=window_;
    const MenuItem divider{0,L"",L"",L"",false,false,true};
    std::vector<MenuItem> items;
    if(section==ThemeMenu){
        for(unsigned i=0;i<4;++i){
            auto sample=appearance_;sample.theme=static_cast<BrowserTheme>(i);
            MenuItem item{ThemeBase+i,Wide(ThemeName(sample.theme)),L"",L"",true,appearance_.theme==sample.theme};
            item.swatch_rgb=MakePalette(sample,profile_name_).frame;items.push_back(std::move(item));
        }
        items.push_back(divider);
        items.push_back({AppearanceMenu,L"Appearance",L"\u203A",L"\uE790"});
    } else if(section==ProfileColorMenu){
        items.push_back({ColorAutomatic,L"Automatic",L"",L"",true,!appearance_.accent_rgb});
        auto automatic=appearance_;automatic.accent_rgb.reset();
        items.back().swatch_rgb=ProfileColor(automatic,profile_name_);
        constexpr const wchar_t* names[]={L"Blue",L"Green",L"Yellow",L"Rose",L"Lavender",L"Cyan",L"Peach",L"Silver"};
        for(unsigned i=0;i<kProfileColors.size();++i){
            MenuItem item{ColorBase+i,names[i],L"",L"",true,appearance_.accent_rgb==kProfileColors[i]};
            item.swatch_rgb=kProfileColors[i];items.push_back(std::move(item));
        }
        items.push_back(divider);
        items.push_back({ColorCustom,L"Custom color...",L"",L"\uE790"});
    } else {
        items={
            {0,L"Appearance for "+Wide(profile_name_),L"",L"",false},
            divider,
            {ThemeMenu,L"Dark theme",Wide(ThemeName(appearance_.theme))+L"  \u203A",L"\uE771"},
            {ProfileColorMenu,L"Profile color",L"\u203A",L"\uE790"},
            {ToggleBookmarks,L"Show bookmarks bar",L"Ctrl+Shift+B",L"\uE734",true,show_bookmarks_},
            divider,
            {AppearanceReset,L"Reset appearance",L"",L"\uE777"}
        };
        items[3].swatch_rgb=ProfileColor(appearance_,profile_name_);
    }
    RECT client{};GetClientRect(owner,&client);
    POINT anchor{client.right-D(8),fullscreen_?D(8):D(82)};ClientToScreen(owner,&anchor);
    UINT selected=0;
    {
        menu_open_=true;
        struct Reset{bool& value;~Reset(){value=false;}}reset{menu_open_};
        selected=ShowDarkMenu(owner,anchor,items,dpi_,354,true,&palette_);
    }
    if(!closing_ && window_==owner && IsWindow(owner) && selected)Dispatch(selected);
}
