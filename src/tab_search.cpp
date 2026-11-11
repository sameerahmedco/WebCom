#include "tab_search.h"
#include "appearance.h"
#include "win32_com_compat.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <string_view>

namespace webcom {
namespace {

constexpr wchar_t WindowClass[] = L"WebCom.NativeTabSearch.v1";
constexpr wchar_t OwnerProperty[] = L"WebCom.ActiveTabSearch";
constexpr UINT SearchEdit = 301, ResultsList = 302, ClearSearch = 303;

COLORREF NativeColor(std::uint32_t rgb) {
    return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

COLORREF Blend(COLORREF foreground, COLORREF background, unsigned amount) {
    const auto channel = [amount](unsigned a, unsigned b) { return (a * amount + b * (100 - amount)) / 100; };
    return RGB(channel(GetRValue(foreground), GetRValue(background)),
               channel(GetGValue(foreground), GetGValue(background)),
               channel(GetBValue(foreground), GetBValue(background)));
}

void Fill(HDC dc, const RECT& r, COLORREF color) {
    HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&r,brush); DeleteObject(brush);
}

void Round(HDC dc, const RECT& r, COLORREF fill, COLORREF edge, int diameter) {
    using namespace Gdiplus;
    Graphics graphics(dc);graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    const REAL x=r.left+0.5f,y=r.top+0.5f;
    const REAL w=std::max(1.0f,static_cast<REAL>(r.right-r.left-1));
    const REAL h=std::max(1.0f,static_cast<REAL>(r.bottom-r.top-1));
    const REAL d=std::max(1.0f,std::min({static_cast<REAL>(diameter),w,h}));
    GraphicsPath path;
    path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);
    path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();
    SolidBrush brush(Color(255,GetRValue(fill),GetGValue(fill),GetBValue(fill)));
    graphics.FillPath(&brush,&path);
    if(fill!=edge){Pen pen(Color(255,GetRValue(edge),GetGValue(edge),GetBValue(edge)),1.0f);graphics.DrawPath(&pen,&path);}
}

void Text(HDC dc,std::wstring_view value,RECT r,HFONT font,COLORREF color,UINT align=DT_LEFT) {
    const auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
    DrawTextW(dc,value.data(),static_cast<int>(value.size()),&r,
              align|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    SelectObject(dc,old);
}

std::wstring Clean(std::wstring_view value,std::size_t maximum) {
    std::wstring text(value.substr(0,maximum));
    for(wchar_t& c:text)if(c<0x20 || c==0x7f)c=L' ';
    if(!text.empty() && text.back()>=0xd800 && text.back()<=0xdbff)text.pop_back();
    return text;
}

struct Buffer {
    HDC dc=nullptr;
    HBITMAP bitmap=nullptr;
    HGDIOBJ previous=nullptr;
    int width=0,height=0;
    ~Buffer(){Reset();}
    void Reset(){
        if(dc && previous)SelectObject(dc,previous);
        if(bitmap)DeleteObject(bitmap);
        if(dc)DeleteDC(dc);
        dc=nullptr;bitmap=nullptr;previous=nullptr;width=height=0;
    }
    bool Ensure(HDC reference,int w,int h){
        if(dc && width==w && height==h)return true;
        if(w<=0 || h<=0)return false;
        HDC next_dc=CreateCompatibleDC(reference);if(!next_dc)return false;
        HBITMAP next_bitmap=CreateCompatibleBitmap(reference,w,h);
        if(!next_bitmap){DeleteDC(next_dc);return false;}
        HGDIOBJ next_previous=SelectObject(next_dc,next_bitmap);
        if(!next_previous || next_previous==HGDI_ERROR){DeleteObject(next_bitmap);DeleteDC(next_dc);return false;}
        Reset();dc=next_dc;bitmap=next_bitmap;previous=next_previous;width=w;height=h;return true;
    }
};

struct SearchState {
    HWND owner=nullptr,window=nullptr,edit=nullptr,list=nullptr,clear=nullptr,empty=nullptr,label=nullptr;
    HWND previous_focus=nullptr;
    HFONT font=nullptr,bold_font=nullptr,small_font=nullptr,title_font=nullptr,icon_font=nullptr;
    HBRUSH background=nullptr,field=nullptr;
    UINT dpi=96;
    int width=0,height=0,list_top=0,list_height=0,pressed=-1,hot_row=-1;
    bool done=false,clear_hover=false;
    COLORREF Background=0,Field=0,Foreground=0,Muted=0,Selected=0,Accent=0,Border=0,Hover=0;
    std::uint64_t result=0;
    std::vector<TabSearchItem> items;
    std::vector<std::size_t> visible;
    SearchPickerOptions options{};
    Buffer chrome_buffer,row_buffer;

    int D(int value)const{return MulDiv(value,static_cast<int>(dpi),96);}
    int RowHeight()const{return D(56);}
    void ApplyPalette(const Palette* requested){
        const auto palette=requested?*requested:MakePalette(Appearance{},"Default");
        Background=NativeColor(palette.frame);Field=NativeColor(palette.surface);
        Foreground=NativeColor(palette.ink);Muted=NativeColor(palette.muted);
        Selected=NativeColor(palette.hover);Accent=NativeColor(palette.accent);
        Border=NativeColor(palette.border);Hover=Blend(Selected,Background,60);
    }
    void HoverRow(int index){
        if(hot_row==index)return;
        for(const int row:{hot_row,index})if(row>=0){
            RECT bounds{};
            if(SendMessageW(list,LB_GETITEMRECT,static_cast<WPARAM>(row),reinterpret_cast<LPARAM>(&bounds))!=LB_ERR)
                InvalidateRect(list,&bounds,FALSE);
        }
        hot_row=index;
    }
    void End(std::uint64_t selected=0){
        if(done)return;
        done=true;result=selected;
        if(window && IsWindow(window))DestroyWindow(window);
    }
    std::wstring Query()const{
        const int size=std::min(512,GetWindowTextLengthW(edit));
        std::wstring text(static_cast<std::size_t>(size)+1,L'\0');
        GetWindowTextW(edit,text.data(),size+1);text.resize(size);return text;
    }
    std::wstring StateLabel(const TabSearchItem& item)const{
        std::wstring text;
        auto add=[&](const wchar_t* label){if(!text.empty())text+=L" · ";text+=label;};
        if(!options.show_tab_state)return text;
        if(item.private_mode)add(L"Incognito");
        if(item.muted)add(L"Muted");else if(item.audible)add(L"Playing");
        if(item.sleeping)add(L"Sleeping");
        if(item.active)add(L"Current");
        return text;
    }
    void Activate(int index){
        if(index<0 || static_cast<std::size_t>(index)>=visible.size())return;
        End(items[visible[static_cast<std::size_t>(index)]].id);
    }
    void Select(int index){
        if(visible.empty())return;
        index=std::clamp(index,0,static_cast<int>(visible.size())-1);
        SendMessageW(list,LB_SETCURSEL,static_cast<WPARAM>(index),0);
        RECT r{};
        if(SendMessageW(list,LB_GETITEMRECT,static_cast<WPARAM>(index),reinterpret_cast<LPARAM>(&r))!=LB_ERR){
            RECT bounds{};GetClientRect(list,&bounds);
            if(r.top<0)SendMessageW(list,LB_SETTOPINDEX,static_cast<WPARAM>(index),0);
            else if(r.bottom>bounds.bottom){
                const int rows=std::max(1,static_cast<int>(bounds.bottom)/RowHeight());
                SendMessageW(list,LB_SETTOPINDEX,static_cast<WPARAM>(std::max(0,index-rows+1)),0);
            }
        }
        NotifyWinEvent(EVENT_OBJECT_SELECTION,list,OBJID_CLIENT,index+1);
    }
    int Selection()const{return static_cast<int>(SendMessageW(list,LB_GETCURSEL,0,0));}
    void Filter(){
        if(done || !edit || !list)return;
        std::uint64_t old_id=0;
        const int old=Selection();
        if(old>=0 && static_cast<std::size_t>(old)<visible.size())old_id=items[visible[static_cast<std::size_t>(old)]].id;
        const auto query=Query();
        auto matches=FilterTabSearchItems(items,query);
        SendMessageW(list,WM_SETREDRAW,FALSE,0);
        SendMessageW(list,LB_RESETCONTENT,0,0);
        visible=std::move(matches);
        int selected=0;
        for(std::size_t i=0;i<visible.size();++i){
            const auto& item=items[visible[i]];
            std::wstring row_text=item.title+L", "+Clean(item.url,512);
            const auto state=StateLabel(item);if(!state.empty())row_text+=L", "+state;
            const LRESULT added=SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(row_text.c_str()));
            if(added==LB_ERR || added==LB_ERRSPACE){
                // No result may refer to a different source item after an allocation failure.
                SendMessageW(list,WM_SETREDRAW,TRUE,0);End();return;
            }
            if(item.id==old_id || (!old_id && item.active))selected=static_cast<int>(i);
        }
        SendMessageW(list,WM_SETREDRAW,TRUE,0);
        SetWindowTextW(empty,items.empty()?(options.show_tab_state?L"No open tabs in this window.":options.empty_text):options.empty_text);
        ShowWindow(list,visible.empty()?SW_HIDE:SW_SHOWNOACTIVATE);
        ShowWindow(empty,visible.empty()?SW_SHOWNOACTIVATE:SW_HIDE);
        ShowWindow(clear,query.empty()?SW_HIDE:SW_SHOWNOACTIVATE);
        if(!visible.empty())Select(selected);
        pressed=-1;hot_row=-1;
        RedrawWindow(list,nullptr,nullptr,RDW_INVALIDATE|RDW_NOERASE);
        InvalidateRect(window,nullptr,FALSE);
    }
    bool Key(UINT key){
        if(key==VK_ESCAPE){End();return true;}
        if(key==VK_RETURN){
            if(GetFocus()==clear){SetWindowTextW(edit,L"");SetFocus(edit);}
            else Activate(Selection());
            return true;
        }
        if(key=='A' && (GetKeyState(VK_CONTROL)&0x8000)){
            SetFocus(edit);SendMessageW(edit,EM_SETSEL,0,-1);return true;
        }
        if(key==VK_BACK && GetFocus()==list){SetFocus(edit);SendMessageW(edit,WM_CHAR,VK_BACK,0);return true;}
        if(key==VK_UP || key==VK_DOWN || key==VK_PRIOR || key==VK_NEXT){
            const int count=static_cast<int>(visible.size());if(!count)return true;
            int index=Selection();if(index<0)index=0;
            const int page=std::max(1,list_height/RowHeight());
            if(key==VK_UP)index=(index+count-1)%count;
            if(key==VK_DOWN)index=(index+1)%count;
            if(key==VK_PRIOR)index-=page;
            if(key==VK_NEXT)index+=page;
            Select(index);return true;
        }
        if(GetFocus()==list && (key==VK_HOME || key==VK_END)){
            Select(key==VK_HOME?0:static_cast<int>(visible.size())-1);return true;
        }
        if(key==VK_TAB){
            std::array<HWND,3> controls{edit,clear,list};
            const int direction=(GetKeyState(VK_SHIFT)&0x8000)?-1:1;
            int current=0;
            for(int i=0;i<3;++i)if(controls[i]==GetFocus())current=i;
            for(int i=0;i<3;++i){current=(current+direction+3)%3;if(IsWindowVisible(controls[current])){SetFocus(controls[current]);break;}}
            return true;
        }
        return false;
    }
    void DrawRow(const DRAWITEMSTRUCT& draw){
        if(draw.itemID==static_cast<UINT>(-1) || draw.itemID>=visible.size()){Fill(draw.hDC,draw.rcItem,Background);return;}
        const int w=static_cast<int>(draw.rcItem.right-draw.rcItem.left),h=static_cast<int>(draw.rcItem.bottom-draw.rcItem.top);
        const bool buffered=row_buffer.Ensure(draw.hDC,w,h);
        HDC dc=buffered?row_buffer.dc:draw.hDC;
        RECT r=buffered?RECT{0,0,w,h}:draw.rcItem;
        Fill(dc,r,Background);
        const auto& item=items[visible[draw.itemID]];
        const bool selected=(draw.itemState&ODS_SELECTED)!=0;
        const bool hovered=static_cast<int>(draw.itemID)==hot_row;
        if(selected || hovered){
            RECT fill=r;InflateRect(&fill,-D(4),-D(2));
            Round(dc,fill,selected?Selected:Hover,selected?Selected:Hover,D(12));
        }
        if(item.active){
            RECT mark{r.left+D(6),r.top+D(17),r.left+D(9),r.bottom-D(17)};
            Round(dc,mark,Accent,Accent,D(3));
        }
        const auto state_label=StateLabel(item);
        const int left=r.left+D(48);
        const COLORREF title_color=item.sleeping?Muted:Foreground;
        Text(dc,item.title,{left,r.top+D(6),r.right-D(12),r.top+D(29)},item.active?bold_font:font,title_color);
        SIZE extent{};const auto old_font=SelectObject(dc,small_font);
        GetTextExtentPoint32W(dc,state_label.c_str(),static_cast<int>(state_label.size()),&extent);
        SelectObject(dc,old_font);
        const int content_width=static_cast<int>(r.right)-D(12)-left;
        const int tag_width=state_label.empty()?0:std::min({D(168),std::max(0,content_width-D(96)),static_cast<int>(extent.cx)});
        if(tag_width)Text(dc,state_label,{r.right-D(12)-tag_width,r.top+D(30),r.right-D(12),r.bottom-D(5)},
            small_font,selected?Accent:Muted,DT_RIGHT);
        std::wstring_view address=item.url;
        if(address.starts_with(L"https://"))address.remove_prefix(8);
        else if(address.starts_with(L"http://"))address.remove_prefix(7);
        if(address==L"webcom:newtab")address=L"New tab";
        Text(dc,address.substr(0,512),{left,r.top+D(30),r.right-D(12)-(tag_width?tag_width+D(12):0),r.bottom-D(5)},small_font,Muted);
        if(item.private_mode){
            const int x=r.left+D(27),y=r.top+RowHeight()/2;
            HPEN pen=CreatePen(PS_SOLID,std::max(1,D(1)),Muted);const auto old_pen=SelectObject(dc,pen);
            const auto old_brush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
            Ellipse(dc,x-D(8),y+D(1),x-D(1),y+D(8));Ellipse(dc,x+D(1),y+D(1),x+D(8),y+D(8));
            MoveToEx(dc,x-D(1),y+D(4),nullptr);LineTo(dc,x+D(1),y+D(4));
            MoveToEx(dc,x-D(10),y-D(2),nullptr);LineTo(dc,x+D(10),y-D(2));
            POINT hat[4]={{x-D(6),y-D(4)},{x-D(3),y-D(10)},{x+D(3),y-D(10)},{x+D(6),y-D(4)}};
            Polygon(dc,hat,4);SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(pen);
        }else {
            RECT icon{r.left+D(13),r.top+D(14),r.left+D(41),r.top+D(42)};
            Round(dc,icon,Field,Field,D(28));
            std::wstring_view host=address;
            if(host.starts_with(L"www."))host.remove_prefix(4);
            const wchar_t initial=host.empty()?L' ':static_cast<wchar_t>(std::towupper(host.front()));
            if((initial>=L'A' && initial<=L'Z') || (initial>=L'0' && initial<=L'9'))
                Text(dc,std::wstring_view(&initial,1),icon,bold_font,item.active?Accent:Muted,DT_CENTER);
            else Text(dc,L"\uE774",icon,icon_font,item.active?Accent:Muted,DT_CENTER);
        }
        if(buffered)BitBlt(draw.hDC,draw.rcItem.left,draw.rcItem.top,w,h,dc,0,0,SRCCOPY);
    }
    void Paint(){
        PAINTSTRUCT paint{};HDC output=BeginPaint(window,&paint);
        const bool buffered=chrome_buffer.Ensure(output,width,height);
        HDC dc=buffered?chrome_buffer.dc:output;
        RECT r{0,0,width,height};Fill(dc,r,Background);Round(dc,r,Background,Border,D(24));
        const std::wstring count=visible.size()==items.size()?std::to_wstring(items.size())+L" "+(items.size()==1?options.noun_singular:options.noun_plural):
            std::to_wstring(visible.size())+L" of "+std::to_wstring(items.size())+L" "+options.noun_plural;
        Text(dc,count,{width-D(105),D(13),width-D(16),D(35)},small_font,Muted,DT_RIGHT);
        const RECT field_rect{D(12),D(43),width-D(12),D(83)};
        Round(dc,field_rect,Field,GetFocus()==edit?Accent:Border,D(16));
        Text(dc,L"\uE721",{D(22),D(52),D(40),D(74)},icon_font,Muted,DT_CENTER);
        Fill(dc,{D(12),D(94),width-D(12),D(94)+1},Border);
        Fill(dc,{D(12),height-D(38),width-D(12),height-D(38)+1},Border);
        Text(dc,options.footer,{D(16),height-D(31),width-D(16),height-D(7)},small_font,Muted);
        if(buffered)BitBlt(output,0,0,width,height,dc,0,0,SRCCOPY);
        EndPaint(window,&paint);
    }
};

LRESULT CALLBACK EditProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR,DWORD_PTR data){
    auto* state=reinterpret_cast<SearchState*>(data);
    if(!state)return DefSubclassProc(window,message,wparam,lparam);
    if(message==WM_PAINT){
        const LRESULT result=DefSubclassProc(window,message,wparam,lparam);
        if(!state->done && IsWindow(window) && !GetWindowTextLengthW(window)){
            HDC dc=GetDC(window);RECT r{};GetClientRect(window,&r);
            Text(dc,state->options.placeholder,r,state->font,state->Muted);ReleaseDC(window,dc);
        }
        return result;
    }
    if(message==WM_MOUSEWHEEL)return SendMessageW(state->list,message,wparam,lparam);
    if((message==WM_SETFOCUS || message==WM_KILLFOCUS) && state->window)InvalidateRect(state->window,nullptr,FALSE);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,EditProc,1);
    return DefSubclassProc(window,message,wparam,lparam);
}

LRESULT CALLBACK ClearProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR,DWORD_PTR data){
    auto* state=reinterpret_cast<SearchState*>(data);
    if(!state)return DefSubclassProc(window,message,wparam,lparam);
    if(message==WM_MOUSEMOVE){
        if(!state->clear_hover){state->clear_hover=true;InvalidateRect(window,nullptr,FALSE);}
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);
    }
    if(message==WM_MOUSELEAVE){state->clear_hover=false;InvalidateRect(window,nullptr,FALSE);}
    if(message==WM_SETFOCUS || message==WM_KILLFOCUS)InvalidateRect(window,nullptr,FALSE);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,ClearProc,1);
    return DefSubclassProc(window,message,wparam,lparam);
}

LRESULT CALLBACK ListProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR,DWORD_PTR data){
    auto* state=reinterpret_cast<SearchState*>(data);
    if(!state)return DefSubclassProc(window,message,wparam,lparam);
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_MOUSEMOVE){
        const DWORD hit=static_cast<DWORD>(SendMessageW(window,LB_ITEMFROMPOINT,0,lparam));
        state->HoverRow(HIWORD(hit)?-1:static_cast<int>(LOWORD(hit)));
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);
    }
    if(message==WM_MOUSELEAVE)state->HoverRow(-1);
    if(message==WM_CAPTURECHANGED || message==WM_CANCELMODE)state->pressed=-1;
    if(message==WM_LBUTTONDOWN){
        const DWORD hit=static_cast<DWORD>(SendMessageW(window,LB_ITEMFROMPOINT,0,lparam));
        state->pressed=HIWORD(hit)?-1:static_cast<int>(LOWORD(hit));
    }
    if(message==WM_LBUTTONUP){
        const int pressed=state->pressed;state->pressed=-1;
        const LRESULT result=DefSubclassProc(window,message,wparam,lparam);
        if(state->done || !IsWindow(window))return result;
        const DWORD hit=static_cast<DWORD>(SendMessageW(window,LB_ITEMFROMPOINT,0,lparam));
        const int index=HIWORD(hit)?-1:static_cast<int>(LOWORD(hit));
        if(index>=0 && index==pressed){
            RECT item{};SendMessageW(window,LB_GETITEMRECT,static_cast<WPARAM>(index),reinterpret_cast<LPARAM>(&item));
            const POINT p{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
            if(PtInRect(&item,p))state->Activate(index);
        }
        return result;
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,ListProc,1);
    return DefSubclassProc(window,message,wparam,lparam);
}

LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
    auto* state=reinterpret_cast<SearchState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){
        state=static_cast<SearchState*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        state->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
    }
    if(!state)return DefWindowProcW(window,message,wparam,lparam);
    try{
        switch(message){
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:state->Paint();return 0;
        case WM_MEASUREITEM:{
            auto* measure=reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
            if(measure->CtlID==ResultsList){measure->itemHeight=state->RowHeight();return TRUE;}break;
        }
        case WM_DRAWITEM:{
            const auto* draw=reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
            if(draw->CtlID==ResultsList){state->DrawRow(*draw);return TRUE;}
            if(draw->CtlID==ClearSearch){
                Fill(draw->hDC,draw->rcItem,state->Field);
                const bool focused=GetFocus()==state->clear;
                RECT inner=draw->rcItem;InflateRect(&inner,-1,-1);
                const COLORREF fill=(draw->itemState&ODS_SELECTED)?state->Selected:state->clear_hover?state->Hover:state->Field;
                Round(draw->hDC,inner,fill,focused?state->Accent:fill,state->D(12));
                Text(draw->hDC,L"×",draw->rcItem,state->title_font,state->Muted,DT_CENTER);return TRUE;
            }break;
        }
        case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:case WM_CTLCOLORSTATIC:{
            HDC dc=reinterpret_cast<HDC>(wparam);
            const HWND child=reinterpret_cast<HWND>(lparam);
            SetTextColor(dc,child==state->empty?state->Muted:state->Foreground);
            const bool edit=child==state->edit;
            SetBkColor(dc,edit?state->Field:state->Background);
            return reinterpret_cast<LRESULT>(edit?state->field:state->background);
        }
        case WM_COMMAND:
            if(LOWORD(wparam)==SearchEdit && HIWORD(wparam)==EN_CHANGE){state->Filter();return 0;}
            if(LOWORD(wparam)==ClearSearch && HIWORD(wparam)==BN_CLICKED){SetWindowTextW(state->edit,L"");SetFocus(state->edit);return 0;}
            if(LOWORD(wparam)==ResultsList && HIWORD(wparam)==LBN_DBLCLK){state->Activate(state->Selection());return 0;}
            break;
        case WM_MOUSEWHEEL:return SendMessageW(state->list,message,wparam,lparam);
        case WM_SETFOCUS:if(state->edit)SetFocus(state->edit);return 0;
        case WM_ACTIVATE:if(LOWORD(wparam)==WA_INACTIVE)state->End();return 0;
        case WM_CANCELMODE:case WM_CLOSE:case WM_DISPLAYCHANGE:case WM_DPICHANGED:state->End();return 0;
        case WM_DESTROY:state->done=true;return 0;
        case WM_NCDESTROY:state->window=nullptr;SetWindowLongPtrW(window,GWLP_USERDATA,0);break;
        default:break;
        }
    }catch(...){
        if(message==WM_NCCREATE)return FALSE;
        state->End();return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}

struct Cleanup {
    SearchState& state;
    ~Cleanup(){
        if(state.window && IsWindow(state.window))DestroyWindow(state.window);
        for(HFONT font:{state.font,state.bold_font,state.small_font,state.title_font,state.icon_font})if(font)DeleteObject(font);
        if(state.background)DeleteObject(state.background);
        if(state.field)DeleteObject(state.field);
    }
};

} // namespace

std::uint64_t ShowTabSearch(HWND owner,POINT anchor_screen,const std::vector<TabSearchItem>& items,UINT dpi,const Palette* palette,const SearchPickerOptions* options){
    if(!owner || !IsWindow(owner))return 0;
    if(HWND current=static_cast<HWND>(GetPropW(owner,OwnerProperty));current && IsWindow(current))
        SendMessageW(current,WM_CANCELMODE,0,0);
    if(!IsWindow(owner))return 0;
    SearchState state;
    Cleanup cleanup{state};
    if(options)state.options=*options;
    state.owner=owner;state.previous_focus=GetFocus();state.dpi=std::clamp(dpi,72u,768u);
    state.ApplyPalette(palette);
    for(std::size_t i=0;i<std::min<std::size_t>(items.size(),200);++i){
        if(!items[i].id)continue;
        auto item=items[i];item.title=Clean(item.title,512);item.url=Clean(item.url,16384);
        if(item.title.empty())item.title=L"Untitled tab";
        state.items.push_back(std::move(item));
    }
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfoW(MonitorFromPoint(anchor_screen,MONITOR_DEFAULTTONEAREST),&monitor))return 0;
    const RECT work=monitor.rcWork;const int margin=std::max(1,state.D(4));
    const int available_width=static_cast<int>(work.right-work.left)-margin*2;
    const int available_height=static_cast<int>(work.bottom-work.top)-margin*2;
    if(available_width<state.D(240) || available_height<state.D(200))return 0;
    state.width=std::min(state.D(464),available_width);
    const int rows=static_cast<int>(std::clamp<std::size_t>(state.items.size(),1,7));
    state.height=std::min(state.D(std::clamp(144+rows*56,200,536)),available_height);
    state.list_top=state.D(100);state.list_height=state.height-state.list_top-state.D(40);
    const int left=std::clamp(static_cast<int>(anchor_screen.x),static_cast<int>(work.left)+margin,
        static_cast<int>(work.right)-state.width-margin);
    const int top=std::clamp(static_cast<int>(anchor_screen.y),static_cast<int>(work.top)+margin,
        static_cast<int>(work.bottom)-state.height-margin);
    auto font=[&](int size,int weight,const wchar_t* face){
        return CreateFontW(-state.D(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,face);
    };
    state.font=font(13,FW_NORMAL,L"Segoe UI");state.bold_font=font(13,FW_SEMIBOLD,L"Segoe UI");state.small_font=font(12,FW_NORMAL,L"Segoe UI");
    state.title_font=font(16,FW_SEMIBOLD,L"Segoe UI");state.icon_font=font(18,FW_NORMAL,L"Segoe MDL2 Assets");
    state.background=CreateSolidBrush(state.Background);state.field=CreateSolidBrush(state.Field);
    if(!state.font || !state.bold_font || !state.small_font || !state.title_font || !state.icon_font || !state.background || !state.field)return 0;
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_DROPSHADOW;
    wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=WindowClass;
    if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    HWND popup=CreateWindowExW(WS_EX_TOOLWINDOW,WindowClass,state.options.title,WS_POPUP|WS_CLIPCHILDREN,
        left,top,state.width,state.height,owner,nullptr,instance,&state);
    if(!popup)return 0;
    const BOOL dark=TRUE;DwmSetWindowAttribute(popup,20,&dark,sizeof(dark));
    HRGN region=CreateRoundRectRgn(0,0,state.width+1,state.height+1,state.D(24),state.D(24));
    if(region && !SetWindowRgn(popup,region,FALSE))DeleteObject(region);
    // The native label immediately precedes the EDIT in tab order for assistive technology.
    state.label=CreateWindowExW(0,L"STATIC",state.options.title,WS_CHILD|WS_VISIBLE|SS_LEFT,
        state.D(16),state.D(14),state.width-state.D(130),state.D(21),popup,nullptr,instance,nullptr);
    if(state.label)SendMessageW(state.label,WM_SETFONT,reinterpret_cast<WPARAM>(state.title_font),FALSE);
    state.edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
        state.D(47),state.D(53),state.width-state.D(102),state.D(22),popup,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(SearchEdit)),instance,nullptr);
    state.clear=CreateWindowExW(0,L"BUTTON",L"Clear search",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,
        state.width-state.D(48),state.D(47),state.D(32),state.D(32),popup,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ClearSearch)),instance,nullptr);
    state.list=CreateWindowExW(0,L"LISTBOX",state.options.title,
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,
        state.D(4),state.list_top,state.width-state.D(8),state.list_height,popup,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ResultsList)),instance,nullptr);
    state.empty=CreateWindowExW(0,L"STATIC",L"No matching tabs.\nTry a page title or address.",
        WS_CHILD|SS_CENTER,state.D(18),state.list_top+state.D(8),state.width-state.D(36),
        std::min(state.D(60),state.list_height-state.D(16)),
        popup,nullptr,instance,nullptr);
    if(!state.edit || !state.list || !state.clear || !state.empty)return 0;
    for(HWND control:{state.edit,state.clear,state.list,state.empty})
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(state.font),FALSE);
    SendMessageW(state.edit,EM_SETLIMITTEXT,512,0);
    SendMessageW(state.edit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
    SendMessageW(state.list,LB_SETITEMHEIGHT,0,state.RowHeight());
    SetWindowTheme(state.list,L"DarkMode_Explorer",nullptr);
    SetWindowSubclass(state.edit,EditProc,1,reinterpret_cast<DWORD_PTR>(&state));
    SetWindowSubclass(state.list,ListProc,1,reinterpret_cast<DWORD_PTR>(&state));
    SetWindowSubclass(state.clear,ClearProc,1,reinterpret_cast<DWORD_PTR>(&state));
    state.Filter();if(state.done)return 0;
    SetPropW(owner,OwnerProperty,popup);
    ShowWindow(popup,SW_SHOW);UpdateWindow(popup);SetForegroundWindow(popup);SetFocus(state.edit);
    NotifyWinEvent(EVENT_SYSTEM_DIALOGSTART,popup,OBJID_WINDOW,CHILDID_SELF);
    bool repost_quit=false;int quit_code=0;
    while(!state.done && state.window && IsWindow(owner)){
        MSG message{};const BOOL status=GetMessageW(&message,nullptr,0,0);
        if(status<=0){repost_quit=status==0;quit_code=static_cast<int>(message.wParam);state.End();break;}
        const bool belongs=message.hwnd==popup || IsChild(popup,message.hwnd);
        if(belongs && message.message==WM_KEYDOWN && state.Key(static_cast<UINT>(message.wParam)))continue;
        if(belongs && message.message==WM_CHAR && message.hwnd==state.list && message.wParam>=32){
            SetFocus(state.edit);SendMessageW(state.edit,message.message,message.wParam,message.lParam);continue;
        }
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    state.End();
    if(IsWindow(owner)){
        if(GetPropW(owner,OwnerProperty)==popup)RemovePropW(owner,OwnerProperty);
        if(GetForegroundWindow()==owner && state.previous_focus && IsWindow(state.previous_focus) &&
            (state.previous_focus==owner || IsChild(owner,state.previous_focus)))SetFocus(state.previous_focus);
        NotifyWinEvent(EVENT_SYSTEM_DIALOGEND,owner,OBJID_WINDOW,CHILDID_SELF);
    }
    if(repost_quit)PostQuitMessage(quit_code);
    return state.result;
}

} // namespace webcom
