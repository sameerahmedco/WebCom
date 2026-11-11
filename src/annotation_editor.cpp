#ifdef _WIN32
#include "annotation_editor.h"
#include "win32_com_compat.h"

#include <commdlg.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace webcom {
namespace {

enum : int { IdPen=1001, IdRect=1002, IdUndo=1003, IdCopy=1004, IdSave=1005, IdDone=1006 };
enum class Tool { Pen, Rectangle };

struct EditorState {
    HWND window{};
    HWND owner{};
    UINT dpi=96;
    std::unique_ptr<Gdiplus::Bitmap> image;
    std::unique_ptr<Gdiplus::Bitmap> undo;
    Tool tool=Tool::Pen;
    bool drawing=false;
    POINT start{};
    POINT last{};
    RECT image_rect{};
    bool done=false;
};

int D(const EditorState& state,int value){return MulDiv(value,static_cast<int>(state.dpi),96);}

std::unique_ptr<Gdiplus::Bitmap> CloneBitmap(Gdiplus::Bitmap& source){
    auto* clone=source.Clone(0,0,static_cast<INT>(source.GetWidth()),static_cast<INT>(source.GetHeight()),PixelFormat32bppARGB);
    return std::unique_ptr<Gdiplus::Bitmap>(clone);
}

std::optional<CLSID> PngEncoder(){
    UINT count=0,bytes=0;if(Gdiplus::GetImageEncodersSize(&count,&bytes)!=Gdiplus::Ok||!count||!bytes)return std::nullopt;
    std::vector<std::byte> storage(bytes);auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    if(Gdiplus::GetImageEncoders(count,bytes,encoders)!=Gdiplus::Ok)return std::nullopt;
    for(UINT i=0;i<count;++i)if(encoders[i].MimeType&&_wcsicmp(encoders[i].MimeType,L"image/png")==0)return encoders[i].Clsid;
    return std::nullopt;
}

bool CopyBitmap(HWND owner,Gdiplus::Bitmap& bitmap){
    HBITMAP handle=nullptr;if(bitmap.GetHBITMAP(Gdiplus::Color(255,255,255,255),&handle)!=Gdiplus::Ok||!handle)return false;
    if(!OpenClipboard(owner)){DeleteObject(handle);return false;}
    struct Close{~Close(){CloseClipboard();}} close;
    if(!EmptyClipboard()){DeleteObject(handle);return false;}
    if(!SetClipboardData(CF_BITMAP,handle)){DeleteObject(handle);return false;}
    return true;
}

bool SaveBitmap(HWND owner,Gdiplus::Bitmap& bitmap){
    wchar_t file[MAX_PATH]=L"WebCom-annotated.png";OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"PNG image (*.png)\0*.png\0All files (*.*)\0*.*\0\0";dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"png";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetSaveFileNameW(&dialog))return true;
    const auto encoder=PngEncoder();return encoder&&bitmap.Save(file,&*encoder,nullptr)==Gdiplus::Ok;
}

POINT ToImage(const EditorState& state,POINT point){
    const int display_w=std::max(1,static_cast<int>(state.image_rect.right-state.image_rect.left));
    const int display_h=std::max(1,static_cast<int>(state.image_rect.bottom-state.image_rect.top));
    const int image_w=static_cast<int>(state.image->GetWidth()),image_h=static_cast<int>(state.image->GetHeight());
    const LONG right=state.image_rect.right-static_cast<LONG>(1);
    const LONG bottom=state.image_rect.bottom-static_cast<LONG>(1);
    point.x=std::clamp<LONG>(point.x,state.image_rect.left,right);
    point.y=std::clamp<LONG>(point.y,state.image_rect.top,bottom);
    const int image_x=static_cast<int>((point.x-state.image_rect.left)*static_cast<LONG>(image_w)/display_w);
    const int image_y=static_cast<int>((point.y-state.image_rect.top)*static_cast<LONG>(image_h)/display_h);
    return {static_cast<LONG>(std::clamp(image_x,0,image_w-1)),
            static_cast<LONG>(std::clamp(image_y,0,image_h-1))};
}

void Layout(EditorState& state){
    RECT client{};GetClientRect(state.window,&client);const int toolbar=D(state,52);const int pad=D(state,16);
    const int available_w=std::max(1,static_cast<int>(client.right-client.left)-pad*2);
    const int available_h=std::max(1,static_cast<int>(client.bottom-client.top)-toolbar-pad*2);
    const double sx=static_cast<double>(available_w)/std::max<UINT>(1,state.image->GetWidth());
    const double sy=static_cast<double>(available_h)/std::max<UINT>(1,state.image->GetHeight());
    const double scale=std::min({1.0,sx,sy});
    const int width=std::max(1,static_cast<int>(state.image->GetWidth()*scale));
    const int height=std::max(1,static_cast<int>(state.image->GetHeight()*scale));
    state.image_rect={pad,toolbar+pad,pad+width,toolbar+pad+height};
    const int bw=D(state,92),gap=D(state,8),top=D(state,8),bh=D(state,34);int x=pad;
    for(int id:{IdPen,IdRect,IdUndo,IdCopy,IdSave,IdDone}){HWND button=GetDlgItem(state.window,id);if(button){SetWindowPos(button,nullptr,x,top,bw,bh,SWP_NOZORDER|SWP_NOACTIVATE);x+=bw+gap;}}
}

void DrawRectangle(EditorState& state,POINT a,POINT b){
    a=ToImage(state,a);b=ToImage(state,b);const int left=std::min(a.x,b.x),top=std::min(a.y,b.y),right=std::max(a.x,b.x),bottom=std::max(a.y,b.y);
    if(right-left<2||bottom-top<2)return;Gdiplus::Graphics graphics(state.image.get());graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Gdiplus::Color(255,232,62,62),4.0f);graphics.DrawRectangle(&pen,left,top,right-left,bottom-top);
}

LRESULT CALLBACK EditorProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
    auto* state=reinterpret_cast<EditorState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){auto* create=reinterpret_cast<CREATESTRUCTW*>(lparam);state=static_cast<EditorState*>(create->lpCreateParams);state->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
    if(!state)return DefWindowProcW(window,message,wparam,lparam);
    switch(message){
    case WM_SIZE:Layout(*state);InvalidateRect(window,nullptr,TRUE);return 0;
    case WM_COMMAND:{const int id=LOWORD(wparam);if(id==IdPen){state->tool=Tool::Pen;return 0;}if(id==IdRect){state->tool=Tool::Rectangle;return 0;}
        if(id==IdUndo&&state->undo){state->image=CloneBitmap(*state->undo);state->undo.reset();Layout(*state);InvalidateRect(window,nullptr,TRUE);return 0;}
        if(id==IdCopy){MessageBeep(CopyBitmap(window,*state->image)?MB_OK:MB_ICONWARNING);return 0;}
        if(id==IdSave){if(!SaveBitmap(window,*state->image))MessageBeep(MB_ICONWARNING);return 0;}
        if(id==IdDone){state->done=true;DestroyWindow(window);return 0;}break;}
    case WM_LBUTTONDOWN:{POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};if(PtInRect(&state->image_rect,point)){state->undo=CloneBitmap(*state->image);state->drawing=true;state->start=state->last=point;SetCapture(window);}return 0;}
    case WM_MOUSEMOVE:if(state->drawing&&(wparam&MK_LBUTTON)){POINT now{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};if(state->tool==Tool::Pen){const POINT a=ToImage(*state,state->last),b=ToImage(*state,now);Gdiplus::Graphics graphics(state->image.get());graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen pen(Gdiplus::Color(255,232,62,62),4.0f);graphics.DrawLine(&pen,static_cast<INT>(a.x),static_cast<INT>(a.y),static_cast<INT>(b.x),static_cast<INT>(b.y));}state->last=now;InvalidateRect(window,&state->image_rect,FALSE);}return 0;
    case WM_LBUTTONUP:if(state->drawing){POINT now{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};if(state->tool==Tool::Rectangle)DrawRectangle(*state,state->start,now);state->drawing=false;if(GetCapture()==window)ReleaseCapture();InvalidateRect(window,&state->image_rect,FALSE);}return 0;
    case WM_KEYDOWN:if(wparam==VK_ESCAPE){state->done=true;DestroyWindow(window);return 0;}if((GetKeyState(VK_CONTROL)&0x8000)&&wparam=='S'){SaveBitmap(window,*state->image);return 0;}if((GetKeyState(VK_CONTROL)&0x8000)&&wparam=='C'){CopyBitmap(window,*state->image);return 0;}break;
    case WM_PAINT:{PAINTSTRUCT paint{};HDC dc=BeginPaint(window,&paint);RECT client{};GetClientRect(window,&client);FillRect(dc,&client,GetSysColorBrush(COLOR_WINDOW));if(state->image){Gdiplus::Graphics graphics(dc);graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);graphics.DrawImage(state->image.get(),Gdiplus::Rect(state->image_rect.left,state->image_rect.top,state->image_rect.right-state->image_rect.left,state->image_rect.bottom-state->image_rect.top));if(state->drawing&&state->tool==Tool::Rectangle){Gdiplus::Pen pen(Gdiplus::Color(255,232,62,62),2.0f);const int l=std::min(state->start.x,state->last.x),t=std::min(state->start.y,state->last.y),r=std::max(state->start.x,state->last.x),b=std::max(state->start.y,state->last.y);graphics.DrawRectangle(&pen,l,t,r-l,b-t);}}EndPaint(window,&paint);return 0;}
    case WM_CLOSE:state->done=true;DestroyWindow(window);return 0;
    case WM_DESTROY:state->done=true;return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}

} // namespace

bool ShowScreenshotAnnotationEditor(HWND owner,HINSTANCE instance,IStream* png_stream,UINT dpi){
    if(!owner||!instance||!png_stream)return false;LARGE_INTEGER zero{};if(FAILED(png_stream->Seek(zero,STREAM_SEEK_SET,nullptr)))return false;
    Gdiplus::Bitmap source(png_stream,FALSE);if(source.GetLastStatus()!=Gdiplus::Ok||!source.GetWidth()||!source.GetHeight())return false;
    EditorState state;state.owner=owner;state.dpi=dpi?dpi:96;state.image=std::make_unique<Gdiplus::Bitmap>(static_cast<INT>(source.GetWidth()),static_cast<INT>(source.GetHeight()),PixelFormat32bppARGB);
    if(!state.image||state.image->GetLastStatus()!=Gdiplus::Ok)return false;{Gdiplus::Graphics graphics(state.image.get());graphics.DrawImage(&source,0,0,static_cast<INT>(source.GetWidth()),static_cast<INT>(source.GetHeight()));}
    static ATOM atom=0;if(!atom){WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.lpfnWndProc=EditorProc;cls.hInstance=instance;cls.hCursor=LoadCursor(nullptr,IDC_CROSS);cls.hbrBackground=GetSysColorBrush(COLOR_WINDOW);cls.lpszClassName=L"WebComScreenshotAnnotation";atom=RegisterClassExW(&cls);if(!atom&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;}
    const int width=MulDiv(980,static_cast<int>(state.dpi),96),height=MulDiv(720,static_cast<int>(state.dpi),96);
    HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,L"WebComScreenshotAnnotation",L"WebCom - Annotate screenshot",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,width,height,owner,nullptr,instance,&state);if(!window)return false;
    const struct {int id;const wchar_t* text;} buttons[]{{IdPen,L"Pen"},{IdRect,L"Rectangle"},{IdUndo,L"Undo"},{IdCopy,L"Copy"},{IdSave,L"Save PNG"},{IdDone,L"Done"}};
    for(const auto& item:buttons)CreateWindowExW(0,L"BUTTON",item.text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(item.id)),instance,nullptr);
    Layout(state);const BOOL owner_enabled=IsWindowEnabled(owner);if(owner_enabled)EnableWindow(owner,FALSE);SetForegroundWindow(window);SetFocus(GetDlgItem(window,IdPen));
    MSG message{};while(!state.done){const BOOL result=GetMessageW(&message,nullptr,0,0);if(result<=0){if(result==0)PostQuitMessage(static_cast<int>(message.wParam));break;}if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    if(IsWindow(window))DestroyWindow(window);if(owner_enabled){EnableWindow(owner,TRUE);SetForegroundWindow(owner);SetFocus(owner);}return true;
}

} // namespace webcom
#endif
