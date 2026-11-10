#include "app.h"
#include "core.h"
#include "storage.h"
#include "bookmarks.h"
#include "dark_menu.h"
#include "render_helpers.h"
#include "tab_search.h"
#include "launch_request.h"
#include "appearance.h"
#include "omnibox_model.h"
#include "history_model.h"
#include "site_permissions.h"
#include "win32_com_compat.h"
#include <wrl.h>
#include <WebView2.h>
#include "com_callback.h"
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>
#include <gdiplus.h>
#include "favicon_image.h"
#include <shlwapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <functional>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace webcom {
namespace {
constexpr UINT kTick = 1;
constexpr UINT kSave = 2;
constexpr UINT kToast = 3;
constexpr UINT kUiFrame = 4;
constexpr UINT kAnimationFrame = 5;
constexpr UINT kDownloadTick = 6;
constexpr UINT kNotice = WM_APP + 1;
constexpr UINT kCloseTab = WM_APP + 2;
constexpr UINT kRunAction = WM_APP + 3;
constexpr UINT kPageFullscreen = WM_APP + 4;
constexpr UINT kSelectTab = WM_APP + 5;
constexpr COLORREF kCaption = RGB(31, 32, 32);
constexpr COLORREF kFrame = RGB(60, 60, 60);
constexpr COLORREF kAccent = RGB(171, 199, 248);
constexpr std::size_t kMaxTabs = 200;
enum Command : UINT {
    Back = 100, Forward, Reload, Home, NewTab, Address, Profile, Saver, More,
    HomeSearch, HomeGo, Google, YouTube, GitHub, TabStrip,
    TabList, MinimizeWindow, MaximizeWindow, CloseWindow, AiMode, BookmarkPage, BookmarksOverflow, SiteInfo, DownloadsQuick,
    CloseCurrent = 200, ReopenTab, PrivateTab, ProfilesMenu, AddProfile,
    ToggleSaver, PinTab, KeepAwake, SuspendNow, DiscardTab, DuplicateTab,
    MemoryReport, Downloads, TogglePasswords, OpenInDefault, About,
    FocusAddress, NextTab, PreviousTab, FullScreen, Exit, ClearData,
    EngineCrashed = 400, TabCrashed, InstallRuntime,
    BookmarkMenu, PasswordMenu, SettingsMenu, ZoomIn, ZoomOut, ZoomReset, PrintPage,
    SearchInPage, DeveloperTools, HelpMenu, RemoveBookmark, PrivateHelp, History, ClearHistory, HistoryDeleteMatches, HistoryDeleteWeek, HistoryDeleteOlderMonth,
    ClearDownloadsList, OpenDownloadsFolder, MuteTab, MoveTabLeft, MoveTabRight, NewProfileWindow,
    AppearanceMenu, ThemeMenu, ProfileColorMenu, ToggleBookmarks, ColorAutomatic, ColorCustom, AppearanceReset, BookmarkManager,
    NativeFilter, ImportBookmarks, ExportBookmarks, NewBookmarkFolder, NewTabGroup, RemoveTabGroup,
    ToggleTabGroup, RenameTabGroup, CloseTabGroup, ProfileManager, OpenAllProfiles, CloseOtherProfiles, RenameBookmarkFolderCmd, ResetSitePermissionsCmd,
    ShowSiteCookiesCmd, ClearSiteCookiesCmd, DeleteCurrentProfileCmd,
    ProfileBase = 1000, SelectTabBase = 2000, BookmarkBase = 3000, OpenTabBase = 4000,
    ThemeBase = 5000, ColorBase = 5100, HistoryBase = 6000, DownloadBase = 7000, RemoveBookmarkBase = 8000,
    MoveProfileBase = 9000, GroupBase = 10000, BookmarkFolderBase = 11000, BookmarkBarFolderBase = 12000,
    GroupColorBase = 13000, DeleteHistoryBase = 14000,
    PauseDownloadBase = 15000, CancelDownloadBase = 15300, RetryDownloadBase = 15600,
    SitePermissionBase = 16000
};

COLORREF NativeColor(std::uint32_t rgb) {
    return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

constexpr std::array<std::uint32_t,8> kProfileColors{
    0xA8C7FA,0x81C995,0xFDD663,0xF6AEA9,0xC7B3F4,0x78D9EC,0xFFB77D,0xBEC6D3};

struct CoString {
    LPWSTR value = nullptr;
    ~CoString() { CoTaskMemFree(value); }
    std::wstring str() const { return value ? value : L""; }
};

std::wstring Wide(std::string_view value) {
    if (value.empty()) return {};
    if (value.size() > 1048576) return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                    static_cast<int>(value.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring result(n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), n);
    return result;
}

std::string Utf8(std::wstring_view value) {
    if (value.empty()) return {};
    if (value.size() > 1048576) return {};
    const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                    static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string result(n, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), n, nullptr, nullptr);
    return result;
}

std::wstring ControlText(HWND window) {
    const int n = GetWindowTextLengthW(window);
    std::wstring result(static_cast<std::size_t>(n) + 1, L'\0');
    GetWindowTextW(window, result.data(), n + 1);
    result.resize(n);
    return result;
}

bool IsHome(std::wstring_view value) {
    return value.empty() || value == L"webcom:newtab";
}

enum class InternalPage { None, NewTab, History, Downloads, Settings, Bookmarks, Profiles };

InternalPage InternalPageFor(std::wstring_view value) {
    if (IsHome(value)) return InternalPage::NewTab;
    if (value == L"webcom:history") return InternalPage::History;
    if (value == L"webcom:downloads") return InternalPage::Downloads;
    if (value == L"webcom:settings") return InternalPage::Settings;
    if (value == L"webcom:bookmarks") return InternalPage::Bookmarks;
    if (value == L"webcom:profiles") return InternalPage::Profiles;
    return InternalPage::None;
}

bool IsInternalPage(std::wstring_view value) { return InternalPageFor(value) != InternalPage::None; }

std::wstring InternalPageTitle(std::wstring_view value, bool private_mode = false) {
    switch (InternalPageFor(value)) {
    case InternalPage::NewTab: return private_mode ? L"New Incognito Tab" : L"New Tab";
    case InternalPage::History: return L"History";
    case InternalPage::Downloads: return L"Downloads";
    case InternalPage::Settings: return L"Settings";
    case InternalPage::Bookmarks: return L"Bookmarks";
    case InternalPage::Profiles: return L"Profiles";
    default: return std::wstring(value.substr(0, 250));
    }
}

bool IsWebAddress(std::wstring_view value) {
    const auto utf = Utf8(value);
    const auto result = ResolveAddress(utf);
    const bool explicit_web_scheme = (value.size() >= 8 && _wcsnicmp(value.data(), L"https://", 8) == 0) ||
                                     (value.size() >= 7 && _wcsnicmp(value.data(), L"http://", 7) == 0);
    return result.allowed && explicit_web_scheme;
}

std::optional<SitePermissionKind> SitePermissionKindFor(COREWEBVIEW2_PERMISSION_KIND kind) {
    switch (kind) {
    case COREWEBVIEW2_PERMISSION_KIND_CAMERA: return SitePermissionKind::Camera;
    case COREWEBVIEW2_PERMISSION_KIND_MICROPHONE: return SitePermissionKind::Microphone;
    case COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION: return SitePermissionKind::Geolocation;
    case COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS: return SitePermissionKind::Notifications;
    default: return std::nullopt;
    }
}

COREWEBVIEW2_PERMISSION_STATE WebPermissionState(SitePermissionDecision decision) {
    switch (decision) {
    case SitePermissionDecision::Allow: return COREWEBVIEW2_PERMISSION_STATE_ALLOW;
    case SitePermissionDecision::Block: return COREWEBVIEW2_PERMISSION_STATE_DENY;
    case SitePermissionDecision::Ask: default: return COREWEBVIEW2_PERMISSION_STATE_DEFAULT;
    }
}

COREWEBVIEW2_PERMISSION_KIND WebPermissionKind(SitePermissionKind kind) {
    switch (kind) {
    case SitePermissionKind::Camera: return COREWEBVIEW2_PERMISSION_KIND_CAMERA;
    case SitePermissionKind::Microphone: return COREWEBVIEW2_PERMISSION_KIND_MICROPHONE;
    case SitePermissionKind::Geolocation: return COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION;
    case SitePermissionKind::Notifications: return COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS;
    }
    return COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION;
}

bool IsCertificateErrorStatus(COREWEBVIEW2_WEB_ERROR_STATUS status) {
    switch (status) {
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_COMMON_NAME_IS_INCORRECT:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_EXPIRED:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CLIENT_CERTIFICATE_CONTAINS_ERRORS:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_REVOKED:
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_IS_INVALID:
        return true;
    default:
        return false;
    }
}

std::wstring CertificateErrorLabel(COREWEBVIEW2_WEB_ERROR_STATUS status) {
    switch (status) {
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_COMMON_NAME_IS_INCORRECT: return L"Certificate name does not match this site";
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_EXPIRED: return L"Certificate has expired";
    case COREWEBVIEW2_WEB_ERROR_STATUS_CLIENT_CERTIFICATE_CONTAINS_ERRORS: return L"Client certificate contains errors";
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_REVOKED: return L"Certificate has been revoked";
    case COREWEBVIEW2_WEB_ERROR_STATUS_CERTIFICATE_IS_INVALID: return L"Certificate is invalid";
    default: return L"Certificate could not be verified";
    }
}

std::wstring HResultText(HRESULT value) {
    std::wostringstream out;
    out << L"0x" << std::hex << std::uppercase << static_cast<unsigned long>(value);
    return out.str();
}

std::wstring HistoryTimeLabel(std::uint64_t unix_ms) {
    constexpr std::uint64_t kWindowsEpochOffsetMs = 11644473600000ull;
    ULARGE_INTEGER value{}; value.QuadPart = (unix_ms + kWindowsEpochOffsetMs) * 10000ull;
    FILETIME file_time{value.LowPart, value.HighPart};
    SYSTEMTIME utc{}, local{};
    if (!FileTimeToSystemTime(&file_time, &utc) ||
        !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return {};
    wchar_t buffer[32]{};
    if (!GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, buffer, 32)) return {};
    return buffer;
}

std::wstring HistoryDateLabel(std::uint64_t unix_ms) {
    constexpr std::uint64_t kWindowsEpochOffsetMs = 11644473600000ull;
    ULARGE_INTEGER value{}; value.QuadPart=(unix_ms+kWindowsEpochOffsetMs)*10000ull;
    FILETIME ft{value.LowPart,value.HighPart};SYSTEMTIME utc{},local{};
    if(!FileTimeToSystemTime(&ft,&utc) || !SystemTimeToTzSpecificLocalTime(nullptr,&utc,&local)) return L"Earlier";
    SYSTEMTIME now{};GetLocalTime(&now);
    if(local.wYear==now.wYear && local.wMonth==now.wMonth && local.wDay==now.wDay) return L"Today";
    SYSTEMTIME a=local,b=now;a.wHour=a.wMinute=a.wSecond=a.wMilliseconds=0;b.wHour=b.wMinute=b.wSecond=b.wMilliseconds=0;
    FILETIME fa{},fb{};ULARGE_INTEGER ua{},ub{};
    if(SystemTimeToFileTime(&a,&fa) && SystemTimeToFileTime(&b,&fb)) {
        ua.LowPart=fa.dwLowDateTime;ua.HighPart=fa.dwHighDateTime;ub.LowPart=fb.dwLowDateTime;ub.HighPart=fb.dwHighDateTime;
        constexpr unsigned long long day=24ull*60ull*60ull*10000000ull;
        if(ub.QuadPart>ua.QuadPart && (ub.QuadPart-ua.QuadPart)/day==1) return L"Yesterday";
    }
    wchar_t buffer[64]{};
    if(GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,DATE_LONGDATE,&local,nullptr,buffer,64,nullptr)) return buffer;
    return L"Earlier";
}

std::wstring FormatBytes(std::int64_t bytes) {
    if (bytes < 0) return L"Unknown size";
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) { value /= 1024.0; ++unit; }
    std::wostringstream out;
    if (unit == 0) out << static_cast<long long>(value);
    else if (value >= 100.0) out << std::fixed << std::setprecision(0) << value;
    else out << std::fixed << std::setprecision(1) << value;
    out << L" " << units[unit];
    return out.str();
}

std::wstring DownloadStateLabel(COREWEBVIEW2_DOWNLOAD_STATE state) {
    switch (state) {
    case COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS: return L"Downloading";
    case COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED: return L"Completed";
    case COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED: return L"Interrupted";
    default: return L"Unknown";
    }
}

std::wstring SafeTitle(std::wstring_view value) {
    std::wstring result;
    result.reserve(std::min<std::size_t>(value.size(), 250));
    for (std::size_t i = 0; i < value.size() && result.size() < 250; ++i) {
        const wchar_t c = value[i];
        if (c < 0x20 || c == 0x7f) result.push_back(L' ');
        else if (c >= 0xd800 && c <= 0xdbff) {
            if (i + 1 < value.size() && value[i + 1] >= 0xdc00 && value[i + 1] <= 0xdfff) {
                if (result.size() + 2 > 250) break;
                result.push_back(c); result.push_back(value[++i]);
            } else result.push_back(L'\ufffd');
        } else if (c >= 0xdc00 && c <= 0xdfff) result.push_back(L'\ufffd');
        else result.push_back(c);
    }
    return result;
}

void Fill(HDC dc, RECT r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

void Rounded(HDC dc, RECT r, COLORREF fill, COLORREF border, int radius) {
    using namespace Gdiplus;
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    const REAL x = static_cast<REAL>(r.left), y = static_cast<REAL>(r.top);
    const REAL w = static_cast<REAL>(r.right - r.left - 1), h = static_cast<REAL>(r.bottom - r.top - 1);
    const REAL d = std::max(1.0f, std::min({static_cast<REAL>(radius), w, h}));
    GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90); path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90); path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    SolidBrush brush(Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
    Pen pen(Color(255, GetRValue(border), GetGValue(border), GetBValue(border)), 1.0f);
    graphics.FillPath(&brush, &path);
    if (fill != border) graphics.DrawPath(&pen, &path);
}

void FocusOutline(HDC dc, RECT r, int diameter, int thickness, COLORREF color = kAccent) {
    using namespace Gdiplus;
    Graphics graphics(dc); graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    const REAL inset = static_cast<REAL>(thickness) / 2;
    const REAL x = r.left + inset, y = r.top + inset;
    const REAL w = r.right - r.left - 2 * inset, h = r.bottom - r.top - 2 * inset;
    const REAL d = std::min(static_cast<REAL>(diameter), h);
    GraphicsPath p;
    p.AddArc(x,y,d,d,180,90); p.AddArc(x+w-d,y,d,d,270,90);
    p.AddArc(x+w-d,y+h-d,d,d,0,90); p.AddArc(x,y+h-d,d,d,90,90); p.CloseFigure();
    Pen pen(Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), static_cast<REAL>(thickness));
    graphics.DrawPath(&pen, &p);
}

enum class Glyph { Back, Forward, Reload, Stop, Close, Plus, More, Down, User, Star, Search,
                   Globe, Folder, Minimize, Maximize, Restore, Spark, Shield, Check, Speaker, Muted, Download };

void Icon(HDC dc, RECT r, Glyph glyph, COLORREF color, bool filled = false) {
    using namespace Gdiplus;
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.TranslateTransform(static_cast<REAL>(r.left), static_cast<REAL>(r.top));
    g.ScaleTransform(static_cast<REAL>(r.right-r.left)/24, static_cast<REAL>(r.bottom-r.top)/24);
    const Color c(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Pen pen(c, 1.8f); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound);
    SolidBrush brush(c);
    auto line = [&](REAL a,REAL b,REAL x,REAL y){g.DrawLine(&pen,a,b,x,y);};
    switch (glyph) {
    case Glyph::Back: line(19,12,5,12); line(5,12,11,6); line(5,12,11,18); break;
    case Glyph::Forward: line(5,12,19,12); line(19,12,13,6); line(19,12,13,18); break;
    case Glyph::Reload: g.DrawArc(&pen,5.0f,5.0f,14.0f,14.0f,30.0f,300.0f); line(19,4,19,10); line(19,10,13,10); break;
    case Glyph::Stop: g.FillRectangle(&brush,6.0f,6.0f,12.0f,12.0f); break;
    case Glyph::Close: line(6,6,18,18); line(18,6,6,18); break;
    case Glyph::Plus: line(5,12,19,12); line(12,5,12,19); break;
    case Glyph::More: for(int i=0;i<3;++i)g.FillEllipse(&brush,10.5f,4.0f+i*6,3.0f,3.0f); break;
    case Glyph::Down: line(6,9,12,15); line(12,15,18,9); break;
    case Glyph::User: g.DrawEllipse(&pen,8.0f,3.0f,8.0f,8.0f); g.DrawArc(&pen,4.0f,14.0f,16.0f,12.0f,180.0f,180.0f); break;
    case Glyph::Search: g.DrawEllipse(&pen,4.0f,4.0f,11.0f,11.0f); line(14,14,20,20); break;
    case Glyph::Globe: g.DrawEllipse(&pen,3.0f,3.0f,18.0f,18.0f); g.DrawEllipse(&pen,8.0f,3.0f,8.0f,18.0f); line(3,12,21,12); break;
    case Glyph::Folder: {
        PointF outline[6]={{3,7},{9,7},{11,10},{21,10},{21,19},{3,19}};
        g.DrawPolygon(&pen,outline,6); line(3,7,3,19); break;
    }
    case Glyph::Minimize: line(5,12,19,12); break;
    case Glyph::Maximize: g.DrawRectangle(&pen,5.0f,5.0f,14.0f,14.0f); break;
    case Glyph::Restore: g.DrawRectangle(&pen,5.0f,8.0f,11.0f,11.0f); line(8,5,19,5);line(19,5,19,16);break;
    case Glyph::Check: line(4,12,9,17);line(9,17,20,6);break;
    case Glyph::Download: line(12,4,12,15); line(7,10,12,15); line(12,15,17,10); line(5,20,19,20); break;
    case Glyph::Speaker: case Glyph::Muted: {
        PointF speaker[6]={{3,9},{7,9},{12,5},{12,19},{7,15},{3,15}};
        g.DrawPolygon(&pen,speaker,6);
        if(glyph==Glyph::Muted){line(16,9,22,15);line(22,9,16,15);}
        else {g.DrawArc(&pen,9.0f,7.0f,10.0f,10.0f,-60.0f,120.0f);g.DrawArc(&pen,7.0f,3.0f,18.0f,18.0f,-50.0f,100.0f);}
        break;
    }
    case Glyph::Star: {
        PointF points[10];
        constexpr REAL xs[10]={12,14.8f,21,16.5f,17.8f,12,6.2f,7.5f,3,9.2f};
        constexpr REAL ys[10]={3,9,9.7f,14.3f,21,17.6f,21,14.3f,9.7f,9};
        for(int i=0;i<10;++i)points[i]={xs[i],ys[i]};
        if(filled)g.FillPolygon(&brush,points,10);else g.DrawPolygon(&pen,points,10);break;
    }
    case Glyph::Spark: {
        PointF p[8]={{11,3},{13,9},{19,11},{13,13},{11,19},{9,13},{3,11},{9,9}};
        g.DrawPolygon(&pen,p,8);line(19,2,19,6);line(17,4,21,4);break;
    }
    case Glyph::Shield: {
        PointF p[6]={{12,3},{20,6},{19,15},{12,21},{5,15},{4,6}};
        g.DrawPolygon(&pen,p,6);break;
    }
    }
}

void PrivateIcon(HDC dc, RECT r, bool circle) {
    using namespace Gdiplus;
    Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.TranslateTransform(static_cast<REAL>(r.left),static_cast<REAL>(r.top));
    g.ScaleTransform(static_cast<REAL>(r.right-r.left)/120,static_cast<REAL>(r.bottom-r.top)/120);
    const Color ink = circle ? Color(255,60,60,60) : Color(255,189,193,198);
    SolidBrush foreground(ink), background(Color(255,218,220,224));
    if(circle)g.FillEllipse(&background,0.0f,0.0f,120.0f,120.0f);
    PointF hat[8]={{38,53},{48,29},{53,27},{60,29},{67,27},{72,29},{82,53},{38,53}};
    g.FillPolygon(&foreground,hat,8);g.FillRectangle(&foreground,25.0f,57.0f,70.0f,4.0f);
    Pen pen(ink,4.0f);
    g.DrawEllipse(&pen,31.0f,66.0f,24.0f,24.0f);g.DrawEllipse(&pen,65.0f,66.0f,24.0f,24.0f);
    g.DrawArc(&pen,55.0f,72.0f,10.0f,9.0f,180.0f,180.0f);
}

void Text(HDC dc, const std::wstring& value, RECT r, HFONT font, COLORREF color,
          UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX) {
    const auto old_font = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &r, flags);
    SelectObject(dc, old_font);
}

// Replace the existing NameDialog, NameProc and AskName block in app.cpp.
// Reuses the existing native drawing helpers and palette immediately above it.
// The profile-name validation stays in BrowserApp::Impl::CreateProfile.
struct NameDialog {
    std::wstring value;
    std::wstring initial=L"Work",accept=L"Create";
    bool color_prompt=false;
    std::vector<std::wstring> existing_names;
    Palette palette=MakePalette(Appearance{},"Default");
    HBRUSH background = CreateSolidBrush(kCaption);
    HBRUSH field = CreateSolidBrush(kFrame);
    HWND hovered = nullptr;
    UINT dpi = 96;

    ~NameDialog() {
        if (background) DeleteObject(background);
        if (field) DeleteObject(field);
    }
    int D(int dip_value) const { return MulDiv(dip_value, static_cast<int>(dpi), 96); }

    static LRESULT CALLBACK ButtonProc(HWND window, UINT message, WPARAM wparam,
                                      LPARAM lparam, UINT_PTR, DWORD_PTR context) {
        auto* state = reinterpret_cast<NameDialog*>(context);
        if (message == BM_SETSTYLE) return 0; // Keep native dialog focus changes from replacing owner drawing.
        if (message == WM_MOUSEMOVE) {
            if (state->hovered != window) {
                const HWND previous = state->hovered;
                state->hovered = window;
                if (previous && IsWindow(previous)) InvalidateRect(previous, nullptr, FALSE);
                InvalidateRect(window, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
        } else if (message == WM_MOUSELEAVE && state->hovered == window) {
            state->hovered = nullptr;
            InvalidateRect(window, nullptr, FALSE);
        } else if (message == WM_NCDESTROY) {
            if (state->hovered == window) state->hovered = nullptr;
            RemoveWindowSubclass(window, ButtonProc, 1);
        }
        return DefSubclassProc(window, message, wparam, lparam);
    }
};

INT_PTR CALLBACK NameProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<NameDialog*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<NameDialog*>(lparam);
        SetWindowLongPtrW(window, DWLP_USER, lparam);
        state->dpi = GetDpiForWindow(window);
        if (!state->dpi) state->dpi = 96;
        const BOOL dark = TRUE;
        if (FAILED(DwmSetWindowAttribute(window, 20, &dark, sizeof(dark))))
            DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
        const HWND edit = GetDlgItem(window, 500);
        SetWindowTheme(edit, L"", L"");
        SendMessageW(edit, EM_SETLIMITTEXT, state->color_prompt ? 7 : 40, 0);
        SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(state->D(2), state->D(2)));
        SetWindowTextW(edit, state->initial.c_str());
        SendMessageW(edit, EM_SETSEL, 0, -1);
        for (const int id : {IDOK, IDCANCEL})
            SetWindowSubclass(GetDlgItem(window, id), NameDialog::ButtonProc, 1,
                              reinterpret_cast<DWORD_PTR>(state));
        SendMessageW(window, DM_SETDEFID, IDOK, 0);
        SetFocus(edit);
        return FALSE;
    }
    if (!state) return FALSE;
    if (message == WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(state->background);
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT) {
        const HDC dc = reinterpret_cast<HDC>(wparam);
        const HWND control = reinterpret_cast<HWND>(lparam);
        const bool edit = control == GetDlgItem(window, 500);
        SetTextColor(dc, edit || GetDlgCtrlID(control) == 501 ? NativeColor(state->palette.ink) : NativeColor(state->palette.muted));
        SetBkColor(dc, edit ? NativeColor(state->palette.frame) : NativeColor(state->palette.caption));
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(edit ? state->field : state->background);
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        const HDC dc = BeginPaint(window, &paint);
        RECT client{}; GetClientRect(window, &client);
        Fill(dc, client, NativeColor(state->palette.caption));
        RECT field{}; GetWindowRect(GetDlgItem(window, 500), &field);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&field), 2);
        InflateRect(&field, state->D(8), state->D(6));
        Rounded(dc, field, NativeColor(state->palette.frame), NativeColor(state->palette.frame), state->D(8));
        if (GetFocus() == GetDlgItem(window, 500))
            FocusOutline(dc, field, state->D(8), state->D(1),NativeColor(state->palette.accent));
        EndPaint(window, &paint);
        return TRUE;
    }
    if (message == WM_DRAWITEM) {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (!draw || draw->CtlType != ODT_BUTTON) return FALSE;
        const bool primary = draw->CtlID == IDOK;
        const bool pressed = (draw->itemState & ODS_SELECTED) != 0;
        const bool hover = state->hovered == draw->hwndItem;
        const bool focused = (draw->itemState & ODS_FOCUS) != 0;
        Fill(draw->hDC, draw->rcItem, NativeColor(state->palette.caption));
        RECT shape = draw->rcItem;
        InflateRect(&shape, -state->D(2), -state->D(2));
        const auto mix = [](COLORREF base, COLORREF tint, unsigned percent) {
            const auto rest = 100u-percent;
            return RGB((GetRValue(base)*rest+GetRValue(tint)*percent)/100,
                (GetGValue(base)*rest+GetGValue(tint)*percent)/100,
                (GetBValue(base)*rest+GetBValue(tint)*percent)/100);
        };
        const COLORREF accent = NativeColor(state->palette.accent);
        const COLORREF background = primary ?
            (pressed ? mix(accent,NativeColor(state->palette.caption),12) :
             hover ? mix(accent,NativeColor(state->palette.ink),12) : accent) :
            pressed ? mix(NativeColor(state->palette.frame),NativeColor(state->palette.ink),12) :
            hover ? NativeColor(state->palette.hover) : NativeColor(state->palette.frame);
        const std::uint32_t background_rgb = (static_cast<std::uint32_t>(GetRValue(background))<<16) |
            (static_cast<std::uint32_t>(GetGValue(background))<<8) | GetBValue(background);
        const COLORREF border = focused ? (primary ? NativeColor(state->palette.ink) : NativeColor(state->palette.accent)) : background;
        Rounded(draw->hDC, shape, background, border, state->D(8));
        const HFONT font = reinterpret_cast<HFONT>(SendMessageW(draw->hwndItem, WM_GETFONT, 0, 0));
        Text(draw->hDC, primary ? state->accept : L"Cancel", shape, font, primary ? NativeColor(ContrastInk(background_rgb)) : NativeColor(state->palette.ink),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        if (LOWORD(wparam) == 500 &&
            (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == EN_KILLFOCUS)) {
            InvalidateRect(window, nullptr, FALSE);
            return TRUE;
        }
        if (LOWORD(wparam) == IDOK) {
            state->value = ControlText(GetDlgItem(window, 500));
            const bool valid=state->color_prompt ? ParseHexColor(Utf8(state->value)).has_value() : IsValidProfileName(Utf8(state->value));
            if(!valid){
                SetWindowTextW(GetDlgItem(window,502),state->color_prompt ? L"Enter a six-digit color, for example #A8C7FA." :
                    L"Use 1-40 letters, numbers, spaces, underscores or hyphens. Avoid leading/trailing spaces and reserved names.");
                SetFocus(GetDlgItem(window,500));
                SendMessageW(GetDlgItem(window,500),EM_SETSEL,0,-1);
                return TRUE;
            }
            if(!state->color_prompt && std::any_of(state->existing_names.begin(),state->existing_names.end(),
                [&](const auto& name){return _wcsicmp(name.c_str(),state->value.c_str())==0;})){
                SetWindowTextW(GetDlgItem(window,502),L"That profile already exists. Choose another name.");
                SetFocus(GetDlgItem(window,500));SendMessageW(GetDlgItem(window,500),EM_SETSEL,0,-1);return TRUE;
            }
            EndDialog(window, IDOK);
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL) { EndDialog(window, IDCANCEL); return TRUE; }
    }
    if (message == WM_CLOSE) { EndDialog(window, IDCANCEL); return TRUE; }
    return FALSE;
}

std::optional<std::wstring> AskName(HWND owner, HINSTANCE instance, const Palette* palette=nullptr,
                                  const wchar_t* title=L"Create a WebCom profile",
                                  const wchar_t* label=L"&Profile name",
                                  const wchar_t* help=L"Use up to 40 letters, numbers, spaces, underscores or hyphens.",
                                  const wchar_t* initial=L"Work", const wchar_t* accept=L"Create", bool color_prompt=false,
                                  const std::vector<SavedProfile>* existing_profiles=nullptr) {
    // Dialog units and Segoe UI keep keyboard navigation, native accessibility
    // labels and Windows DPI scaling; only surfaces and buttons are custom drawn.
    std::vector<WORD> words;
    auto word = [&](WORD v) { words.push_back(v); };
    auto dword = [&](DWORD v) { word(static_cast<WORD>(v)); word(static_cast<WORD>(v >> 16)); };
    auto string = [&](const wchar_t* v) { while (*v) word(static_cast<WORD>(*v++)); word(0); };
    auto align = [&] { if (words.size() & 1) word(0); };
    dword(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN |
          DS_MODALFRAME | DS_SETFONT | DS_CENTER);
    dword(0); word(5); word(0); word(0); word(286); word(124);
    word(0); word(0); string(title); word(10); string(L"Segoe UI");
    auto item = [&](DWORD style, WORD x, WORD y, WORD w, WORD h, WORD id,
                    WORD type, const wchar_t* label) {
        align(); dword(style | WS_CHILD | WS_VISIBLE); dword(0);
        word(x); word(y); word(w); word(h); word(id);
        word(0xffff); word(type); string(label); word(0);
    };
    item(SS_LEFT, 16, 14, 254, 14, 501, 0x82, label);
    item(WS_TABSTOP | ES_AUTOHSCROLL, 20, 37, 246, 15, 500, 0x81, L"");
    item(SS_LEFT, 16, 63, 254, 24, 502, 0x82, help);
    item(WS_TABSTOP | BS_OWNERDRAW, 150, 94, 60, 22, IDOK, 0x80, accept);
    item(WS_TABSTOP | BS_OWNERDRAW, 214, 94, 60, 22, IDCANCEL, 0x80, L"Cancel");
    NameDialog result;
    result.initial=initial;result.accept=accept;result.color_prompt=color_prompt;
    if(existing_profiles)for(const auto& profile:*existing_profiles)result.existing_names.push_back(Wide(profile.name));
    if(palette)result.palette=*palette;
    DeleteObject(result.background);DeleteObject(result.field);
    result.background=CreateSolidBrush(NativeColor(result.palette.caption));
    result.field=CreateSolidBrush(NativeColor(result.palette.frame));
    if (!result.background || !result.field) return std::nullopt;
    const auto id = DialogBoxIndirectParamW(instance,
        reinterpret_cast<const DLGTEMPLATE*>(words.data()), owner, NameProc,
        reinterpret_cast<LPARAM>(&result));
    if (id != IDOK) return std::nullopt;
    return result.value;
}

} // namespace

#include "profile_host_decl.inl"

class BrowserApp::Impl : public std::enable_shared_from_this<BrowserApp::Impl> {
public:
    Impl(HINSTANCE instance,const std::shared_ptr<Session>& session,std::string profile)
        : instance_(instance),session_(session),root_(session->root),state_(session->state),profile_name_(std::move(profile)) {
        appearance_=LoadAppearance(root_,profile_name_);
        ApplyAppearance();
        LoadHistory();
        LoadDownloads();
        site_permissions_ = LoadSitePermissions(root_, profile_name_);
    }
    ~Impl();
    bool Create(int show_command, const std::wstring& initial_address, bool start_private);
    bool HandleKeyMessage(const MSG& message);
private:
    friend class BrowserApp::Session;
    struct CookieSummary {
        std::wstring name;
        std::wstring domain;
        std::wstring path;
        bool secure = false;
        bool http_only = false;
    };
    struct Tab {
        std::uint64_t id = 0;
        std::uint64_t generation = 0;
        UINT64 navigation_id = 0;
        std::wstring url = L"webcom:newtab";
        std::wstring title = L"New tab";
        std::wstring error;
        std::wstring group;
        bool private_mode = false;
        bool creating = false;
        bool restore_pending = false;
        bool failed = false;
        bool muted = false;
        bool controller_visible = false;
        bool visibility_known = false;
        bool bounds_known = false;
        RECT controller_bounds{};
        std::unique_ptr<Gdiplus::Bitmap> favicon;
        std::wstring favicon_uri;
        std::uint64_t favicon_request = 0;
        std::vector<CookieSummary> site_cookies;
        unsigned site_cookie_count = 0;
        bool cookies_known = false;
        bool cookies_loading = false;
        std::optional<COREWEBVIEW2_WEB_ERROR_STATUS> certificate_error;
        TabActivity activity;
        ComPtr<ICoreWebView2Controller> controller;
        ComPtr<ICoreWebView2> web;
        ComPtr<ICoreWebView2NewWindowRequestedEventArgs> popup_args;
        ComPtr<ICoreWebView2Deferral> popup_deferral;
        std::vector<ComPtr<ICoreWebView2DownloadOperation>> downloads;
        EventRegistrationToken navigation{}, completed{}, source{}, title_changed{}, history{}, popup{},
            close_requested{}, process_failed{}, accelerator{}, audio{}, muted_changed{}, download{}, fullscreen{}, favicon_changed{}, permission{};
    };
    struct ClosedTab { std::wstring url; std::wstring title; bool pinned; bool awake; };
    using HistoryEntry = HistoryRecord;
    struct DownloadEntry {
        std::uint64_t id = 0;
        std::wstring uri;
        std::wstring path;
        std::uint64_t started_ms = 0;
        std::int64_t received = 0;
        std::int64_t total = -1;
        COREWEBVIEW2_DOWNLOAD_STATE state = COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;
        COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON interrupt_reason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE;
        bool paused = false;
        bool event_driven = false;
        std::uint64_t last_persist_tick = 0;
        EventRegistrationToken bytes_changed{}, state_changed{};
        ComPtr<ICoreWebView2DownloadOperation> operation;
    };
    struct NativeAction { RECT rect{}; UINT command = 0; };
    struct TabReflowOrigin { std::uint64_t id = 0; RECT rect{}; };
    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr, tab_strip_ = nullptr, address_ = nullptr, home_search_ = nullptr, native_filter_ = nullptr, suggestion_panel_ = nullptr;
    HWND tooltips_ = nullptr, toast_ = nullptr, hovered_button_ = nullptr;
    std::vector<HWND> toolbar_;
    std::array<HWND, 4> home_buttons_{};
    std::vector<HWND> bookmark_buttons_;
    std::vector<Bookmark> bookmarks_;
    std::vector<std::wstring> bookmark_folders_;
    std::vector<SavedTabGroup> tab_groups_;
    std::vector<RECT> tab_rects_;
    std::size_t first_visible_tab_ = 0;
    int hot_tab_ = -1;
    int private_scroll_ = 0;
    int native_scroll_ = 0;
    int native_page_extent_ = 0;
    UINT hot_native_command_ = 0;
    bool hot_close_ = false, menu_open_ = false, main_menu_active_ = false;
    std::uint64_t dragged_tab_id_ = 0, pressed_close_tab_id_ = 0;
    bool tab_dragging_ = false;
    POINT tab_drag_start_{};
    int drag_target_index_ = -1;
    int bookmark_drag_source_ = -1, bookmark_drag_target_ = -1;
    bool bookmark_dragging_ = false;
    POINT bookmark_drag_start_{};
    HFONT font_ = nullptr, small_font_ = nullptr, logo_font_ = nullptr, heading_font_ = nullptr;
    HBRUSH edit_brush_ = nullptr, idle_edit_brush_ = nullptr, search_brush_ = nullptr, toast_brush_ = nullptr;
    ULONG_PTR graphics_token_ = 0;
    ComPtr<IStream> google_stream_;
    std::unique_ptr<Gdiplus::Bitmap> google_logo_;
    NativeChromeState native_chrome_;
    UINT dpi_ = 96;
    std::weak_ptr<Session> session_;
    const std::filesystem::path& root_;
    AppState& state_;
    const std::string profile_name_;
    Appearance appearance_;
    Palette palette_{};
    bool show_bookmarks_ = true;
    COLORREF kCaption=0,kFrame=0,kSurface=0,kHover=0,kInk=0,kMuted=0,kAccent=0,kSearch=0,kBorder=0;
    COLORREF profile_color_=0,profile_ink_=0;
    std::uint64_t environment_generation_ = 0;
    std::vector<std::unique_ptr<Tab>> tabs_;
    std::deque<ClosedTab> closed_tabs_;
    std::vector<HistoryEntry> history_entries_;
    std::vector<std::unique_ptr<DownloadEntry>> download_entries_;
    std::vector<SitePermissionRule> site_permissions_;
    std::vector<NativeAction> native_actions_;
    std::vector<OmniboxSuggestion> omnibox_suggestions_;
    int omnibox_selected_ = -1, omnibox_hot_ = -1;
    std::uint64_t omnibox_hover_start_ = 0;
    int omnibox_hover_from_ = -1, omnibox_hover_to_ = -1;
    InternalPage native_filter_page_ = InternalPage::None;
    std::size_t active_ = 0;
    std::uint64_t next_id_ = 1;
    std::uint64_t next_download_id_ = 1;
    ComPtr<ICoreWebView2Environment> environment_;
    bool starting_environment_ = false, environment_failed_ = false, closing_ = false;
    bool dirty_ = false, fullscreen_ = false, queued_notice_ = false;
    bool delete_profile_pending_ = false;
    bool address_editing_ = false, changing_address_ = false;
    bool ui_frame_pending_ = false;
    std::uint64_t queued_address_tab_ = 0;
    WINDOWPLACEMENT placement_{};
    DWORD saved_style_ = 0;
    std::wstring notice_text_, status_note_;
    std::uint64_t note_until_ = 0;
    std::uint64_t tab_transition_until_ = 0;
    std::uint64_t hover_animation_start_ = 0;
    int hover_animation_from_ = -1, hover_animation_to_ = -1;
    std::uint64_t tab_reflow_start_ = 0;
    std::vector<TabReflowOrigin> tab_reflow_from_;
    std::uint64_t menu_tab_id_ = 0;

    int D(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }
    Tab* Active() { return active_ < tabs_.size() ? tabs_[active_].get() : nullptr; }
    Tab* Find(std::uint64_t id) {
        for (auto& t : tabs_) if (t->id == id) return t.get();
        return nullptr;
    }
    bool IsActive(const Tab& t) const { return active_ < tabs_.size() && tabs_[active_]->id == t.id; }
    int ContentTop() const { return fullscreen_ ? 0 : D(show_bookmarks_ ? 121 : 84); }
    RECT ContentBounds() const;
    void Fonts();
    void Controls();
    HWND Button(UINT id, const wchar_t* label);
    void Layout();
    void Paint();
    void DrawItem(const DRAWITEMSTRUCT& draw);
    void PaintTabs();
    bool ScrollPrivate(int delta);
    int HitTab(POINT point) const;
    void RebuildBookmarks();
    void BookmarkCurrent();
    void BookmarksMenu(POINT anchor, int context = -1);
    void ImportBookmarksFromFile();
    void ExportBookmarksToFile();
    void MoveBookmarkToFolder(std::size_t index, std::wstring folder);
    void RenameBookmarkFolderTree(std::wstring folder);
    void MoveBookmark(std::size_t source, std::size_t target);
    void OpenTabsMenu();
    void PreferencesMenu(UINT section);
    void ConfigureTheme(Tab& tab);
    void ApplyAppearance();
    bool CommitAppearance(const Appearance& next);
    void AppearanceOptions(UINT section);
    void RequestFavicon(Tab& tab);
    void UpdateUI(bool update_address = false);
    void QueueUI(bool update_address = false);
    void SyncTabs();
    void RestoreProfile();
    void CaptureProfile();
    bool Save(bool show_error);
    void Dirty();
    void Note(const std::wstring& text);
    void Notice(const std::wstring& text);
    void FocusOmnibox();
    void UpdateOmniboxSuggestions();
    void HideOmniboxSuggestions();
    void ApplyOmniboxSuggestion(std::size_t index);
    void Navigate(std::wstring input);
    std::uint64_t Add(std::wstring url = L"webcom:newtab", bool select = true, bool private_mode = false);
    void Select(std::size_t index);
    void MoveTab(std::uint64_t id, int direction);
    void MoveTabTo(std::uint64_t id, std::size_t target);
    void AssignTabGroup(std::uint64_t id, std::wstring group);
    std::vector<std::wstring> TabGroups() const;
    SavedTabGroup* FindTabGroup(std::wstring_view group);
    const SavedTabGroup* FindTabGroup(std::wstring_view group) const;
    SavedTabGroup& EnsureTabGroup(std::wstring_view group);
    bool IsGroupCollapsed(std::wstring_view group) const;
    void ToggleTabGroupState(std::wstring_view group);
    void RenameGroup(std::wstring_view group);
    void CloseGroup(std::wstring_view group);
    void SetGroupColor(std::wstring_view group, std::uint8_t color);
    std::vector<std::size_t> VisibleTabIndices() const;
    void CancelTabDrag();
    void Close(std::uint64_t id);
    void Release(Tab& tab);
    void Reopen();
    void BeginEnvironment();
    void Ensure(Tab& tab);
    void Attach(Tab& tab);
    void SetVisible(Tab& tab, bool visible);
    void ConfigurePasswords(Tab& tab);
    void Tick();
    void Suspend(Tab& tab, bool manual);
    bool HasDownload(Tab& tab);
    void LowMemoryTarget(Tab& tab, bool low);
    void LoadHistory();
    void SaveHistory() const;
    void RecordHistory(const Tab& tab);
    void HistoryMenu();
    void LoadDownloads();
    void SaveDownloads() const;
    void TrackDownload(ICoreWebView2DownloadOperation* operation);
    void RefreshDownload(DownloadEntry& entry);
    void OnDownloadChanged(std::uint64_t id, bool state_event);
    void ToggleDownloadPause(std::size_t index);
    void CancelDownload(std::size_t index);
    void RetryDownload(std::size_t index);
    void ClearDownloads();
    void DownloadQuickMenu();
    void SiteInfoMenu();
    void RefreshSiteCookies(Tab& tab);
    void SiteCookiesMenu();
    void ClearSiteCookies();
    void SaveSitePermissionsNow();
    void SyncRuntimeSitePermission(Tab& tab, std::string_view origin, SitePermissionKind kind, SitePermissionDecision decision);
    void OpenInternalPage(std::wstring url);
    bool ScrollInternal(int delta);
    void PaintInternalPage(HDC dc, const RECT& content, Tab& tab);
    bool HitNativeAction(POINT point);
    RECT DisplayTabRect(std::size_t index) const;
    void MainMenu();
    void ProfileMenu();
    void TabMenu(POINT point);
    void CreateProfile();
    void SwitchProfile(std::size_t index);
    void DeleteCurrentProfile();
    void DeleteCurrentProfileNow(Tab& source);
    void ReportMemory();
    void ClearBrowsingData();
    void ToggleFullscreen();
    void Dispatch(UINT command, LPARAM extra = 0);
    UINT Shortcut(UINT key, bool ctrl, bool shift, bool alt);
    LRESULT Message(UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK TabProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK EditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK SuggestionProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK ButtonProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
};

BrowserApp::Impl::~Impl() {
    closing_ = true;
    SaveDownloads();
    for (auto& t : tabs_) { Release(*t); t->favicon.reset(); }
    download_entries_.clear();
    environment_.Reset();
    if (window_ && IsWindow(window_)) DestroyWindow(window_);
    for (HFONT font : {font_, small_font_, logo_font_, heading_font_}) if (font) DeleteObject(font);
    if (edit_brush_) DeleteObject(edit_brush_);
    if (idle_edit_brush_) DeleteObject(idle_edit_brush_);
    if (search_brush_) DeleteObject(search_brush_);
    if (toast_brush_) DeleteObject(toast_brush_);
    google_logo_.reset(); google_stream_.Reset();
    if (graphics_token_) Gdiplus::GdiplusShutdown(graphics_token_);
}

bool BrowserApp::Impl::Create(int show_command, const std::wstring& initial_address, bool start_private) {
    Gdiplus::GdiplusStartupInput graphics_input;
    if (Gdiplus::GdiplusStartup(&graphics_token_, &graphics_input, nullptr) != Gdiplus::Ok) return false;
    if (HRSRC resource = FindResourceW(instance_, MAKEINTRESOURCEW(201), RT_RCDATA)) {
        HGLOBAL loaded = LoadResource(instance_, resource);
        const BYTE* data = static_cast<const BYTE*>(LockResource(loaded));
        const DWORD size = SizeofResource(instance_, resource);
        if (data && size) {
            google_stream_.Attach(SHCreateMemStream(data, size));
            if (google_stream_) {
                google_logo_ = std::make_unique<Gdiplus::Bitmap>(google_stream_.Get(), FALSE);
                if (google_logo_->GetLastStatus() != Gdiplus::Ok) google_logo_.reset();
            }
        }
    }
    INITCOMMONCONTROLSEX controls{}; controls.dwSize=sizeof(controls); controls.dwICC=ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = BrowserApp::WindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(101));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    window_ = CreateWindowExW(0, wc.lpszClassName, L"WebCom", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1240, 840, nullptr, nullptr, instance_, this);
    if (!window_) return false;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(window_, 20, &dark, sizeof(dark));
    const MARGINS margins{1,1,1,1};
    DwmExtendFrameIntoClientArea(window_, &margins);
    SetWindowPos(window_, nullptr, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    dpi_ = GetDpiForWindow(window_);
    Fonts();
    Controls();
    RestoreProfile();
    Layout();
    ShowWindow(window_, show_command == SW_SHOWNORMAL ? SW_SHOWMAXIMIZED : show_command);
    UpdateWindow(window_);
    SetTimer(window_, kTick, 10000, nullptr);
    if (start_private) {
        const auto private_id = Add(L"webcom:newtab", true, true);
        if (auto* tab = Find(private_id); tab && tab->private_mode && IsActive(*tab)) {
            if (!initial_address.empty()) Navigate(initial_address);
            else FocusOmnibox();
        }
    } else if (!initial_address.empty()) {
        Add();
        Navigate(initial_address);
    } else if (auto* tab = Active()) {
        Ensure(*tab);
        if (IsHome(tab->url)) FocusOmnibox();
    }
    UpdateUI(true);
    return true;
}

#include "native_ui.inl"
#include "appearance_ui.inl"

void BrowserApp::Impl::QueueUI(bool update_address) {
    if(closing_ || !window_)return;
    if(update_address)if(auto* tab=Active())queued_address_tab_=tab->id;
    if(!ui_frame_pending_){
        ui_frame_pending_=true;
        if(!SetTimer(window_,kUiFrame,16,nullptr)){
            ui_frame_pending_=false;
            UpdateUI(update_address && !address_editing_ && !menu_open_);
        }
    }
}

void BrowserApp::Impl::UpdateUI(bool update_address) {
    auto* tab = Active();
    if (!tab || !window_ || closing_) return;
    SyncTabs();
    if (update_address) {
        changing_address_ = true;
        const std::wstring address=IsHome(tab->url)?L"":tab->url;
        if(ControlText(address_)!=address)SetWindowTextW(address_,address.c_str());
        changing_address_ = false;
        address_editing_ = false;
    }
    BOOL back = FALSE, forward = FALSE;
    if (tab->web) {
        tab->web->get_CanGoBack(&back);
        tab->web->get_CanGoForward(&forward);
    }
    EnableWindow(GetDlgItem(window_, Back), back);
    EnableWindow(GetDlgItem(window_, Forward), forward);
    const std::wstring profile_label=tab->private_mode?L"Incognito - "+Wide(profile_name_)+L" profile":L"Profile: "+Wide(profile_name_);
    if(ControlText(GetDlgItem(window_,Profile))!=profile_label)SetWindowTextW(GetDlgItem(window_,Profile),profile_label.c_str());
    std::wstring title = tab->title + L" - WebCom - " + Wide(profile_name_);
    if (tab->private_mode) title += L" (Private)";
    if(ControlText(window_)!=title)SetWindowTextW(window_,title.c_str());
    for (auto h : toolbar_) InvalidateRect(h, nullptr, FALSE);
    Layout();
}

void BrowserApp::Impl::RestoreProfile() {
    private_scroll_ = 0;
    if (home_search_) SetWindowTextW(home_search_, L"");
    for (auto& t : tabs_) Release(*t);
    tabs_.clear();
    closed_tabs_.clear();
    tab_groups_.clear();
    auto it = std::find_if(state_.profiles.begin(), state_.profiles.end(),
        [&](const SavedProfile& p) { return p.name == profile_name_; });
    if (it == state_.profiles.end()) {
        state_.profiles.push_back(SavedProfile{profile_name_, {}, 0});
        it = std::prev(state_.profiles.end());
    }
    tab_groups_=it->groups;
    for (const auto& saved : it->tabs) {
        auto tab = std::make_unique<Tab>();
        tab->id = next_id_++;
        tab->url = Wide(saved.url);
        if (!IsInternalPage(tab->url) && !IsWebAddress(tab->url)) tab->url = L"webcom:newtab";
        tab->title = Wide(saved.title);
        if (tab->title.empty()) tab->title = IsInternalPage(tab->url) ? InternalPageTitle(tab->url) : tab->url;
        tab->activity.pinned = saved.pinned;
        tab->activity.protected_from_sleep = saved.keep_awake;
        tab->group = Wide(saved.group);
        tab->activity.last_active_ms = GetTickCount64();
        tab->restore_pending = !IsInternalPage(tab->url);
        tabs_.push_back(std::move(tab));
    }
    for(const auto& tab:tabs_)if(!tab->group.empty())EnsureTabGroup(tab->group);
    if (tabs_.empty()) {
        auto tab = std::make_unique<Tab>();
        tab->id = next_id_++;
        tab->activity.last_active_ms = GetTickCount64();
        tabs_.push_back(std::move(tab));
    }
    active_ = std::min(it->active_tab, tabs_.size() - 1);
    if(!tabs_[active_]->group.empty())if(auto* group=FindTabGroup(tabs_[active_]->group))group->collapsed=false;
    tabs_[active_]->activity.active = true;
    bookmarks_ = LoadBookmarks(root_, profile_name_);
    RebuildBookmarks();
    SyncTabs();
}

void BrowserApp::Impl::CaptureProfile() {
    auto it = std::find_if(state_.profiles.begin(), state_.profiles.end(),
        [&](const SavedProfile& p) { return p.name == profile_name_; });
    if (it == state_.profiles.end()) return;
    it->tabs.clear();
    it->groups.clear();
    it->active_tab = 0;
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        const auto& tab = *tabs_[i];
        if (tab.private_mode) continue;
        if (i == active_) it->active_tab = it->tabs.size();
        std::string url = Utf8(tab.url);
        const auto resolved = ResolveAddress(url);
        if (!resolved.allowed || url.size() > 16384 || (!IsInternalPage(tab.url) && !IsWebAddress(tab.url)))
            url = "webcom:newtab";
        else url = resolved.url;
        const std::wstring title = SafeTitle(tab.title);
        std::string utf_title = Utf8(title);
        if (utf_title.empty()) utf_title = "New tab";
        it->tabs.push_back(SavedTab{url, utf_title, tab.activity.pinned, tab.activity.protected_from_sleep, Utf8(tab.group)});
    }
    if (it->tabs.empty()) it->tabs.push_back({"webcom:newtab", "New tab", false, false});
    std::set<std::string> used_groups;
    for(const auto& tab:it->tabs)if(!tab.group.empty())used_groups.insert(tab.group);
    for(const auto& group:tab_groups_)
        if(used_groups.find(group.name)!=used_groups.end())it->groups.push_back(group);
}

bool BrowserApp::Impl::Save(bool show_error) {
    if(auto session=session_.lock())return session->SaveAll(window_,show_error);
    return false;
}

void BrowserApp::Impl::Dirty() {
    dirty_ = true;
    if (window_ && !closing_) SetTimer(window_, kSave, 1200, nullptr);
}

void BrowserApp::Impl::Note(const std::wstring& text) {
    status_note_ = text;
    note_until_ = GetTickCount64() + 4500;
    if (window_ && toast_) {
        SetWindowTextW(toast_, text.c_str());
        SetTimer(window_, kToast, 4500, nullptr);
        Layout();
    }
}

void BrowserApp::Impl::Notice(const std::wstring& text) {
    // Show a native dialog after returning from the WebView2 callback. Nested
    // message loops inside its synchronous callbacks can stall the engine.
    if (closing_) return;
    notice_text_ = text;
    if (!queued_notice_) { queued_notice_ = true; PostMessageW(window_, kNotice, 0, 0); }
}

void BrowserApp::Impl::FocusOmnibox() {
    if (fullscreen_) ToggleFullscreen();
    SetFocus(address_);
    SendMessageW(address_, EM_SETSEL, 0, -1);
    address_editing_ = true;
    Layout();
    RECT chrome{}; GetClientRect(window_, &chrome); chrome.bottom=ContentTop();
    InvalidateRect(window_, &chrome, FALSE);
}

void BrowserApp::Impl::HideOmniboxSuggestions() {
    omnibox_suggestions_.clear();
    omnibox_selected_=-1; omnibox_hot_=-1; omnibox_hover_start_=0; omnibox_hover_from_=-1; omnibox_hover_to_=-1;
    if(suggestion_panel_ && IsWindowVisible(suggestion_panel_)) ShowWindow(suggestion_panel_,SW_HIDE);
}

void BrowserApp::Impl::UpdateOmniboxSuggestions() {
    if(!suggestion_panel_ || !address_ || GetFocus()!=address_ || changing_address_ || menu_open_) {
        HideOmniboxSuggestions(); return;
    }
    const auto query=ControlText(address_);
    if(query.empty()) { HideOmniboxSuggestions(); return; }
    std::vector<OmniboxCandidate> candidates;
    candidates.reserve(tabs_.size()+bookmarks_.size()+std::min<std::size_t>(history_entries_.size(),250));
    for(const auto& tab:tabs_) {
        if(IsHome(tab->url)) continue;
        candidates.push_back({OmniboxSuggestionKind::OpenTab,tab->title,tab->url,0,tab->id,IsActive(*tab)});
    }
    for(const auto& bookmark:bookmarks_)
        candidates.push_back({OmniboxSuggestionKind::Bookmark,Wide(bookmark.title),Wide(bookmark.url),0,0,false});
    const std::size_t history_count=std::min<std::size_t>(history_entries_.size(),250);
    for(std::size_t i=0;i<history_count;++i) {
        const auto& item=history_entries_[i];
        candidates.push_back({OmniboxSuggestionKind::History,item.title,item.url,
            static_cast<std::uint64_t>((history_count-i)*60000ull),0,false});
    }
    auto next=BuildOmniboxSuggestions(query,candidates,8);
    if(next.empty()) { HideOmniboxSuggestions(); return; }
    omnibox_suggestions_=std::move(next);omnibox_selected_=-1;omnibox_hot_=-1;
    const int width=std::max(D(320),static_cast<int>(native_chrome_.omnibox.right-native_chrome_.omnibox.left));
    const int height=D(10)+static_cast<int>(omnibox_suggestions_.size())*D(52);
    SetWindowPos(suggestion_panel_,HWND_TOP,native_chrome_.omnibox.left,native_chrome_.omnibox.bottom+D(4),width,height,
        SWP_NOACTIVATE|SWP_SHOWWINDOW|SWP_NOOWNERZORDER);
    InvalidateRect(suggestion_panel_,nullptr,FALSE);
}

void BrowserApp::Impl::ApplyOmniboxSuggestion(std::size_t index) {
    if(index>=omnibox_suggestions_.size()) return;
    const auto item=omnibox_suggestions_[index];
    HideOmniboxSuggestions();
    if(item.kind==OmniboxSuggestionKind::OpenTab && item.tab_id) {
        for(std::size_t i=0;i<tabs_.size();++i) if(tabs_[i]->id==item.tab_id) { Select(i); return; }
    }
    Navigate(item.kind==OmniboxSuggestionKind::Search?ControlText(address_):item.url);
}

std::uint64_t BrowserApp::Impl::Add(std::wstring url, bool select, bool private_mode) {
    if (tabs_.size() >= kMaxTabs) { Note(L"This profile already has 200 open tabs. Close a tab to continue."); return 0; }
    std::vector<TabReflowOrigin> reflow_origins;
    reflow_origins.reserve(tabs_.size());
    for(std::size_t i=0;i<tabs_.size();++i)reflow_origins.push_back({tabs_[i]->id,DisplayTabRect(i)});
    auto tab = std::make_unique<Tab>();
    tab->id = next_id_++;
    tab->url = std::move(url);
    tab->title = IsInternalPage(tab->url) ? InternalPageTitle(tab->url, private_mode) : tab->url.substr(0, 250);
    tab->private_mode = private_mode;
    tab->activity.last_active_ms = GetTickCount64();
    const auto id = tab->id;
    tabs_.push_back(std::move(tab));
    SyncTabs();
    if (select) Select(tabs_.size() - 1);
    else UpdateUI();
    Dirty();
    if(!reflow_origins.empty()){
        tab_reflow_from_=std::move(reflow_origins);
        tab_reflow_start_=GetTickCount64();
        SetTimer(window_,kAnimationFrame,16,nullptr);
        if(tab_strip_)InvalidateRect(tab_strip_,nullptr,FALSE);
    }
    return id;
}

void BrowserApp::Impl::Select(std::size_t index) {
    HideOmniboxSuggestions();
    if (index >= tabs_.size()) return;
    if(!tabs_[index]->group.empty())if(auto* group=FindTabGroup(tabs_[index]->group))
        if(group->collapsed){group->collapsed=false;Dirty();}
    Tab* old=Active();
    const bool changed=!old || old->id!=tabs_[index]->id;
    const auto old_id=old?old->id:0;
    if (changed) {
        private_scroll_=0;
        native_scroll_=0;
        hot_native_command_=0;
        if(home_search_ && GetWindowTextLengthW(home_search_))SetWindowTextW(home_search_,L"");
        if(old){old->activity.active=false;old->activity.last_active_ms=GetTickCount64();}
    }
    active_=index;
    auto& tab=*tabs_[active_];
    tab.activity.active=true;tab.activity.last_active_ms=GetTickCount64();
    if (tab.web) {
        if(tab.activity.suspended){
            ComPtr<ICoreWebView2_3> lifecycle;
            if(SUCCEEDED(tab.web.As(&lifecycle)))lifecycle->Resume();
        }
        tab.activity.suspended=false;
        LowMemoryTarget(tab,false);
    }
    // Show the prepared destination before hiding the old controller. No nested
    // message pump or whole-window erase is allowed during this handoff.
    Ensure(tab);
    SetVisible(tab,true);
    UpdateUI(true);
    if(changed)if(auto* previous=Find(old_id))SetVisible(*previous,false);
    tab_transition_until_=GetTickCount64()+140;
    SetTimer(window_,kAnimationFrame,16,nullptr);
    if(!tab.controller){
        const RECT content=ContentBounds();
        RedrawWindow(window_,&content,nullptr,RDW_INVALIDATE|RDW_NOERASE);
        if(IsHome(tab.url)) PostMessageW(window_,kRunAction,FocusAddress,0);
        else if(tab_strip_) SetFocus(tab_strip_);
    } else if(!menu_open_ && GetCapture()!=tab_strip_)tab.controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    Dirty();
}

void BrowserApp::Impl::MoveTab(std::uint64_t id,int direction) {
    const auto found=std::find_if(tabs_.begin(),tabs_.end(),[&](const auto& t){return t->id==id;});
    if(found==tabs_.end())return;
    const std::size_t index=static_cast<std::size_t>(found-tabs_.begin());
    if((direction<0 && index==0) || (direction>0 && index+1==tabs_.size()))return;
    const std::size_t next=direction<0?index-1:index+1;
    MoveTabTo(id,next);
}

void BrowserApp::Impl::MoveTabTo(std::uint64_t id,std::size_t target) {
    const auto found=std::find_if(tabs_.begin(),tabs_.end(),[&](const auto& t){return t->id==id;});
    if(found==tabs_.end() || target>=tabs_.size())return;
    const auto index=static_cast<std::size_t>(found-tabs_.begin());
    const bool moving_pinned=(*found)->activity.pinned;
    std::size_t pinned_others=0;for(const auto& item:tabs_)if(item->id!=id && item->activity.pinned)++pinned_others;
    target=moving_pinned?std::min(target,pinned_others):std::max(target,pinned_others);
    target=std::min(target,tabs_.size()-1);
    if(index==target)return;
    const auto active_id=Active()?Active()->id:0;
    auto moved=std::move(*found);
    tabs_.erase(found);
    tabs_.insert(tabs_.begin()+static_cast<std::ptrdiff_t>(target),std::move(moved));
    for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->id==active_id){active_=i;break;}
    hot_tab_=-1;hot_close_=false;
    Dirty();UpdateUI();
}

std::vector<std::wstring> BrowserApp::Impl::TabGroups() const {
    std::vector<std::wstring> groups;
    for(const auto& saved:tab_groups_) {
        const auto name=Wide(saved.name);
        if(!name.empty() && std::find(groups.begin(),groups.end(),name)==groups.end())groups.push_back(name);
    }
    for(const auto& tab:tabs_) if(!tab->group.empty() && std::find(groups.begin(),groups.end(),tab->group)==groups.end()) groups.push_back(tab->group);
    return groups;
}

SavedTabGroup* BrowserApp::Impl::FindTabGroup(std::wstring_view group) {
    const std::wstring wanted(group);
    for(auto& item:tab_groups_)if(_wcsicmp(Wide(item.name).c_str(),wanted.c_str())==0)return &item;
    return nullptr;
}

const SavedTabGroup* BrowserApp::Impl::FindTabGroup(std::wstring_view group) const {
    const std::wstring wanted(group);
    for(const auto& item:tab_groups_)if(_wcsicmp(Wide(item.name).c_str(),wanted.c_str())==0)return &item;
    return nullptr;
}

SavedTabGroup& BrowserApp::Impl::EnsureTabGroup(std::wstring_view group) {
    if(auto* existing=FindTabGroup(group))return *existing;
    const auto utf=Utf8(group);
    const auto hash=std::hash<std::string>{}(utf);
    tab_groups_.push_back({utf,static_cast<std::uint8_t>(hash%kProfileColors.size()),false});
    return tab_groups_.back();
}

bool BrowserApp::Impl::IsGroupCollapsed(std::wstring_view group) const {
    if(group.empty())return false;
    if(const auto* item=FindTabGroup(group))return item->collapsed;
    return false;
}

void BrowserApp::Impl::ToggleTabGroupState(std::wstring_view group) {
    if(group.empty())return;
    auto& item=EnsureTabGroup(group);
    const bool collapse=!item.collapsed;

    // Capture every tab's current visual origin. Hidden members of a collapsed
    // group inherit the representative's rectangle so expansion fans out from
    // one stable point instead of popping the restored tabs into existence.
    std::vector<TabReflowOrigin> reflow_origins;
    reflow_origins.reserve(tabs_.size());
    RECT representative{};
    for(std::size_t i=0;i<tabs_.size();++i)if(i<tab_rects_.size() && tabs_[i]->group==group && !IsRectEmpty(&tab_rects_[i])){representative=DisplayTabRect(i);break;}
    for(std::size_t i=0;i<tabs_.size();++i){
        RECT origin=DisplayTabRect(i);
        if(IsRectEmpty(&origin) && tabs_[i]->group==group && !IsRectEmpty(&representative))origin=representative;
        if(!IsRectEmpty(&origin))reflow_origins.push_back({tabs_[i]->id,origin});
    }

    if(collapse) {
        std::size_t first=tabs_.size();
        for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->group==group){first=i;break;}
        // Select while the group is still expanded. Select() intentionally expands a
        // collapsed group, so toggling metadata before this handoff would immediately
        // undo the requested collapse.
        if(first<tabs_.size() && first!=active_)Select(first);
    }
    item.collapsed=collapse;
    Dirty();UpdateUI();
    if(!reflow_origins.empty()){
        tab_reflow_from_=std::move(reflow_origins);tab_reflow_start_=GetTickCount64();
        SetTimer(window_,kAnimationFrame,16,nullptr);
        if(tab_strip_)InvalidateRect(tab_strip_,nullptr,FALSE);
    }
}

void BrowserApp::Impl::RenameGroup(std::wstring_view group) {
    if(group.empty())return;
    const auto current=std::wstring(group);
    const auto renamed=AskName(window_,instance_,&palette_,L"Rename tab group",L"&Group name",
        L"Use a short name that identifies these tabs.",current.c_str(),L"Rename");
    if(!renamed || renamed->empty() || *renamed==current)return;
    std::wstring next=*renamed;if(next.size()>40)next.resize(40);
    for(const auto& existing:TabGroups())
        if(_wcsicmp(existing.c_str(),next.c_str())==0 && _wcsicmp(existing.c_str(),current.c_str())!=0){
            Notice(L"A tab group with that name already exists.");return;
        }
    for(auto& tab:tabs_)if(tab->group==current)tab->group=next;
    auto& meta=EnsureTabGroup(current);meta.name=Utf8(next);
    Dirty();UpdateUI();
}

void BrowserApp::Impl::CloseGroup(std::wstring_view group) {
    if (group.empty()) return;
    const std::wstring wanted(group);
    std::vector<std::uint64_t> ids;
    for (const auto& tab : tabs_) if (_wcsicmp(tab->group.c_str(), wanted.c_str()) == 0) ids.push_back(tab->id);
    for (const auto id : ids) if (Find(id)) Close(id);
    const bool remains=std::any_of(tabs_.begin(),tabs_.end(),[&](const auto& tab){return _wcsicmp(tab->group.c_str(),wanted.c_str())==0;});
    if(!remains)std::erase_if(tab_groups_, [&](const SavedTabGroup& item){ return _wcsicmp(Wide(item.name).c_str(), wanted.c_str()) == 0; });
    Dirty(); UpdateUI(true);
}

void BrowserApp::Impl::SetGroupColor(std::wstring_view group,std::uint8_t color) {
    if(group.empty() || color>=kProfileColors.size())return;
    auto& item=EnsureTabGroup(group);item.color=color;Dirty();UpdateUI();
}

std::vector<std::size_t> BrowserApp::Impl::VisibleTabIndices() const {
    std::vector<std::pair<std::wstring,bool>> model;model.reserve(tabs_.size());
    for(const auto& tab:tabs_)model.push_back({tab->group,IsGroupCollapsed(tab->group)});
    return MakeChromeVisibleTabIndices(model,std::wstring{});
}

void BrowserApp::Impl::AssignTabGroup(std::uint64_t id,std::wstring group) {
    auto* tab=Find(id);if(!tab)return;
    if(group.size()>40)group.resize(40);
    tab->group=std::move(group);
    if(!tab->group.empty()){
        auto& meta=EnsureTabGroup(tab->group);meta.collapsed=false;
        std::size_t last=tabs_.size();
        for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->id!=id && tabs_[i]->group==tab->group)last=i;
        if(last<tabs_.size()){
            const auto source=static_cast<std::size_t>(std::find_if(tabs_.begin(),tabs_.end(),[&](const auto& item){return item->id==id;})-tabs_.begin());
            const auto target=source<last?last:last+1;
            if(source!=target && target<tabs_.size())MoveTabTo(id,target);
        }
    }
    std::erase_if(tab_groups_,[&](const SavedTabGroup& item){
        return std::none_of(tabs_.begin(),tabs_.end(),[&](const auto& candidate){return _wcsicmp(candidate->group.c_str(),Wide(item.name).c_str())==0;});
    });
    Dirty();UpdateUI();
}

void BrowserApp::Impl::CancelTabDrag() {
    dragged_tab_id_=0;pressed_close_tab_id_=0;tab_dragging_=false;drag_target_index_=-1;
    if(tab_strip_ && GetCapture()==tab_strip_)ReleaseCapture();
    if(tab_strip_ && IsWindow(tab_strip_))InvalidateRect(tab_strip_,nullptr,FALSE);
}

void BrowserApp::Impl::Release(Tab& tab) {
    ++tab.generation;
    ++tab.favicon_request;
    if (tab.popup_args) tab.popup_args->put_Handled(TRUE);
    if (tab.popup_deferral) tab.popup_deferral->Complete();
    tab.popup_args.Reset();
    tab.popup_deferral.Reset();
    if (tab.web) {
        tab.web->remove_NavigationStarting(tab.navigation);
        tab.web->remove_NavigationCompleted(tab.completed);
        tab.web->remove_SourceChanged(tab.source);
        tab.web->remove_DocumentTitleChanged(tab.title_changed);
        tab.web->remove_HistoryChanged(tab.history);
        tab.web->remove_NewWindowRequested(tab.popup);
        tab.web->remove_WindowCloseRequested(tab.close_requested);
        tab.web->remove_ProcessFailed(tab.process_failed);
        tab.web->remove_ContainsFullScreenElementChanged(tab.fullscreen);
        tab.web->remove_PermissionRequested(tab.permission);
        ComPtr<ICoreWebView2_15> icons;
        if(SUCCEEDED(tab.web.As(&icons)))icons->remove_FaviconChanged(tab.favicon_changed);
        ComPtr<ICoreWebView2_8> media;
        if (SUCCEEDED(tab.web.As(&media))) {
            media->remove_IsDocumentPlayingAudioChanged(tab.audio);
            media->remove_IsMutedChanged(tab.muted_changed);
        }
        ComPtr<ICoreWebView2_4> download;
        if (SUCCEEDED(tab.web.As(&download))) download->remove_DownloadStarting(tab.download);
    }
    if (tab.controller) {
        tab.controller->remove_AcceleratorKeyPressed(tab.accelerator);
        tab.controller->Close();
    }
    tab.downloads.clear();
    tab.web.Reset();
    tab.controller.Reset();
    tab.creating = false;
    tab.activity.loading = false;
    tab.activity.suspended = false;
    tab.activity.suspend_pending = false;
    tab.activity.audible = false;
    tab.visibility_known = false; tab.bounds_known = false; tab.controller_visible = false;
}

bool BrowserApp::Impl::HasDownload(Tab& tab) {
    bool ongoing = false;
    std::erase_if(tab.downloads, [&](const ComPtr<ICoreWebView2DownloadOperation>& download) {
        COREWEBVIEW2_DOWNLOAD_STATE state = COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;
        if (FAILED(download->get_State(&state))) { ongoing = true; return false; }
        if (state == COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS) { ongoing = true; return false; }
        if (state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED) {
            COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON reason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE;
            if (SUCCEEDED(download->get_InterruptReason(&reason)) &&
                reason == COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_PAUSED) { ongoing = true; return false; }
        }
        return true;
    });
    return ongoing;
}

void BrowserApp::Impl::Close(std::uint64_t id) {
    if(dragged_tab_id_ || pressed_close_tab_id_)CancelTabDrag();
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto& t) { return t->id == id; });
    if (it == tabs_.end()) return;
    if (HasDownload(**it) && MessageBoxW(window_, L"A download is still running in this tab. Close it anyway?",
        L"Close tab", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
    // Modal loops can deliver a page's close request. Never reuse a Tab pointer
    // or vector iterator obtained before a native dialog.
    it = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto& t) { return t->id == id; });
    if (it == tabs_.end() || closing_) return;
    auto& tab = **it;
    std::vector<TabReflowOrigin> reflow_origins;
    reflow_origins.reserve(tabs_.size()>0?tabs_.size()-1:0);
    for(std::size_t i=0;i<tabs_.size();++i)
        if(tabs_[i]->id!=id)reflow_origins.push_back({tabs_[i]->id,DisplayTabRect(i)});
    const bool closed_active = IsActive(tab);
    if (closed_active && home_search_) SetWindowTextW(home_search_, L"");
    if (!tab.private_mode) {
        closed_tabs_.push_front({tab.url, tab.title, tab.activity.pinned, tab.activity.protected_from_sleep});
        if (closed_tabs_.size() > 20) closed_tabs_.pop_back();
    }
    const auto index = static_cast<std::size_t>(std::distance(tabs_.begin(), it));
    Release(tab);
    tabs_.erase(it);
    if (tabs_.empty()) { active_ = 0; Add(); }
    else {
        if (active_ > index) --active_;
        active_ = std::min(active_, tabs_.size() - 1);
        if (closed_active) {
            SetWindowTextW(home_search_, L"");
            Select(active_);
        }
    }
    Dirty();
    UpdateUI(closed_active);
    if(!reflow_origins.empty()){
        tab_reflow_from_=std::move(reflow_origins);
        tab_reflow_start_=GetTickCount64();
        SetTimer(window_,kAnimationFrame,16,nullptr);
        if(tab_strip_)InvalidateRect(tab_strip_,nullptr,FALSE);
    }
}

void BrowserApp::Impl::Reopen() {
    if (closed_tabs_.empty()) { Note(L"No recently closed normal tabs in this session."); return; }
    if (tabs_.size() >= kMaxTabs) return;
    const auto saved = closed_tabs_.front();
    closed_tabs_.pop_front();
    const auto id = Add(saved.url);
    if (auto* tab = Find(id)) {
        tab->title = saved.title;
        tab->activity.pinned = saved.pinned;
        tab->activity.protected_from_sleep = saved.awake;
    }
    UpdateUI();
}

void BrowserApp::Impl::Navigate(std::wstring input) {
    HideOmniboxSuggestions();
    auto* tab = Active();
    if (!tab) return;
    const auto result = ResolveAddress(Utf8(input));
    if (!result.allowed) { Notice(Wide(result.error)); return; }
    if (result.url.rfind("webcom:", 0) == 0 && tab->web && HasDownload(*tab)) {
        const auto id = tab->id;
        if (MessageBoxW(window_, L"A download is running in this tab. Opening an internal WebCom page will close its browser view. Continue?",
            L"Open WebCom page", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
        tab = Find(id);
        if (!tab || !IsActive(*tab) || closing_) return;
    }
    tab->error.clear();
    tab->failed = false;
    tab->url = Wide(result.url);
    tab->activity.last_active_ms = GetTickCount64();
    tab->activity.suspended = false;
    if (IsInternalPage(tab->url)) {
        Release(*tab);
        tab->favicon.reset();
        tab->favicon_uri.clear();
        tab->title = InternalPageTitle(tab->url, tab->private_mode);
        native_scroll_ = 0;
        if (IsHome(tab->url)) SetWindowTextW(home_search_, L"");
        UpdateUI(true);
        if (IsHome(tab->url)) FocusOmnibox();
        else if (tab_strip_) SetFocus(tab_strip_);
    } else if (tab->web) {
        ComPtr<ICoreWebView2_3> lifecycle;
        if (SUCCEEDED(tab->web.As(&lifecycle))) lifecycle->Resume();
        const HRESULT hr = tab->web->Navigate(tab->url.c_str());
        if (FAILED(hr)) Notice(L"This address could not be opened. " + HResultText(hr));
        else if (tab->controller) tab->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        UpdateUI(true);
    } else {
        tab->title = tab->url.substr(0, 250);
        Ensure(*tab);
        UpdateUI(true);
    }
    Dirty();
}

void BrowserApp::Impl::SetVisible(Tab& tab,bool visible) {
    if(!tab.controller)return;
    const bool show=visible && !IsIconic(window_);
    if(show){
        const RECT bounds=ContentBounds();
        if(!tab.bounds_known || !EqualRect(&tab.controller_bounds,&bounds)){
            if(SUCCEEDED(tab.controller->put_Bounds(bounds))){tab.controller_bounds=bounds;tab.bounds_known=true;}
        }
    }
    if(!tab.visibility_known || tab.controller_visible!=show){
        if(SUCCEEDED(tab.controller->put_IsVisible(show?TRUE:FALSE))){tab.controller_visible=show;tab.visibility_known=true;}
        if(show)LowMemoryTarget(tab,false);
    }
    if(!visible && state_.memory_mode==MemoryMode::Saver && !tab.activity.audible &&
        !tab.activity.pinned && !tab.activity.protected_from_sleep)LowMemoryTarget(tab,true);
}

void BrowserApp::Impl::BeginEnvironment() {
    if(environment_ || starting_environment_ || closing_)return;
    if(auto session=session_.lock())session->RequestEnvironment(shared_from_this());
}

void BrowserApp::Impl::Ensure(Tab& tab) {
    if (closing_ || tab.web || tab.creating || (IsInternalPage(tab.url) && !tab.popup_args)) return;
    if (!environment_) {
        if (!environment_failed_) BeginEnvironment();
        return;
    }
    ComPtr<ICoreWebView2Environment10> profiles;
    ComPtr<ICoreWebView2ControllerOptions> options;
    HRESULT hr = environment_.As(&profiles);
    if (SUCCEEDED(hr)) hr = profiles->CreateCoreWebView2ControllerOptions(&options);
    if (SUCCEEDED(hr)) hr = options->put_ProfileName(Wide(profile_name_).c_str());
    if (SUCCEEDED(hr)) hr = options->put_IsInPrivateModeEnabled(tab.private_mode ? TRUE : FALSE);
    // Set the backing color before creation, avoiding a white first frame.
    if (SUCCEEDED(hr)) {
        ComPtr<ICoreWebView2ControllerOptions3> appearance;
        if(SUCCEEDED(options.As(&appearance)))appearance->put_DefaultBackgroundColor({255,GetRValue(kFrame),GetGValue(kFrame),GetBValue(kFrame)});
    }
    if (FAILED(hr)) {
        tab.failed = true;
        tab.error = L"The installed WebView2 Runtime does not support this profile. Update the runtime and Reload.\n" + HResultText(hr);
        if (tab.popup_deferral) tab.popup_deferral->Complete();
        tab.popup_args.Reset(); tab.popup_deferral.Reset();
        UpdateUI();
        return;
    }
    tab.creating = true;
    tab.failed = false;
    const auto id = tab.id;
    const auto generation = tab.generation;
    const auto weak = weak_from_this();
    auto callback = MakeCallback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [weak, id, generation](HRESULT error, ICoreWebView2Controller* controller) -> HRESULT {
            auto app = weak.lock();
            auto* current = app ? app->Find(id) : nullptr;
            if (!app || app->closing_ || !current || current->generation != generation) {
                if (controller) controller->Close();
                return S_OK;
            }
            current->creating = false;
            if (FAILED(error) || !controller) {
                current->failed = true;
                current->error = L"This tab could not start. Press Reload to try again.\n" + HResultText(error);
                if (current->popup_deferral) current->popup_deferral->Complete();
                current->popup_args.Reset(); current->popup_deferral.Reset();
                app->UpdateUI();
                return S_OK;
            }
            current->controller = controller;
            const HRESULT core_hr = controller->get_CoreWebView2(&current->web);
            if (FAILED(core_hr) || !current->web) {
                app->Release(*current);
                current->failed = true;
                current->error = L"The browser engine did not return a page. Press Reload.";
                app->UpdateUI();
                return S_OK;
            }
            app->Attach(*current);
            app->SetVisible(*current, app->IsActive(*current));
            current->restore_pending = false;
            if (app->delete_profile_pending_ && !current->private_mode) {
                app->DeleteCurrentProfileNow(*current);
                return S_OK;
            }
            if (current->popup_args) {
                current->popup_args->put_NewWindow(current->web.Get());
                current->popup_args->put_Handled(TRUE);
                current->popup_deferral->Complete();
                current->popup_args.Reset(); current->popup_deferral.Reset();
            } else {
                const HRESULT navigation = current->web->Navigate(current->url.c_str());
                if (FAILED(navigation)) app->Note(L"Navigation failed. Check the address and press Reload.");
            }
            if (app->IsActive(*current) && !app->address_editing_ && !app->menu_open_ && GetCapture()!=app->tab_strip_)
                current->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            app->UpdateUI(app->IsActive(*current) && !app->address_editing_ && !app->menu_open_);
            return S_OK;
        });
    hr = profiles->CreateCoreWebView2ControllerWithOptions(window_, options.Get(), callback.Get());
    if (FAILED(hr)) {
        tab.creating = false;
        tab.failed = true;
        tab.error = L"This tab could not be created. Press Reload.\n" + HResultText(hr);
        if (tab.popup_deferral) tab.popup_deferral->Complete();
        tab.popup_args.Reset(); tab.popup_deferral.Reset();
        UpdateUI();
    }
}

void BrowserApp::Impl::ConfigurePasswords(Tab& tab) {
    if (!tab.web) return;
    ComPtr<ICoreWebView2Settings> settings;
    ComPtr<ICoreWebView2Settings4> passwords;
    if (SUCCEEDED(tab.web->get_Settings(&settings)) && SUCCEEDED(settings.As(&passwords))) {
        passwords->put_IsPasswordAutosaveEnabled(state_.save_passwords && !tab.private_mode ? TRUE : FALSE);
        passwords->put_IsGeneralAutofillEnabled(tab.private_mode ? FALSE : TRUE);
    }
}

void BrowserApp::Impl::RequestFavicon(Tab& tab) {
    if(!tab.web || closing_)return;
    ComPtr<ICoreWebView2_15> icons;
    if(FAILED(tab.web.As(&icons)))return;
    CoString uri;
    if(FAILED(icons->get_FaviconUri(&uri.value)))return;
    tab.favicon_uri=uri.str();
    const auto request=++tab.favicon_request;
    if(tab.favicon_uri.empty()){tab.favicon.reset();QueueUI();return;}
    const auto weak=weak_from_this();
    const auto id=tab.id,generation=tab.generation;
    icons->GetFavicon(COREWEBVIEW2_FAVICON_IMAGE_FORMAT_PNG,
        MakeCallback<ICoreWebView2GetFaviconCompletedHandler>([weak,id,generation,request](HRESULT error,IStream* stream)->HRESULT{
            auto app=weak.lock();auto* current=app?app->Find(id):nullptr;
            if(!app || app->closing_ || !current || current->generation!=generation || current->favicon_request!=request)return S_OK;
            if(SUCCEEDED(error))current->favicon=DecodeTabFavicon(stream);
            app->QueueUI();return S_OK;
        }).Get());
}

void BrowserApp::Impl::Attach(Tab& tab) {
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(tab.web->get_Settings(&settings))) {
        settings->put_IsWebMessageEnabled(FALSE);
        settings->put_AreHostObjectsAllowed(FALSE);
        settings->put_AreDefaultContextMenusEnabled(TRUE);
        settings->put_AreDevToolsEnabled(TRUE);
        settings->put_IsZoomControlEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    }
    ConfigurePasswords(tab);
    ConfigureTheme(tab);
    const auto id = tab.id;
    const auto generation = tab.generation;
    const auto weak = weak_from_this();
    // Returning a strong owner only for the duration of an event avoids both
    // use-after-free and the controller -> callback -> app reference cycle.
    const auto get = [weak, id, generation]() -> std::pair<std::shared_ptr<Impl>, Tab*> {
        auto app = weak.lock();
        if (!app || app->closing_) return {};
        auto* current = app->Find(id);
        if (!current || current->generation != generation) return {};
        return {std::move(app), current};
    };
    tab.web->add_NavigationStarting(MakeCallback<ICoreWebView2NavigationStartingEventHandler>(
        [get](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
            auto [app, current] = get();
            if (!current) return S_OK;
            CoString uri;
            args->get_Uri(&uri.value);
            const auto address = uri.str();
            if (address != L"about:blank" && !IsWebAddress(address)) {
                args->put_Cancel(TRUE);
                app->Note(L"WebCom supports HTTP and HTTPS pages. This address was not opened.");
                return S_OK;
            }
            current->activity.loading = true;
            ++current->favicon_request;current->favicon_uri.clear();current->favicon.reset();
            current->cookies_known = false;
            current->cookies_loading = false;
            current->site_cookie_count = 0;
            current->site_cookies.clear();
            current->certificate_error.reset();
            args->get_NavigationId(&current->navigation_id);
            current->activity.suspended = false;
            current->failed = false;
            current->error.clear();
            app->QueueUI();
            return S_OK;
        }).Get(), &tab.navigation);
    tab.web->add_NavigationCompleted(MakeCallback<ICoreWebView2NavigationCompletedEventHandler>(
        [get](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
            auto [app, current] = get();
            if (!current) return S_OK;
            UINT64 completed_navigation = 0;
            args->get_NavigationId(&completed_navigation);
            if (completed_navigation != current->navigation_id) return S_OK;
            current->activity.loading = false;
            current->activity.last_active_ms = GetTickCount64();
            BOOL successful = FALSE;
            args->get_IsSuccess(&successful);
            if (!successful) {
                COREWEBVIEW2_WEB_ERROR_STATUS status{};
                args->get_WebErrorStatus(&status);
                if (IsCertificateErrorStatus(status)) current->certificate_error = status;
                if (status != COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED)
                    app->Note(IsCertificateErrorStatus(status) ? L"The site's certificate could not be verified. Check the site-info shield for details."
                                                            : L"The page did not finish loading. Check the address or connection, then Reload.");
            } else {
                current->certificate_error.reset();
                app->RefreshSiteCookies(*current);
            }
            app->RequestFavicon(*current);
            if(successful) app->RecordHistory(*current);
            app->QueueUI();
            app->Dirty();
            return S_OK;
        }).Get(), &tab.completed);
    tab.web->add_SourceChanged(MakeCallback<ICoreWebView2SourceChangedEventHandler>(
        [get](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
            auto [app, current] = get();
            if (!current) return S_OK;
            CoString source;
            if (SUCCEEDED(sender->get_Source(&source.value))) {
                const auto url = source.str();
                if (IsWebAddress(url) || url == L"about:blank") {
                    if (current->url != url) {
                        current->cookies_known = false;
                        current->cookies_loading = false;
                        current->site_cookie_count = 0;
                        current->site_cookies.clear();
                    }
                    current->url = url;
                }
            }
            app->QueueUI(app->IsActive(*current) && !app->address_editing_);
            app->Dirty();
            return S_OK;
        }).Get(), &tab.source);
    tab.web->add_DocumentTitleChanged(MakeCallback<ICoreWebView2DocumentTitleChangedEventHandler>(
        [get](ICoreWebView2* sender, IUnknown*) -> HRESULT {
            auto [app, current] = get();
            if (!current) return S_OK;
            CoString title;
            if (SUCCEEDED(sender->get_DocumentTitle(&title.value))) {
                current->title = SafeTitle(title.str());
                if (current->title.empty()) current->title = current->url.substr(0, 250);
            }
            app->QueueUI();
            app->Dirty();
            return S_OK;
        }).Get(), &tab.title_changed);
    tab.web->add_HistoryChanged(MakeCallback<ICoreWebView2HistoryChangedEventHandler>(
        [get](ICoreWebView2*, IUnknown*) -> HRESULT {
            auto [app, current] = get();
            if (current && app->IsActive(*current)) app->QueueUI();
            return S_OK;
        }).Get(), &tab.history);
    tab.web->add_PermissionRequested(MakeCallback<ICoreWebView2PermissionRequestedEventHandler>(
        [get](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
            auto [app, current] = get();
            if (!current || !args) return S_OK;
            COREWEBVIEW2_PERMISSION_KIND web_kind = COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION;
            if (FAILED(args->get_PermissionKind(&web_kind))) return S_OK;
            const auto kind = SitePermissionKindFor(web_kind);
            if (!kind) return S_OK;
            CoString uri;
            if (FAILED(args->get_Uri(&uri.value))) return S_OK;
            const auto origin = SiteOrigin(Utf8(uri.str()));
            if (origin.empty()) return S_OK;
            // Keep WebCom in control of supported permissions. Otherwise WebView2
            // can persist its own answer and stop raising PermissionRequested.
            ComPtr<ICoreWebView2PermissionRequestedEventArgs3> extended;
            if (SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&extended)))) extended->put_SavesInProfile(FALSE);
            const auto decision = current->private_mode ? SitePermissionDecision::Ask :
                SitePermissionFor(app->site_permissions_, origin, *kind);
            if (decision != SitePermissionDecision::Ask)
                args->put_State(WebPermissionState(decision));
            return S_OK;
        }).Get(), &tab.permission);
    ComPtr<ICoreWebView2_15> icons;
    if(SUCCEEDED(tab.web.As(&icons)))icons->add_FaviconChanged(
        MakeCallback<ICoreWebView2FaviconChangedEventHandler>([get](ICoreWebView2*,IUnknown*)->HRESULT{
            auto [app,current]=get();if(current)app->RequestFavicon(*current);return S_OK;
        }).Get(),&tab.favicon_changed);
    tab.web->add_NewWindowRequested(MakeCallback<ICoreWebView2NewWindowRequestedEventHandler>(
        [get](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
            auto [app, current] = get();
            args->put_Handled(TRUE);
            if (!current) return S_OK;
            BOOL initiated = FALSE;
            args->get_IsUserInitiated(&initiated);
            if (!initiated) { app->Note(L"An automatic popup was blocked."); return S_OK; }
            CoString uri;
            args->get_Uri(&uri.value);
            const auto url = uri.str();
            if (!IsWebAddress(url) && url != L"about:blank" && !url.empty()) {
                app->Note(L"The popup uses an address type WebCom does not support.");
                return S_OK;
            }
            ComPtr<ICoreWebView2Deferral> deferral;
            if (FAILED(args->GetDeferral(&deferral))) return S_OK;
            // Create a native placeholder first, then hand the engine an actual
            // new WebView. This preserves window.opener and popup navigation.
            const auto popup_id = app->Add(L"webcom:newtab", false, current->private_mode);
            if (auto* popup = app->Find(popup_id)) {
                popup->url = L"about:blank";
                popup->title = L"Opening tab...";
                popup->popup_args = args;
                popup->popup_deferral = deferral;
                app->Select(app->tabs_.size() - 1);
            } else deferral->Complete();
            return S_OK;
        }).Get(), &tab.popup);
    tab.web->add_WindowCloseRequested(MakeCallback<ICoreWebView2WindowCloseRequestedEventHandler>(
        [get, id](ICoreWebView2*, IUnknown*) -> HRESULT {
            auto [app, current] = get();
            if (current) PostMessageW(app->window_, kCloseTab, 0, static_cast<LPARAM>(id));
            return S_OK;
        }).Get(), &tab.close_requested);
    tab.web->add_ProcessFailed(MakeCallback<ICoreWebView2ProcessFailedEventHandler>(
        [get, id](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
            auto [app, current] = get();
            if (!current) return S_OK;
            COREWEBVIEW2_PROCESS_FAILED_KIND kind{};
            args->get_ProcessFailedKind(&kind);
            if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED)
                PostMessageW(app->window_, kRunAction, EngineCrashed, static_cast<LPARAM>(app->environment_generation_));
            else if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED)
                PostMessageW(app->window_, kRunAction, TabCrashed, static_cast<LPARAM>(id));
            else app->Note(L"A page component stopped responding. Reload the page if it does not recover.");
            return S_OK;
        }).Get(), &tab.process_failed);
    tab.web->add_ContainsFullScreenElementChanged(MakeCallback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
        [get, id](ICoreWebView2*, IUnknown*) -> HRESULT {
            auto [app, current] = get();
            if (!current || !app->IsActive(*current)) return S_OK;
            // Resolve the latest requested state after this callback returns;
            // toggles queued during a rapid enter/exit would otherwise race.
            PostMessageW(app->window_, kPageFullscreen, 0, static_cast<LPARAM>(id));
            return S_OK;
        }).Get(), &tab.fullscreen);
    tab.controller->add_AcceleratorKeyPressed(MakeCallback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
        [get](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT {
            auto [app, current] = get();
            if (!current) return S_OK;
            COREWEBVIEW2_KEY_EVENT_KIND kind{};
            args->get_KeyEventKind(&kind);
            if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN && kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) return S_OK;
            UINT key = 0;
            args->get_VirtualKey(&key);
            const UINT command = app->Shortcut(key, (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                (GetKeyState(VK_SHIFT) & 0x8000) != 0, (GetKeyState(VK_MENU) & 0x8000) != 0);
            if (command) {
                args->put_Handled(TRUE);
                // Accelerator callbacks are synchronous IPC. Never call the
                // WebView or open a modal dialog until after this returns.
                PostMessageW(app->window_, kRunAction, command, 0);
            }
            return S_OK;
        }).Get(), &tab.accelerator);
    ComPtr<ICoreWebView2_8> media;
    if (SUCCEEDED(tab.web.As(&media))) {
        media->put_IsMuted(tab.muted ? TRUE : FALSE);
        media->add_IsMutedChanged(MakeCallback<ICoreWebView2IsMutedChangedEventHandler>(
            [get](ICoreWebView2* sender,IUnknown*) -> HRESULT {
                auto [app,current]=get();if(!current)return S_OK;
                ComPtr<ICoreWebView2_8> audio;BOOL muted=FALSE;
                if(SUCCEEDED(sender->QueryInterface(IID_ICoreWebView2_8,reinterpret_cast<void**>(audio.GetAddressOf()))))audio->get_IsMuted(&muted);
                current->muted=muted!=FALSE;app->QueueUI();return S_OK;
            }).Get(),&tab.muted_changed);
        media->add_IsDocumentPlayingAudioChanged(MakeCallback<ICoreWebView2IsDocumentPlayingAudioChangedEventHandler>(
            [get](ICoreWebView2* sender, IUnknown*) -> HRESULT {
                auto [app, current] = get();
                if (!current) return S_OK;
                ComPtr<ICoreWebView2_8> audio;
                BOOL playing = FALSE;
                if (SUCCEEDED(sender->QueryInterface(IID_ICoreWebView2_8,reinterpret_cast<void**>(audio.GetAddressOf())))) audio->get_IsDocumentPlayingAudio(&playing);
                current->activity.audible = playing != FALSE;
                if (playing) { current->activity.last_active_ms = GetTickCount64(); app->LowMemoryTarget(*current, false); }
                app->QueueUI();
                return S_OK;
            }).Get(), &tab.audio);
    }
    ComPtr<ICoreWebView2_4> downloads;
    if (SUCCEEDED(tab.web.As(&downloads))) {
        downloads->add_DownloadStarting(MakeCallback<ICoreWebView2DownloadStartingEventHandler>(
            [get](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                auto [app, current] = get();
                if (!current) return S_OK;
                ComPtr<ICoreWebView2DownloadOperation> download;
                if (SUCCEEDED(args->get_DownloadOperation(&download))) {
                    current->downloads.push_back(download);
                    app->TrackDownload(download.Get());
                }
                // The runtime owns its normal download prompt, destination,
                // progress and security checks. Keep the source tab awake.
                current->activity.last_active_ms = GetTickCount64();
                app->LowMemoryTarget(*current, false);
                return S_OK;
            }).Get(), &tab.download);
    }
}

void BrowserApp::Impl::LowMemoryTarget(Tab& tab, bool low) {
    if (!tab.web) return;
    ComPtr<ICoreWebView2_19> target;
    if (SUCCEEDED(tab.web.As(&target))) target->put_MemoryUsageTargetLevel(low ?
        COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW : COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_NORMAL);
}

void BrowserApp::Impl::Suspend(Tab& tab, bool manual) {
    if (tab.web) {
        ComPtr<ICoreWebView2_8> media;
        BOOL audible = TRUE;
        if (SUCCEEDED(tab.web.As(&media))) media->get_IsDocumentPlayingAudio(&audible);
        tab.activity.audible = audible != FALSE;
    }
    if (!tab.web || IsActive(tab) || tab.activity.loading || tab.activity.audible || tab.activity.pinned ||
        tab.activity.protected_from_sleep || tab.activity.suspended || tab.activity.suspend_pending || HasDownload(tab)) return;
    ComPtr<ICoreWebView2_3> lifecycle;
    if (FAILED(tab.web.As(&lifecycle))) { if (manual) Note(L"Update the WebView2 Runtime to use sleeping tabs."); return; }
    tab.controller->put_IsVisible(FALSE);
    tab.activity.suspend_pending = true;
    const auto weak = weak_from_this();
    const auto id = tab.id;
    const auto generation = tab.generation;
    const HRESULT hr = lifecycle->TrySuspend(MakeCallback<ICoreWebView2TrySuspendCompletedHandler>(
        [weak, id, generation, manual](HRESULT error, BOOL succeeded) -> HRESULT {
            auto app = weak.lock();
            auto* current = app ? app->Find(id) : nullptr;
            if (!app || app->closing_ || !current || current->generation != generation) return S_OK;
            current->activity.suspend_pending = false;
            if (app->IsActive(*current) || current->activity.loading || current->activity.pinned || current->activity.protected_from_sleep ||
                current->activity.audible || app->HasDownload(*current)) {
                // The user may have selected/protected a tab before the async
                // suspend completed. A stale completion must not freeze it.
                ComPtr<ICoreWebView2_3> resume;
                if (SUCCEEDED(current->web.As(&resume))) resume->Resume();
                current->activity.suspended = false;
                app->LowMemoryTarget(*current, false);
            } else {
                ComPtr<ICoreWebView2_3> lifecycle;
                BOOL actually_suspended = FALSE;
                if (SUCCEEDED(current->web.As(&lifecycle))) lifecycle->get_IsSuspended(&actually_suspended);
                current->activity.suspended = SUCCEEDED(error) && succeeded != FALSE && actually_suspended != FALSE;
                if (!current->activity.suspended) {
                    current->activity.last_active_ms = GetTickCount64();
                    if (manual) app->Note(L"The runtime kept a busy tab awake. Its page state was preserved.");
                }
            }
            app->UpdateUI();
            return S_OK;
        }).Get());
    if (FAILED(hr)) {
        tab.activity.suspend_pending = false;
        tab.activity.last_active_ms = GetTickCount64();
        if (manual) Note(L"This tab could not sleep yet. Its page state was preserved.");
    }
}

void BrowserApp::Impl::Tick() {
    if (closing_) return;
    const auto now = GetTickCount64();
    for (auto& tab : tabs_) {
        if (!tab->web) continue;
        const bool downloading = HasDownload(*tab);
        if (IsActive(*tab)) { tab->activity.last_active_ms = now; continue; }
        ComPtr<ICoreWebView2_8> media;
        BOOL audible = TRUE; // Conservative if an older runtime cannot report audio.
        if (SUCCEEDED(tab->web.As(&media))) media->get_IsDocumentPlayingAudio(&audible);
        tab->activity.audible = audible != FALSE;
        if (downloading || audible) { tab->activity.last_active_ms = now; continue; }
        if (ShouldSuspend(tab->activity, now, state_.memory_mode)) Suspend(*tab, false);
    }
}


std::filesystem::path HistoryPathFor(const std::filesystem::path& root, const std::string& profile) {
    const auto hash = static_cast<unsigned long long>(std::hash<std::string>{}(profile));
    std::ostringstream name; name << "history-" << std::hex << hash << ".tsv";
    return root / name.str();
}

void BrowserApp::Impl::LoadHistory() {
    history_entries_.clear();
    std::ifstream in(HistoryPathFor(root_, profile_name_), std::ios::binary);
    std::string line;
    while (history_entries_.size() < 500 && std::getline(in, line)) {
        const auto a=line.find('\t'), b=a==std::string::npos?std::string::npos:line.find('\t',a+1);
        if(a==std::string::npos || b==std::string::npos) continue;
        std::uint64_t when=0;
        try { when=std::stoull(line.substr(0,a)); } catch (...) { continue; }
        const auto url=UnescapeField(std::string_view(line).substr(a+1,b-a-1));
        const auto title=UnescapeField(std::string_view(line).substr(b+1));
        const auto wide_url=Wide(url), wide_title=Wide(title);
        if(!IsWebAddress(wide_url)) continue;
        history_entries_.push_back({wide_url,SafeTitle(wide_title),when});
    }
}

void BrowserApp::Impl::SaveHistory() const {
    std::error_code ec; std::filesystem::create_directories(root_,ec);
    const auto path=HistoryPathFor(root_,profile_name_);
    const std::filesystem::path temp=path.wstring()+L".tmp";
    std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    if(!out) return;
    for(const auto& item:history_entries_) out<<item.visited_ms<<'\t'<<EscapeField(Utf8(item.url))<<'\t'<<EscapeField(Utf8(item.title))<<'\n';
    out.close();
    if(!out) { std::filesystem::remove(temp,ec); return; }
    std::filesystem::rename(temp,path,ec);
    if(ec) { ec.clear(); std::filesystem::remove(path,ec); ec.clear(); std::filesystem::rename(temp,path,ec); }
}

void BrowserApp::Impl::RecordHistory(const Tab& tab) {
    if(tab.private_mode || !IsWebAddress(tab.url)) return;
    const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    HistoryEntry entry{tab.url, SafeTitle(tab.title.empty()?tab.url:tab.title), now};
    if(!history_entries_.empty() && history_entries_.front().url==entry.url && now-history_entries_.front().visited_ms<2000) {
        history_entries_.front()=std::move(entry);
    } else {
        history_entries_.insert(history_entries_.begin(),std::move(entry));
        if(history_entries_.size()>500) history_entries_.resize(500);
    }
    SaveHistory();
}

std::filesystem::path DownloadsPathFor(const std::filesystem::path& root, const std::string& profile) {
    const auto hash = static_cast<unsigned long long>(std::hash<std::string>{}(profile));
    std::ostringstream name; name << "downloads-" << std::hex << hash << ".tsv";
    return root / name.str();
}

void BrowserApp::Impl::LoadDownloads() {
    download_entries_.clear();
    std::ifstream in(DownloadsPathFor(root_, profile_name_), std::ios::binary);
    std::string line;
    while (download_entries_.size() < 200 && std::getline(in, line)) {
        std::array<std::size_t, 5> sep{};
        std::size_t start = 0;
        bool valid = true;
        for (std::size_t i = 0; i < sep.size(); ++i) {
            sep[i] = line.find('\t', start);
            if (sep[i] == std::string::npos) { valid = false; break; }
            start = sep[i] + 1;
        }
        if (!valid) continue;
        auto item = std::make_unique<DownloadEntry>();
        try {
            item->started_ms = std::stoull(line.substr(0, sep[0]));
            const int state = std::stoi(line.substr(sep[0] + 1, sep[1] - sep[0] - 1));
            if (state != static_cast<int>(COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS) &&
                state != static_cast<int>(COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED) &&
                state != static_cast<int>(COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED)) continue;
            item->state = static_cast<COREWEBVIEW2_DOWNLOAD_STATE>(state);
            item->received = std::stoll(line.substr(sep[1] + 1, sep[2] - sep[1] - 1));
            item->total = std::stoll(line.substr(sep[2] + 1, sep[3] - sep[2] - 1));
        } catch (...) { continue; }
        item->path = Wide(UnescapeField(std::string_view(line).substr(sep[3] + 1, sep[4] - sep[3] - 1)));
        item->uri = Wide(UnescapeField(std::string_view(line).substr(sep[4] + 1)));
        // A previous-run in-progress download cannot still be controlled here.
        if (item->state == COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS)
            item->state = COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED;
        item->id = next_download_id_++;
        download_entries_.push_back(std::move(item));
    }
}

void BrowserApp::Impl::SaveDownloads() const {
    std::error_code ec; std::filesystem::create_directories(root_, ec);
    const auto path = DownloadsPathFor(root_, profile_name_);
    const std::filesystem::path temp = path.wstring() + L".tmp";
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) return;
    const std::size_t count = std::min<std::size_t>(download_entries_.size(), 200);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& item = *download_entries_[i];
        out << item.started_ms << '\t' << static_cast<int>(item.state) << '\t'
            << item.received << '\t' << item.total << '\t'
            << EscapeField(Utf8(item.path)) << '\t' << EscapeField(Utf8(item.uri)) << '\n';
    }
    out.close();
    if (!out) { std::filesystem::remove(temp, ec); return; }
    std::filesystem::rename(temp, path, ec);
    if (ec) { ec.clear(); std::filesystem::remove(path, ec); ec.clear(); std::filesystem::rename(temp, path, ec); }
}

void BrowserApp::Impl::RefreshDownload(DownloadEntry& entry) {
    if (!entry.operation) return;
    CoString text;
    if (SUCCEEDED(entry.operation->get_Uri(&text.value)) && text.value) entry.uri = text.str();
    CoTaskMemFree(text.value); text.value = nullptr;
    if (SUCCEEDED(entry.operation->get_ResultFilePath(&text.value)) && text.value) entry.path = text.str();
    INT64 received = 0, total = -1;
    if (SUCCEEDED(entry.operation->get_BytesReceived(&received))) entry.received = received;
    if (SUCCEEDED(entry.operation->get_TotalBytesToReceive(&total))) entry.total = total;
    COREWEBVIEW2_DOWNLOAD_STATE state = entry.state;
    if (SUCCEEDED(entry.operation->get_State(&state))) entry.state = state;
    COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON reason = entry.interrupt_reason;
    if (entry.state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED &&
        SUCCEEDED(entry.operation->get_InterruptReason(&reason))) entry.interrupt_reason = reason;
    else if (entry.state != COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED)
        entry.interrupt_reason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE;
    entry.paused = entry.state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED &&
                   entry.interrupt_reason == COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_PAUSED;
}

void BrowserApp::Impl::TrackDownload(ICoreWebView2DownloadOperation* operation) {
    if (!operation) return;
    auto entry = std::make_unique<DownloadEntry>();
    entry->id = next_download_id_++;
    entry->operation = operation;
    entry->started_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    RefreshDownload(*entry);
    const auto id = entry->id;
    const std::weak_ptr<Impl> weak = shared_from_this();
    const HRESULT bytes_result = entry->operation->add_BytesReceivedChanged(
        MakeCallback<ICoreWebView2BytesReceivedChangedEventHandler>(
            [weak,id](ICoreWebView2DownloadOperation*, IUnknown*) -> HRESULT {
                if (auto app = weak.lock()) app->OnDownloadChanged(id, false);
                return S_OK;
            }).Get(), &entry->bytes_changed);
    const HRESULT state_result = entry->operation->add_StateChanged(
        MakeCallback<ICoreWebView2StateChangedEventHandler>(
            [weak,id](ICoreWebView2DownloadOperation*, IUnknown*) -> HRESULT {
                if (auto app = weak.lock()) app->OnDownloadChanged(id, true);
                return S_OK;
            }).Get(), &entry->state_changed);
    entry->event_driven = SUCCEEDED(bytes_result) && SUCCEEDED(state_result);
    download_entries_.insert(download_entries_.begin(), std::move(entry));
    if (download_entries_.size() > 200) download_entries_.resize(200);
    SaveDownloads();
    // Older runtimes or partial event registration fall back to a low-frequency
    // timer. Event-capable runtimes update the UI directly from WebView2.
    if (!download_entries_.front()->event_driven) SetTimer(window_, kDownloadTick, 1000, nullptr);
    if (auto* tab = Active(); tab && tab->url == L"webcom:downloads") QueueUI();
    Note(L"Download started.");
}

void BrowserApp::Impl::OnDownloadChanged(std::uint64_t id, bool state_event) {
    const auto found = std::find_if(download_entries_.begin(), download_entries_.end(),
        [id](const auto& item){ return item && item->id == id; });
    if (found == download_entries_.end()) return;
    auto& item = **found;
    const auto before_state = item.state;
    const auto before_bytes = item.received;
    const bool before_paused = item.paused;
    RefreshDownload(item);
    const auto now = GetTickCount64();
    const bool meaningful = state_event || before_state != item.state || before_paused != item.paused;
    if (meaningful || now - item.last_persist_tick >= 2000) {
        item.last_persist_tick = now;
        SaveDownloads();
    }
    if (auto* current = Active(); current && current->url == L"webcom:downloads" &&
        (meaningful || before_bytes != item.received)) QueueUI();
    if (state_event && before_state != item.state) {
        if (item.state == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED) {
            std::wstring name = item.path.empty() ? L"Download" : std::filesystem::path(item.path).filename().wstring();
            if (name.empty()) name = L"Download";
            Note(name + L" finished downloading.");
        } else if (item.state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED && !item.paused &&
                   item.interrupt_reason != COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_CANCELED) {
            Note(L"Download interrupted. Open Downloads to retry it.");
        }
    }
}

void BrowserApp::Impl::ToggleDownloadPause(std::size_t index) {
    if (index >= download_entries_.size()) return;
    auto& item = *download_entries_[index];
    RefreshDownload(item);
    if (!item.operation) { Note(L"This download can no longer be controlled."); return; }
    HRESULT result = E_FAIL;
    if (item.paused) {
        BOOL can_resume = FALSE;
        if (FAILED(item.operation->get_CanResume(&can_resume)) || !can_resume) {
            Note(L"This paused download cannot be resumed by the browser engine."); return;
        }
        result = item.operation->Resume();
        if (SUCCEEDED(result)) {
            item.state = COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;
            item.interrupt_reason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE;
            item.paused = false;
        }
    } else if (item.state == COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS) {
        result = item.operation->Pause();
        if (SUCCEEDED(result)) {
            item.state = COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED;
            item.interrupt_reason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_PAUSED;
            item.paused = true;
        }
    } else {
        Note(L"This download is not currently running."); return;
    }
    if (FAILED(result)) { Note(item.paused ? L"This download could not be resumed." : L"This download could not be paused."); return; }
    if (!item.event_driven) SetTimer(window_, kDownloadTick, 1000, nullptr);
    SaveDownloads();
    QueueUI();
    Note(item.paused ? L"Download paused." : L"Download resumed.");
}

void BrowserApp::Impl::CancelDownload(std::size_t index) {
    if (index >= download_entries_.size()) return;
    auto& item = *download_entries_[index];
    RefreshDownload(item);
    if (!item.operation || (item.state != COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS && !item.paused)) {
        Note(L"This download is no longer running."); return;
    }
    const HRESULT result = item.operation->Cancel();
    if (FAILED(result)) { Note(L"This download could not be cancelled."); return; }
    item.paused = false;
    item.state = COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED;
    item.interrupt_reason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
    SaveDownloads();
    QueueUI();
    Note(L"Download cancelled.");
}

void BrowserApp::Impl::RetryDownload(std::size_t index) {
    if (index >= download_entries_.size()) return;
    auto& item = *download_entries_[index];
    RefreshDownload(item);
    if (item.operation && item.state == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED) {
        BOOL can_resume = FALSE;
        if (SUCCEEDED(item.operation->get_CanResume(&can_resume)) && can_resume) {
            if (SUCCEEDED(item.operation->Resume())) {
                item.state = COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;
                item.interrupt_reason = COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE;
                item.paused = false;
                if (!item.event_driven) SetTimer(window_, kDownloadTick, 1000, nullptr);
                QueueUI();
                Note(L"Download resumed.");
                return;
            }
        }
    }
    if (item.uri.empty()) { Note(L"The original download address is unavailable."); return; }
    const auto resolved = ResolveAddress(Utf8(item.uri));
    const bool http_source = resolved.url.rfind("http://", 0) == 0 || resolved.url.rfind("https://", 0) == 0;
    if (!resolved.allowed || !http_source) { Note(L"WebCom cannot safely reopen this download source."); return; }
    Add(Wide(resolved.url), true, false);
    Note(L"Opened the original download source in a new tab.");
}

void BrowserApp::Impl::ClearDownloads() {
    // Keep active operations visible; clearing history must not cancel downloads.
    std::erase_if(download_entries_, [](const auto& item) {
        return item->state != COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS && !item->paused;
    });
    SaveDownloads();
    if (auto* tab = Active(); tab && tab->url == L"webcom:downloads") QueueUI();
}

void BrowserApp::Impl::SaveSitePermissionsNow() {
    std::string error;
    if (!SaveSitePermissions(root_, profile_name_, site_permissions_, &error))
        Note(L"Site permissions could not be saved. " + Wide(error));
}

void BrowserApp::Impl::SyncRuntimeSitePermission(Tab& tab, std::string_view origin,
                                                  SitePermissionKind kind, SitePermissionDecision decision) {
    if (!tab.web || tab.private_mode || origin.empty()) return;
    ComPtr<ICoreWebView2_13> access;
    ComPtr<ICoreWebView2Profile> profile;
    ComPtr<ICoreWebView2Profile4> permissions;
    if (FAILED(tab.web.As(&access)) || FAILED(access->get_Profile(&profile)) || FAILED(profile.As(&permissions))) return;
    const auto origin_wide = Wide(origin);
    if (origin_wide.empty()) return;
    const auto weak = weak_from_this();
    permissions->SetPermissionState(WebPermissionKind(kind), origin_wide.c_str(), WebPermissionState(decision),
        MakeCallback<ICoreWebView2SetPermissionStateCompletedHandler>([weak](HRESULT error) -> HRESULT {
            if (FAILED(error)) if (auto app=weak.lock();app && !app->closing_)
                app->Note(L"The WebView2 Runtime could not update this permission immediately.");
            return S_OK;
        }).Get());
}

void BrowserApp::Impl::RefreshSiteCookies(Tab& tab) {
    if (closing_ || !tab.web || !IsWebAddress(tab.url) || tab.cookies_loading) return;
    ComPtr<ICoreWebView2CookieManager> manager;
    if (FAILED(tab.web->get_CookieManager(&manager)) || !manager) return;
    tab.cookies_loading = true;
    const auto weak = weak_from_this();
    const auto id = tab.id;
    const auto generation = tab.generation;
    const std::wstring uri = tab.url;
    const HRESULT result = manager->GetCookies(uri.c_str(),
        MakeCallback<ICoreWebView2GetCookiesCompletedHandler>(
            [weak,id,generation](HRESULT error, ICoreWebView2CookieList* list) -> HRESULT {
                auto app=weak.lock();
                auto* current=app?app->Find(id):nullptr;
                if(!app || app->closing_ || !current || current->generation!=generation) return S_OK;
                current->cookies_loading=false;
                current->site_cookies.clear();
                current->site_cookie_count=0;
                current->cookies_known=false;
                if(FAILED(error) || !list){ app->QueueUI(); return S_OK; }
                UINT count=0;
                if(FAILED(list->get_Count(&count))){ app->QueueUI(); return S_OK; }
                current->site_cookie_count=count;
                current->cookies_known=true;
                const UINT cache_count=std::min<UINT>(count,50);
                current->site_cookies.reserve(cache_count);
                for(UINT i=0;i<cache_count;++i){
                    ComPtr<ICoreWebView2Cookie> cookie;
                    if(FAILED(list->GetValueAtIndex(i,&cookie)) || !cookie) continue;
                    CookieSummary summary;
                    CoString name,domain,path;
                    BOOL secure=FALSE,http_only=FALSE;
                    cookie->get_Name(&name.value);
                    cookie->get_Domain(&domain.value);
                    cookie->get_Path(&path.value);
                    cookie->get_IsSecure(&secure);
                    cookie->get_IsHttpOnly(&http_only);
                    summary.name=name.str(); summary.domain=domain.str(); summary.path=path.str();
                    summary.secure=secure!=FALSE; summary.http_only=http_only!=FALSE;
                    current->site_cookies.push_back(std::move(summary));
                }
                app->QueueUI();
                return S_OK;
            }).Get());
    if(FAILED(result)) tab.cookies_loading=false;
}

void BrowserApp::Impl::SiteCookiesMenu() {
    if(menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    auto* tab=Active();
    if(!tab || !tab->web || !IsWebAddress(tab->url)){ Note(L"Open a website first."); return; }
    if(!tab->cookies_known){ RefreshSiteCookies(*tab); Note(L"Cookie information is loading. Open site information again in a moment."); return; }
    std::vector<MenuItem> items;
    const std::wstring count_text=std::to_wstring(tab->site_cookie_count)+L" cookie"+(tab->site_cookie_count==1?L"":L"s")+L" in use";
    items.push_back({0,L"Cookies and site data",count_text,L"\uE74E",false});
    items.push_back({0,L"",L"",L"",true,false,true});
    const std::size_t shown=std::min<std::size_t>(tab->site_cookies.size(),12);
    for(std::size_t i=0;i<shown;++i){
        const auto& cookie=tab->site_cookies[i];
        std::wstring detail=cookie.domain;
        if(!cookie.path.empty()) detail+=cookie.path;
        if(cookie.secure) detail+=L"  •  Secure";
        if(cookie.http_only) detail+=L"  •  HttpOnly";
        items.push_back({0,cookie.name.empty()?L"(unnamed cookie)":cookie.name,detail,L"\uE8D7",false});
    }
    if(tab->site_cookie_count>shown)
        items.push_back({0,L"More cookies",std::to_wstring(tab->site_cookie_count-shown)+L" additional cookies are not shown",L"\uE712",false});
    items.push_back({0,L"",L"",L"",true,false,true});
    items.push_back({ClearSiteCookiesCmd,L"Clear cookies for this site",L"Signs you out of this site",L"\uE74D",tab->site_cookie_count>0});
    RECT r{};GetWindowRect(GetDlgItem(window_,SiteInfo),&r);
    UINT selected=0;
    {
        menu_open_=true;
        struct Reset{bool& value;~Reset(){value=false;}} reset{menu_open_};
        selected=ShowDarkMenu(window_,{r.left,r.bottom},items,dpi_,390,false,&palette_);
    }
    if(selected==ClearSiteCookiesCmd) ClearSiteCookies();
}

void BrowserApp::Impl::ClearSiteCookies() {
    if(closing_) return;
    auto* tab=Active();
    if(!tab || !tab->web || !IsWebAddress(tab->url)) return;
    ComPtr<ICoreWebView2CookieManager> manager;
    if(FAILED(tab->web->get_CookieManager(&manager)) || !manager){ Note(L"Cookies could not be accessed for this site."); return; }
    const auto weak=weak_from_this();
    const auto id=tab->id;
    const auto generation=tab->generation;
    const std::wstring uri=tab->url;
    const HRESULT result=manager->GetCookies(uri.c_str(),
        MakeCallback<ICoreWebView2GetCookiesCompletedHandler>(
            [weak,id,generation,manager](HRESULT error,ICoreWebView2CookieList* list)->HRESULT{
                auto app=weak.lock();auto* current=app?app->Find(id):nullptr;
                if(!app || app->closing_ || !current || current->generation!=generation) return S_OK;
                if(FAILED(error) || !list){app->Note(L"Cookies could not be cleared for this site.");return S_OK;}
                UINT count=0;if(FAILED(list->get_Count(&count))){app->Note(L"Cookies could not be cleared for this site.");return S_OK;}
                for(UINT i=0;i<count;++i){ComPtr<ICoreWebView2Cookie> cookie;if(SUCCEEDED(list->GetValueAtIndex(i,&cookie)) && cookie) manager->DeleteCookie(cookie.Get());}
                current->site_cookies.clear();current->site_cookie_count=0;current->cookies_known=true;current->cookies_loading=false;
                app->QueueUI();app->Notice(L"Cookies for this site were cleared. Reload the page to start a fresh site session.");
                return S_OK;
            }).Get());
    if(FAILED(result)) Note(L"Cookies could not be cleared for this site.");
}

void BrowserApp::Impl::SiteInfoMenu() {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    auto* tab = Active();
    if (!tab || !IsWebAddress(tab->url)) { Note(L"Site controls are available for HTTP and HTTPS pages."); return; }
    if(!tab->cookies_loading) RefreshSiteCookies(*tab);
    const auto origin_utf8 = SiteOrigin(Utf8(tab->url));
    if (origin_utf8.empty()) return;
    const auto origin = Wide(origin_utf8);
    const bool https = tab->url.size()>=8 && _wcsnicmp(tab->url.c_str(),L"https://",8)==0;
    std::wstring security_title,security_detail;
    if(tab->certificate_error){
        security_title=L"Certificate problem";
        security_detail=CertificateErrorLabel(*tab->certificate_error);
    }else if(https){
        security_title=L"Connection is secure";
        security_detail=L"HTTPS • certificate verified by Windows/WebView2";
    }else{
        security_title=L"Connection is not secure";
        security_detail=L"HTTP traffic is not encrypted";
    }
    const std::wstring cookie_detail=tab->cookies_known
        ? std::to_wstring(tab->site_cookie_count)+L" cookie"+(tab->site_cookie_count==1?L"":L"s")+L" in use"
        : tab->cookies_loading?L"Checking cookies…":L"Cookie information unavailable";
    constexpr std::array<SitePermissionKind,4> kinds{
        SitePermissionKind::Camera, SitePermissionKind::Microphone,
        SitePermissionKind::Geolocation, SitePermissionKind::Notifications};
    std::vector<MenuItem> items;
    items.push_back({0,origin,tab->private_mode?L"Incognito site information":L"Site information",L"\uE72E",false});
    items.push_back({0,L"",L"",L"",true,false,true});
    items.push_back({0,security_title,security_detail,L"\uE72E",false});
    items.push_back({ShowSiteCookiesCmd,L"Cookies and site data",cookie_detail,L"\uE74E",tab->cookies_known});
    items.push_back({0,L"",L"",L"",true,false,true});
    for(std::size_t i=0;i<kinds.size();++i){
        if(tab->private_mode){
            items.push_back({0,Wide(SitePermissionKindName(kinds[i])),L"Ask each time",L"\uE7BA",false});
        }else{
            const auto decision=SitePermissionFor(site_permissions_,origin_utf8,kinds[i]);
            items.push_back({SitePermissionBase+static_cast<UINT>(i),Wide(SitePermissionKindName(kinds[i])),Wide(SitePermissionDecisionName(decision)),L"\uE7BA"});
        }
    }
    const bool customized=!tab->private_mode && std::any_of(site_permissions_.begin(),site_permissions_.end(),
        [&](const SitePermissionRule& rule){return rule.origin==origin_utf8;});
    if(!tab->private_mode){
        items.push_back({0,L"",L"",L"",true,false,true});
        items.push_back({ResetSitePermissionsCmd,L"Reset permissions for this site",L"",L"\uE777",customized});
    }
    RECT r{};GetWindowRect(GetDlgItem(window_,SiteInfo),&r);
    UINT selected=0;
    {
        menu_open_=true;
        struct Reset{bool& value;~Reset(){value=false;}} reset{menu_open_};
        selected=ShowDarkMenu(window_,{r.left,r.bottom},items,dpi_,390,false,&palette_);
    }
    if(closing_ || !IsWindow(window_) || !selected) return;
    if(selected==ShowSiteCookiesCmd){ SiteCookiesMenu(); return; }
    if(selected>=SitePermissionBase && selected<SitePermissionBase+kinds.size() && !tab->private_mode){
        const auto kind=kinds[selected-SitePermissionBase];
        const auto current=SitePermissionFor(site_permissions_,origin_utf8,kind);
        const auto next=NextSitePermissionDecision(current);
        SetSitePermission(site_permissions_,origin_utf8,kind,next);
        SaveSitePermissionsNow();
        SyncRuntimeSitePermission(*tab,origin_utf8,kind,next);
        InvalidateRect(GetDlgItem(window_,SiteInfo),nullptr,FALSE);
        Note(Wide(SitePermissionKindName(kind))+L" for this site: "+Wide(SitePermissionDecisionName(next))+L". Reload if the page already requested it.");
    }else if(selected==ResetSitePermissionsCmd && !tab->private_mode){
        ResetSitePermissions(site_permissions_,origin_utf8);
        SaveSitePermissionsNow();
        for(const auto kind:kinds) SyncRuntimeSitePermission(*tab,origin_utf8,kind,SitePermissionDecision::Ask);
        InvalidateRect(GetDlgItem(window_,SiteInfo),nullptr,FALSE);
        Note(L"Site permissions reset to Ask. Reload the page to request them again.");
    }
}

void BrowserApp::Impl::DownloadQuickMenu() {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    if (download_entries_.empty()) { OpenInternalPage(L"webcom:downloads"); return; }
    std::vector<MenuItem> items;
    const std::size_t count=std::min<std::size_t>(download_entries_.size(),5);
    for(std::size_t i=0;i<count;++i) {
        auto& item=*download_entries_[i];
        RefreshDownload(item);
        std::wstring name=item.path.empty()?L"Download":std::filesystem::path(item.path).filename().wstring();
        if(name.empty()) name=L"Download";
        std::wstring detail;
        if(item.paused) detail=L"Paused";
        else if(item.state==COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS) {
            if(item.total>0) {
                const auto percent=static_cast<int>(std::clamp<std::int64_t>(item.received*100/item.total,0,100));
                detail=std::to_wstring(percent)+L"%  •  "+FormatBytes(item.received)+L" of "+FormatBytes(item.total);
            } else detail=FormatBytes(item.received)+L"  •  Downloading";
        } else detail=DownloadStateLabel(item.state);
        items.push_back({DownloadBase+static_cast<UINT>(i),name,detail,L"\uE896"});
    }
    items.push_back({0,L"",L"",L"",true,false,true});
    items.push_back({Downloads,L"Show all downloads",L"Ctrl+J",L"\uE896"});
    RECT r{}; GetWindowRect(GetDlgItem(window_,DownloadsQuick),&r);
    const POINT anchor{r.right,r.bottom};
    UINT selected=0;
    {
        menu_open_=true;
        struct Reset{bool& value;~Reset(){value=false;}} reset{menu_open_};
        selected=ShowDarkMenu(window_,anchor,items,dpi_,390,true,&palette_);
    }
    if(closing_ || !IsWindow(window_) || !selected) return;
    if(selected==Downloads) { OpenInternalPage(L"webcom:downloads"); return; }
    if(selected>=DownloadBase && selected<DownloadBase+count) {
        auto& item=*download_entries_[selected-DownloadBase];
        RefreshDownload(item);
        if(item.state==COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED && !item.path.empty())
            ShellExecuteW(window_,L"open",item.path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        else OpenInternalPage(L"webcom:downloads");
    }
}

void BrowserApp::Impl::OpenInternalPage(std::wstring url) {
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        if (!tabs_[i]->private_mode && tabs_[i]->url == url) { Select(i); return; }
    }
    Add(std::move(url), true, false);
}

void BrowserApp::Impl::HistoryMenu() {
    if(menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    std::vector<MenuItem> items;
    const std::size_t count=std::min<std::size_t>(history_entries_.size(),18);
    for(std::size_t i=0;i<count;++i) {
        auto title=history_entries_[i].title.empty()?history_entries_[i].url:history_entries_[i].title;
        if(title.size()>46) title=title.substr(0,43)+L"...";
        items.push_back({HistoryBase+static_cast<UINT>(i),title,HistoryTimeLabel(history_entries_[i].visited_ms),L"\uE81C"});
    }
    if(items.empty()) items.push_back({0,L"No browsing history yet",L"",L"",false});
    items.push_back({0,L"",L"",L"",true,false,true});
    items.push_back({ClearHistory,L"Clear browsing history",L"",L"\uE74D",!history_entries_.empty()});
    RECT client{};GetClientRect(window_,&client);POINT anchor{client.right,fullscreen_?D(8):D(80)};ClientToScreen(window_,&anchor);
    menu_open_=true; struct Reset{bool& v;~Reset(){v=false;}} reset{menu_open_};
    const UINT selected=ShowDarkMenu(window_,anchor,items,dpi_,390,true,&palette_);
    if(!selected || closing_) return;
    if(selected>=HistoryBase && selected<HistoryBase+count) Navigate(history_entries_[selected-HistoryBase].url);
    else if(selected==ClearHistory){ history_entries_.clear(); SaveHistory(); Note(L"Browsing history cleared."); }
}

// Replacement definitions for app.cpp. MenuItem/ShowDarkMenu come from dark_menu.h.
// Dispatch routes PasswordMenu/HelpMenu to PreferencesMenu; SettingsMenu opens the native settings page.
// BookmarkMenu to BookmarksMenu, and implement Zoom*, PrintPage, DeveloperTools,
// and PrivateHelp. tab_rects_ uses absolute tab indices; invisible entries are zero.

void BrowserApp::Impl::MainMenu() {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    const auto keep_alive = shared_from_this();
    const HWND owner = window_;
    menu_tab_id_ = 0;
    RECT client{}; GetClientRect(owner, &client);
    POINT anchor{client.right, fullscreen_ ? D(8) : D(80)};
    ClientToScreen(owner, &anchor);
    main_menu_active_ = true;
    HWND more_button = GetDlgItem(owner, More);
    InvalidateRect(more_button, nullptr, FALSE);
    struct ClearMain {
        bool& active; HWND button;
        ~ClearMain() { active = false; if (IsWindow(button)) InvalidateRect(button, nullptr, FALSE); }
    } clear_main{main_menu_active_, more_button};
    for (;;) {
        Tab* tab = Active();
        const bool page = tab && tab->web;
        const bool findable = page || (tab && !tab->web && (tab->url==L"webcom:history" || tab->url==L"webcom:bookmarks"));
        const bool private_mode = tab && tab->private_mode;
        double zoom = 1.0;
        if (tab && tab->controller) tab->controller->get_ZoomFactor(&zoom);
        const std::wstring percentage = std::to_wstring(static_cast<int>(zoom * 100.0 + 0.5)) + L"%";
        const MenuItem divider{0, L"", L"", L"", false, false, true};
        std::vector<MenuItem> items{
            {NewTab, private_mode ? L"New normal tab" : L"New tab", L"Ctrl+T", L"\uE78B"},
            {NewProfileWindow, L"Open profile window...", L"Ctrl+N", L"\uE77B"},
            {PrivateTab, L"New Incognito tab", L"Ctrl+Shift+N", L"incognito"},
            {ReopenTab, L"Reopen closed tab", L"Ctrl+Shift+T", L"\uE777", !closed_tabs_.empty()},
            divider,
            {ProfilesMenu, private_mode ? L"Incognito" : Wide(profile_name_), L"\u203A",
                private_mode ? L"incognito" : L"\uE77B", true, false, false, true},
            {PasswordMenu, L"Passwords", L"\u203A", L"\uE72E"},
            {Downloads, L"Downloads", L"Ctrl+J", L"\uE896"},
            {History, L"History", L"Ctrl+H", L"\uE81C"},
            {BookmarkMenu, L"Bookmarks", L"\u203A", L"\uE734"},
            {TabList, L"Search tabs...", L"Ctrl+Shift+A", L"\uE721"},
            {ClearData, L"Delete browsing data...", L"Ctrl+Shift+Del", L"\uE74D"},
            divider,
            {ZoomReset, L"Zoom", percentage, L"\uE71E", true, false, false, false,
                ZoomOut, ZoomIn, FullScreen, page},
            divider,
            {PrintPage, L"Print...", L"Ctrl+P", L"\uE749", page},
            {SearchInPage, L"Find in page...", L"Ctrl+F", L"\uE721", findable},
            {OpenInDefault, L"Open in default browser", L"", L"\uE8A7", tab && IsWebAddress(tab->url)},
            {DeveloperTools, L"Developer tools", L"F12", L"\uEC7A", page},
            {MemoryReport, L"Performance", L"Shift+Esc", L"\uE9D9"},
            divider,
            {HelpMenu, L"Help", L"\u203A", L"\uE897"},
            {SettingsMenu, L"Settings", L"", L"\uE713"},
            {Exit, L"Close this window", L"Alt+F4", L"\uE8AC"}
        };
        UINT selected = 0;
        {
            menu_open_ = true;
            struct Reset { bool& value; ~Reset() { value = false; } } reset{menu_open_};
            selected = ShowDarkMenu(owner, anchor, items, dpi_, 334, true, &palette_);
        }
        if (closing_ || window_ != owner || !IsWindow(owner) || !selected) return;
        Dispatch(selected);
        if (closing_ || window_ != owner || !IsWindow(owner)) return;
        // Reopen at the same point after zoom so repeated +/- clicks keep working.
        if (selected != ZoomIn && selected != ZoomOut && selected != ZoomReset) return;
    }
}

void BrowserApp::Impl::ProfileMenu() {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    const auto keep_alive = shared_from_this();
    const HWND owner = window_;
    menu_tab_id_ = 0;
    std::vector<MenuItem> items;
    items.reserve(state_.profiles.size() + 5);
    for (std::size_t i = 0; i < state_.profiles.size(); ++i) {
        const bool current = state_.profiles[i].name == profile_name_;
        const auto session=session_.lock();
        const bool running=session && session->IsProfileOpen(state_.profiles[i].name);
        items.push_back({ProfileBase + static_cast<UINT>(i), Wide(state_.profiles[i].name),
                         current?L"This window":running?L"Open":L"", L"\uE77B", true, current, false, current});
        const auto appearance=current?appearance_:LoadAppearance(root_,state_.profiles[i].name);
        items.back().swatch_rgb=ProfileColor(appearance,state_.profiles[i].name);
    }
    items.push_back({0, L"", L"", L"", false, false, true});
    items.push_back({ProfileManager, L"Manage profiles", L"", L"\uE713"});
    items.push_back({AddProfile, L"Add profile...", L"", L"", state_.profiles.size() < 32});
    items.push_back({ProfileColorMenu,L"Profile color",L"›",L""});
    items.push_back({DeleteCurrentProfileCmd,L"Delete this profile...",L"",L"",state_.profiles.size()>1});
    items.push_back({PrivateTab, L"New Incognito tab", L"Ctrl+Shift+N", L"incognito"});
    if (auto* tab = Active(); tab && tab->private_mode)
        items.push_back({PrivateHelp, L"About Incognito", L"", L"\uE897"});
    RECT client{}; GetClientRect(owner, &client);
    POINT anchor{client.right, fullscreen_ ? D(8) : D(80)};
    ClientToScreen(owner, &anchor);
    RECT profile{};
    HWND button = GetDlgItem(owner, Profile);
    if (button && IsWindowVisible(button) && GetWindowRect(button, &profile))
        anchor = {profile.right, profile.bottom};
    UINT selected = 0;
    {
        menu_open_ = true;
        struct Reset { bool& value; ~Reset() { value = false; } } reset{menu_open_};
        selected = ShowDarkMenu(owner, anchor, items, dpi_, 334, true, &palette_);
    }
    if (!closing_ && window_ == owner && IsWindow(owner) && selected) Dispatch(selected);
}

void BrowserApp::Impl::TabMenu(POINT point) {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_) || tabs_.empty()) return;
    const auto keep_alive = shared_from_this();
    const HWND owner = window_;
    std::size_t target = active_;
    if (point.x == -1 && point.y == -1) {
        std::size_t visible = active_;
        if (visible >= tab_rects_.size() || IsRectEmpty(&tab_rects_[visible])) visible = first_visible_tab_;
        RECT rect{D(40), 0, D(80), D(40)};
        if (visible < tab_rects_.size() && !IsRectEmpty(&tab_rects_[visible])) rect = tab_rects_[visible];
        point = {rect.left + D(16), rect.bottom};
        ClientToScreen(tab_strip_, &point);
    } else {
        POINT local = point; ScreenToClient(tab_strip_, &local);
        const int hit = HitTab(local);
        if (hit >= 0 && static_cast<std::size_t>(hit) < tabs_.size()) target = static_cast<std::size_t>(hit);
    }
    if (target >= tabs_.size()) return;
    Tab& tab = *tabs_[target];
    const std::uint64_t target_id = tab.id;
    const bool background_page = !IsActive(tab) && tab.web && !tab.activity.loading && !HasDownload(tab);
    const bool can_sleep = background_page && !tab.activity.pinned &&
        !tab.activity.protected_from_sleep && !tab.activity.audible &&
        !tab.activity.suspended && !tab.activity.suspend_pending;
    const MenuItem divider{0, L"", L"", L"", false, false, true};
    std::vector<MenuItem> items{
        {NewTab, L"New tab", L"Ctrl+T", L"\uE710"},
        {TabList, L"Search tabs...", L"Ctrl+Shift+A", L"\uE721"},
        {DuplicateTab, L"Duplicate tab", L"", L"\uE8C8"},
        {MuteTab, tab.muted ? L"Unmute tab" : L"Mute tab", L"", L"\uE767", !IsHome(tab.url), tab.muted},
        {MoveTabLeft, L"Move tab left", L"Ctrl+Shift+PgUp", L"\uE72B", target > 0},
        {MoveTabRight, L"Move tab right", L"Ctrl+Shift+PgDn", L"\uE72A", target+1 < tabs_.size()},
        {PinTab, L"Pin tab", L"", L"\uE718", true, tab.activity.pinned},
        {KeepAwake, L"Keep this tab awake", L"", L"\uE945", true, tab.activity.protected_from_sleep}
    };
    const auto groups=TabGroups();
    items.push_back(divider);
    items.push_back({NewTabGroup,L"Add to new tab group...",L"",L"\uE8B7"});
    for(std::size_t i=0;i<groups.size();++i)
        items.push_back({GroupBase+static_cast<UINT>(i),L"Add to group: "+groups[i],L"",L"\uE8B7",true,tab.group==groups[i]});
    if(!tab.group.empty()){
        const bool collapsed=IsGroupCollapsed(tab.group);
        items.push_back(divider);
        items.push_back({ToggleTabGroup,collapsed?L"Expand group":L"Collapse group",L"",collapsed?L"\uE70D":L"\uE70E"});
        items.push_back({RenameTabGroup,L"Rename group...",L"",L"\uE8AC"});
        items.push_back({CloseTabGroup,L"Close group",L"",L"\uE8BB"});
        static constexpr std::array<const wchar_t*,8> color_names{L"Blue",L"Green",L"Yellow",L"Red",L"Purple",L"Cyan",L"Orange",L"Gray"};
        const auto* meta=FindTabGroup(tab.group);
        for(std::size_t i=0;i<color_names.size();++i){
            MenuItem color{GroupColorBase+static_cast<UINT>(i),L"Group color: "+std::wstring(color_names[i]),L"",L"",true,meta && meta->color==i};
            color.swatch_rgb=kProfileColors[i];items.push_back(std::move(color));
        }
        items.push_back({RemoveTabGroup,L"Remove from group",L"",L"\uE711"});
    }
    std::vector<std::string> move_profiles;
    for(const auto& profile:state_.profiles)if(profile.name!=profile_name_){
        move_profiles.push_back(profile.name);items.push_back({MoveProfileBase+static_cast<UINT>(move_profiles.size()-1),L"Move to "+Wide(profile.name)+L" profile",L"",L"\uE77B"});
    }
    items.push_back(divider);
    items.push_back({SuspendNow, L"Sleep tab", L"", L"\uE708", can_sleep});
    items.push_back({DiscardTab, L"Unload tab...", L"", L"\uE74D", background_page});
    items.push_back(divider);
    items.push_back({CloseCurrent, L"Close tab", L"Ctrl+W", L"\uE711"});
    items.push_back({ReopenTab, L"Reopen closed tab", L"Ctrl+Shift+T", L"\uE777", !closed_tabs_.empty()});
    UINT selected = 0;
    {
        menu_open_ = true;
        struct Reset { bool& value; ~Reset() { value = false; } } reset{menu_open_};
        selected = ShowDarkMenu(owner, point, items, dpi_, 330, false, &palette_);
    }
    if (closing_ || window_ != owner || !IsWindow(owner) || !selected || !Find(target_id)) return;
    menu_tab_id_ = target_id;
    struct ResetTarget { std::uint64_t& value; ~ResetTarget() { value = 0; } } reset_target{menu_tab_id_};
    if(selected==NewTabGroup){
        const auto name=AskName(window_,instance_,&palette_,L"Tab group",L"&Group name",L"Give related tabs a short group name.",L"",L"Create");
        if(name && !name->empty())AssignTabGroup(target_id,*name);
    } else if(selected==RemoveTabGroup)AssignTabGroup(target_id,L"");
    else if(selected>=GroupBase && selected<GroupBase+groups.size())AssignTabGroup(target_id,groups[selected-GroupBase]);
    else if(selected>=MoveProfileBase && selected<MoveProfileBase+move_profiles.size()){
        if(auto session=session_.lock())
            if(!session->MoveTabToProfile(this,target_id,move_profiles[selected-MoveProfileBase]))Note(L"This tab could not be moved. Finish active downloads first.");
    } else Dispatch(selected);
}

void BrowserApp::Impl::PreferencesMenu(UINT section) {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    const auto keep_alive = shared_from_this();
    const HWND owner = window_;
    menu_tab_id_ = 0;
    const MenuItem divider{0, L"", L"", L"", false, false, true};
    std::vector<MenuItem> items;
    if (section == PasswordMenu) {
        items = {
            {TogglePasswords, L"Offer to save passwords", L"", L"\uE72E", true, state_.save_passwords},
            divider,
            {ProfileManager, L"Manage profiles", L"\u203A", L"\uE77B"}
        };
    } else if (section == HelpMenu) {
        items = {
            {About, L"About WebCom", L"", L"\uE946"},
            {PrivateHelp, L"About Incognito", L"", L"incognito"},
            divider,
            {InstallRuntime, L"Get or update WebView2 Runtime", L"", L"\uE895"}
        };
    } else {
        items = {
            {AppearanceMenu,L"Appearance",L"\u203A",L"\uE790"},
            {ToggleBookmarks,L"Show bookmarks bar",L"Ctrl+Shift+B",L"\uE734",true,show_bookmarks_},
            divider,
            {ToggleSaver, L"Memory Saver", L"", L"\uE945", true, state_.memory_mode == MemoryMode::Saver},
            {SuspendNow, L"Sleep background tabs now", L"", L"\uE708"},
            {MemoryReport, L"Memory usage...", L"Shift+Esc", L"\uE9D9"},
            divider,
            {TogglePasswords, L"Offer to save passwords", L"", L"\uE72E", true, state_.save_passwords},
            {ClearData, L"Delete browsing data...", L"Ctrl+Shift+Del", L"\uE74D"},
            divider,
            {BookmarkMenu, L"Bookmarks", L"\u203A", L"\uE734"},
            {ProfileManager, L"Manage profiles", L"\u203A", L"\uE77B"},
            divider,
            {InstallRuntime, L"Get or update WebView2 Runtime", L"", L"\uE895"},
            {About, L"About WebCom", L"", L"\uE946"}
        };
    }
    RECT client{}; GetClientRect(owner, &client);
    POINT anchor{client.right, fullscreen_ ? D(8) : D(80)};
    ClientToScreen(owner, &anchor);
    UINT selected = 0;
    {
        menu_open_ = true;
        struct Reset { bool& value; ~Reset() { value = false; } } reset{menu_open_};
        selected = ShowDarkMenu(owner, anchor, items, dpi_, 334, true, &palette_);
    }
    if (!closing_ && window_ == owner && IsWindow(owner) && selected) Dispatch(selected);
}

void BrowserApp::Impl::CreateProfile() {
    if (state_.profiles.size() >= 32) { Notice(L"WebCom supports up to 32 profiles in this version."); return; }
    std::wstring suggestion=L"Work";
    unsigned suffix=2;
    const auto taken=[&](const std::wstring& value){return std::any_of(state_.profiles.begin(),state_.profiles.end(),
        [&](const auto& p){return _wcsicmp(Wide(p.name).c_str(),value.c_str())==0;});};
    while(taken(suggestion))suggestion=L"Profile "+std::to_wstring(suffix++);
    const auto name = AskName(window_,instance_,&palette_,L"Create a WebCom profile",L"&Profile name",
        L"Use up to 40 letters, numbers, spaces, underscores or hyphens.",suggestion.c_str(),L"Create",false,&state_.profiles);
    if (!name) return;
    if (!IsValidProfileName(Utf8(*name))) {
        Notice(L"Use 1 to 40 letters, numbers, spaces, underscores or hyphens. Avoid leading/trailing spaces and Windows device names.");
        return;
    }
    for (const auto& p : state_.profiles) if (_wcsicmp(name->c_str(), Wide(p.name).c_str()) == 0) {
        Notice(L"A profile with that name already exists."); return;
    }
    state_.profiles.push_back({Utf8(*name), {{"webcom:newtab", "New tab", false, false}}, 0});
    Dirty();
    SwitchProfile(state_.profiles.size() - 1);
}

void BrowserApp::Impl::SwitchProfile(std::size_t index) {
    if(index>=state_.profiles.size())return;
    // Profiles are separate native windows. Their live tabs and downloads remain
    // running while another profile is opened or brought to the foreground.
    if(auto session=session_.lock())session->OpenProfile(state_.profiles[index].name,L"",false);
}

void BrowserApp::Impl::DeleteCurrentProfile() {
    if(closing_ || delete_profile_pending_) return;
    if(state_.profiles.size()<=1){Notice(L"WebCom must keep at least one profile. Create another profile before deleting this one.");return;}
    for(auto& tab:tabs_)if(HasDownload(*tab)){Notice(L"Finish or cancel active downloads before deleting this profile.");return;}
    const std::wstring message=L"Delete the '"+Wide(profile_name_)+L"' profile?\n\nThis permanently removes its cookies, saved passwords, site storage, history, bookmarks, permissions, appearance settings and saved tabs. Downloaded files on your PC are not deleted.\n\nThis cannot be undone.";
    if(MessageBoxW(window_,message.c_str(),L"Delete WebCom profile",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return;
    if(closing_)return;
    delete_profile_pending_=true;
    for(auto& candidate:tabs_)if(!candidate->private_mode && candidate->web){DeleteCurrentProfileNow(*candidate);return;}
    // A profile containing only native WebCom pages has no CoreWebView2Profile
    // object yet. Create an invisible normal about:blank view so WebView2 can
    // mark the real profile directory for deletion using its supported API.
    const auto id=Add(L"about:blank",false,false);
    auto* temporary=Find(id);
    if(!temporary){delete_profile_pending_=false;Notice(L"WebCom could not prepare this profile for deletion.");return;}
    Ensure(*temporary);
    if(!environment_)BeginEnvironment();
    Note(L"Preparing profile deletion…");
}

void BrowserApp::Impl::DeleteCurrentProfileNow(Tab& source) {
    if(!delete_profile_pending_ || closing_ || !source.web || source.private_mode)return;
    ComPtr<ICoreWebView2_13> access;
    ComPtr<ICoreWebView2Profile> profile;
    ComPtr<ICoreWebView2Profile8> deletable;
    HRESULT result=source.web.As(&access);
    if(SUCCEEDED(result))result=access->get_Profile(&profile);
    if(SUCCEEDED(result))result=profile.As(&deletable);
    if(FAILED(result) || !deletable){
        delete_profile_pending_=false;
        Notice(L"This WebView2 Runtime cannot delete browser profiles. Update the runtime and try again.");
        return;
    }
    result=deletable->Delete();
    if(FAILED(result)){
        delete_profile_pending_=false;
        Notice(L"The profile could not be deleted. Close other WebCom windows using this profile and try again. Error "+HResultText(result));
        return;
    }
    if(auto session=session_.lock())session->FinalizeProfileDeletion(this);
}

void BrowserApp::Impl::ReportMemory() {
    std::set<DWORD> pids{GetCurrentProcessId()};
    bool runtime_list_available = !environment_;
    if (environment_) {
        ComPtr<ICoreWebView2Environment8> environment;
        ComPtr<ICoreWebView2ProcessInfoCollection> processes;
        if (SUCCEEDED(environment_.As(&environment)) && SUCCEEDED(environment->GetProcessInfos(&processes))) {
            UINT count = 0;
            if (SUCCEEDED(processes->get_Count(&count))) {
                runtime_list_available = true;
                for (UINT i = 0; i < count; ++i) {
                    ComPtr<ICoreWebView2ProcessInfo> info;
                    INT32 pid = 0;
                    if (SUCCEEDED(processes->GetValueAtIndex(i, &info)) && SUCCEEDED(info->get_ProcessId(&pid)) && pid > 0)
                        pids.insert(static_cast<DWORD>(pid));
                }
            }
        }
    }
    std::uint64_t working = 0, committed = 0;
    unsigned measured = 0;
    for (DWORD pid : pids) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) continue;
        PROCESS_MEMORY_COUNTERS_EX info{};
        info.cb = sizeof(info);
        if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info), sizeof(info))) {
            working += info.WorkingSetSize; committed += info.PrivateUsage; ++measured;
        }
        CloseHandle(process);
    }
    std::wostringstream body;
    body << std::fixed << std::setprecision(1)
         << L"WebCom memory snapshot\n\n"
         << L"Working set: " << static_cast<double>(working) / 1048576.0 << L" MiB\n"
         << L"Private committed: " << static_cast<double>(committed) / 1048576.0 << L" MiB\n"
         << L"Processes measured: " << measured << L" of " << pids.size() << L"\n\n";
    if (!runtime_list_available)
        body << L"PARTIAL RESULT: the runtime process list was unavailable. Do not treat the host-only values as the browser total.\n\n";
    if (measured != pids.size()) body << L"Some processes exited or could not be read. This snapshot is incomplete.\n\n";
    body << L"Includes WebCom.exe and the WebView2 process group for this app, except the crash reporter.\n\n"
         << L"Working set is resident memory and may count shared pages more than once. Private committed includes memory that may be paged out; it is not physical RAM usage.\n\n"
         << L"Compare the same sites and tab states in Chrome. No percentage saving has been established.";
    MessageBoxW(window_, body.str().c_str(), L"WebCom - Memory usage", MB_OK | MB_ICONINFORMATION);
}

void BrowserApp::Impl::ClearBrowsingData() {
    auto* tab = Active();
    if (!tab || !tab->web || tab->private_mode) {
        tab = nullptr;
        for (auto& candidate : tabs_)
            if (!candidate->private_mode && candidate->web) { tab = candidate.get(); break; }
    }
    if (!tab) {
        Notice(L"Open a normal webpage in this profile once, then use this command again."); return;
    }
    ComPtr<ICoreWebView2_13> access;
    ComPtr<ICoreWebView2Profile> profile;
    ComPtr<ICoreWebView2Profile2> clear;
    if (FAILED(tab->web.As(&access)) || FAILED(access->get_Profile(&profile)) || FAILED(profile.As(&clear))) {
        Notice(L"Update the WebView2 Runtime to clear profile data from this menu."); return;
    }
    const auto question = L"Clear all browsing data for profile \"" + Wide(profile_name_) +
        L"\"?\n\nCookies, cached pages, site storage and saved passwords will be removed. Websites may sign you out. Downloaded files remain. This cannot be undone.";
    if (MessageBoxW(window_, question.c_str(), L"Clear profile data",
        MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES || closing_) return;
    const auto weak = weak_from_this();
    const HRESULT hr = clear->ClearBrowsingDataAll(MakeCallback<ICoreWebView2ClearBrowsingDataCompletedHandler>(
        [weak](HRESULT error) -> HRESULT {
            if (auto app = weak.lock(); app && !app->closing_) {
                if (SUCCEEDED(error)) {
                    app->site_permissions_.clear();
                    app->SaveSitePermissionsNow();
                    app->Notice(L"Profile browsing data and site permissions were cleared. Reload open pages when you are ready.");
                } else app->Notice(L"Some browsing data could not be cleared. Close busy pages and try again.");
            }
            return S_OK;
        }).Get());
    if (FAILED(hr)) Notice(L"Could not clear profile data. " + HResultText(hr));
}

void BrowserApp::Impl::ToggleFullscreen() {
    if (!fullscreen_) {
        saved_style_ = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE));
        placement_.length=sizeof(placement_);
        GetWindowPlacement(window_, &placement_);
        MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
        GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor);
        SetWindowLongPtrW(window_, GWL_STYLE, saved_style_ & ~WS_OVERLAPPEDWINDOW);
        fullscreen_ = true;
        SetWindowPos(window_, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    } else {
        fullscreen_ = false;
        SetWindowLongPtrW(window_, GWL_STYLE, saved_style_);
        SetWindowPlacement(window_, &placement_);
        SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    Layout();
}

void BrowserApp::Impl::Dispatch(UINT command, LPARAM extra) {
    if (closing_) return;
    Tab* tab = menu_tab_id_ ? Find(menu_tab_id_) : Active();
    if(command>=ThemeBase && command<ThemeBase+4){
        auto next=appearance_;next.theme=static_cast<BrowserTheme>(command-ThemeBase);CommitAppearance(next);return;
    }
    if(command>=ColorBase && command<ColorBase+kProfileColors.size()){
        auto next=appearance_;next.accent_rgb=kProfileColors[command-ColorBase];CommitAppearance(next);return;
    }
    if(command>=GroupColorBase && command<GroupColorBase+kProfileColors.size()){
        if(tab && !tab->group.empty())SetGroupColor(tab->group,static_cast<std::uint8_t>(command-GroupColorBase));
        return;
    }
    if(command>=DeleteHistoryBase && command<DeleteHistoryBase+history_entries_.size()){
        const auto index=static_cast<std::size_t>(command-DeleteHistoryBase);
        history_entries_.erase(history_entries_.begin()+static_cast<std::ptrdiff_t>(index));
        SaveHistory();native_scroll_=0;QueueUI();Note(L"History entry removed.");return;
    }
    if(command>=HistoryBase && command<HistoryBase+history_entries_.size()){
        Add(history_entries_[command-HistoryBase].url, true, false); return;
    }
    if(command>=PauseDownloadBase && command<PauseDownloadBase+download_entries_.size()){
        ToggleDownloadPause(static_cast<std::size_t>(command-PauseDownloadBase));return;
    }
    if(command>=CancelDownloadBase && command<CancelDownloadBase+download_entries_.size()){
        CancelDownload(static_cast<std::size_t>(command-CancelDownloadBase));return;
    }
    if(command>=RetryDownloadBase && command<RetryDownloadBase+download_entries_.size()){
        RetryDownload(static_cast<std::size_t>(command-RetryDownloadBase));return;
    }
    if(command>=DownloadBase && command<DownloadBase+download_entries_.size()){
        auto& item=*download_entries_[command-DownloadBase];
        RefreshDownload(item);
        if(item.state==COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED && !item.path.empty())
            ShellExecuteW(window_, L"open", item.path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        else Note(item.state==COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS ? L"This download is still in progress." : L"This download is not available to open.");
        return;
    }
    if (command >= ProfileBase && command < ProfileBase + state_.profiles.size()) {
        SwitchProfile(command - ProfileBase); return;
    }
    if (command >= SelectTabBase && command <= SelectTabBase + 8) {
        const auto index = command - SelectTabBase;
        if (index == 8 && !tabs_.empty()) Select(tabs_.size() - 1);
        else if (index < tabs_.size()) Select(index);
        return;
    }
    if (command >= RemoveBookmarkBase && command < RemoveBookmarkBase + bookmarks_.size()) {
        const auto index=static_cast<std::size_t>(command-RemoveBookmarkBase);
        auto next=bookmarks_; next.erase(next.begin()+static_cast<std::ptrdiff_t>(index));
        std::string error;
        if(!SaveBookmarks(root_,profile_name_,next,&error)){Notice(L"The bookmark could not be removed. "+Wide(error));return;}
        bookmarks_=std::move(next);RebuildBookmarks();native_scroll_=0;QueueUI();Note(L"Bookmark removed.");return;
    }
    if (command >= BookmarkBarFolderBase && command < BookmarkBarFolderBase + bookmark_folders_.size()) {
        const auto keep_alive=shared_from_this();
        const HWND owner=window_;
        const auto folder_index=static_cast<std::size_t>(command-BookmarkBarFolderBase);
        const std::wstring folder=bookmark_folders_[folder_index];
        std::vector<MenuItem> items;
        const std::wstring prefix=folder+L"/";
        for(std::size_t i=0;i<bookmarks_.size();++i) {
            const auto saved_folder=Wide(bookmarks_[i].folder);
            if(saved_folder!=folder && !saved_folder.starts_with(prefix)) continue;
            std::wstring detail=Wide(bookmarks_[i].url);
            if(saved_folder!=folder) detail=saved_folder.substr(prefix.size())+L"  •  "+detail;
            items.push_back({BookmarkBase+static_cast<UINT>(i),Wide(bookmarks_[i].title),detail,L"\uE774"});
        }
        if(items.empty()) return;
        RECT r{}; if(HWND button=GetDlgItem(owner,command)) GetWindowRect(button,&r);
        const POINT anchor{r.left,r.bottom};
        UINT selected=0;
        {
            menu_open_=true;
            struct Reset{bool& value;~Reset(){value=false;}}reset{menu_open_};
            selected=ShowDarkMenu(owner,anchor,items,dpi_,340,false,&palette_);
        }
        if(closing_ || window_!=owner || !IsWindow(owner)) return;
        if(selected>=BookmarkBase && selected<BookmarkBase+bookmarks_.size())
            Navigate(Wide(bookmarks_[selected-BookmarkBase].url));
        return;
    }
    if (command >= BookmarkBase && command < BookmarkBase + bookmarks_.size()) {
        Navigate(Wide(bookmarks_[command - BookmarkBase].url)); return;
    }
    if (command >= OpenTabBase && command < OpenTabBase + tabs_.size()) {
        Select(command - OpenTabBase); return;
    }
    switch (command) {
    case AppearanceMenu: case ThemeMenu: case ProfileColorMenu: AppearanceOptions(command);break;
    case ToggleBookmarks:{auto next=appearance_;next.show_bookmarks=!show_bookmarks_;CommitAppearance(next);break;}
    case ColorAutomatic:{auto next=appearance_;next.accent_rgb.reset();CommitAppearance(next);break;}
    case AppearanceReset:CommitAppearance(Appearance{});break;
    case ColorCustom:{
        const auto initial=Wide(FormatHexColor(ProfileColor(appearance_,profile_name_)));
        const auto entered=AskName(window_,instance_,&palette_,L"Profile color",L"&Color",
            L"Enter a six-digit RGB color. The profile badge uses your color; keyboard focus keeps readable contrast.",
            initial.c_str(),L"Apply",true);
        if(!closing_ && entered)if(auto color=ParseHexColor(Utf8(*entered))){auto next=appearance_;next.accent_rgb=*color;CommitAppearance(next);}
        break;
    }
    case NewProfileWindow: ProfileMenu(); break;
    case MoveTabLeft: case MoveTabRight: if(tab)MoveTab(tab->id,command==MoveTabLeft?-1:1);break;
    case ToggleTabGroup: if(tab && !tab->group.empty())ToggleTabGroupState(tab->group);break;
    case RenameTabGroup: if(tab && !tab->group.empty())RenameGroup(tab->group);break;
    case CloseTabGroup: if(tab && !tab->group.empty())CloseGroup(tab->group);break;
    case MuteTab:
        if(tab && !IsHome(tab->url)){
            if(tab->web){
                ComPtr<ICoreWebView2_8> audio;
                if(FAILED(tab->web.As(&audio)) || FAILED(audio->put_IsMuted(tab->muted?FALSE:TRUE))){
                    Note(L"This runtime could not change tab audio.");break;
                }
            }
            tab->muted=!tab->muted;UpdateUI();
        }
        break;
    case TabList: OpenTabsMenu(); break;
    case History: OpenInternalPage(L"webcom:history"); break;
    case BookmarkManager: OpenInternalPage(L"webcom:bookmarks"); break;
    case ImportBookmarks: ImportBookmarksFromFile(); break;
    case ExportBookmarks: ExportBookmarksToFile(); break;
    case ClearHistory: history_entries_.clear(); SaveHistory(); native_scroll_=0; QueueUI(); Note(L"Browsing history cleared."); break;
    case HistoryDeleteMatches: {
        const std::wstring query = native_filter_ ? ControlText(native_filter_) : L"";
        const auto removed = DeleteHistoryMatches(history_entries_, query);
        if (removed) { SaveHistory(); native_scroll_=0; QueueUI(); Note(L"Deleted " + std::to_wstring(removed) + L" matching history entr" + (removed==1?L"y.":L"ies.")); }
        else Note(L"No matching history entries to delete.");
        break;
    }
    case HistoryDeleteWeek: {
        const auto now = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        const auto span = static_cast<std::uint64_t>(7ull*24ull*60ull*60ull*1000ull);
        const auto removed = DeleteHistoryRange(history_entries_, now>span?now-span:0, now);
        if (removed) { SaveHistory(); native_scroll_=0; QueueUI(); Note(L"Deleted history from the last 7 days."); } else Note(L"No history from the last 7 days.");
        break;
    }
    case HistoryDeleteOlderMonth: {
        const auto now = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        const auto span = static_cast<std::uint64_t>(30ull*24ull*60ull*60ull*1000ull);
        const auto removed = DeleteHistoryOlderThan(history_entries_, now>span?now-span:0);
        if (removed) { SaveHistory(); native_scroll_=0; QueueUI(); Note(L"Deleted history older than 30 days."); } else Note(L"No history older than 30 days.");
        break;
    }
    case ClearDownloadsList: ClearDownloads(); native_scroll_=0; Note(L"Completed download entries cleared."); break;
    case OpenDownloadsFolder: {
        PWSTR folder=nullptr;
        if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads,0,nullptr,&folder))){
            ShellExecuteW(window_,L"open",folder,nullptr,nullptr,SW_SHOWNORMAL);CoTaskMemFree(folder);
        } else Note(L"The Downloads folder could not be opened.");
        break;
    }
    case MinimizeWindow: ShowWindow(window_, SW_MINIMIZE); break;
    case MaximizeWindow: ShowWindow(window_, IsZoomed(window_) ? SW_RESTORE : SW_MAXIMIZE); break;
    case CloseWindow: PostMessageW(window_, WM_CLOSE, 0, 0); break;
    case NewTab: Add(); FocusOmnibox(); break;
    case PrivateTab: Add(L"webcom:newtab", true, true); FocusOmnibox(); break;
    case FocusAddress: FocusOmnibox(); break;
    case Home: Navigate(L"webcom:newtab"); break;
    case Back: if (tab && tab->web) tab->web->GoBack(); break;
    case Forward: if (tab && tab->web) tab->web->GoForward(); break;
    case Reload:
        if (!tab) break;
        if (tab->web) {
            if (tab->activity.loading) tab->web->Stop(); else tab->web->Reload();
        } else { environment_failed_ = false; tab->failed = false; Ensure(*tab); UpdateUI(); }
        break;
    case HomeGo:
        if(tab && tab->failed && !IsHome(tab->url))Dispatch(Reload);
        else Navigate(ControlText(home_search_));
        break;
    case AiMode: Navigate(L"https://www.google.com/aimode"); break;
    case SiteInfo: SiteInfoMenu(); break;
    case DownloadsQuick: DownloadQuickMenu(); break;
    case BookmarkPage: BookmarkCurrent(); break;
    case BookmarkMenu: case BookmarksOverflow: {
        RECT r{}; GetWindowRect(GetDlgItem(window_, command == BookmarksOverflow ? BookmarksOverflow : More), &r);
        BookmarksMenu({r.right, r.bottom}); break;
    }
    case PasswordMenu: case HelpMenu: PreferencesMenu(command); break;
    case SettingsMenu: OpenInternalPage(L"webcom:settings"); break;
    case PrivateHelp:
        Notice(L"Incognito tabs use a separate in-private WebView2 profile and are not saved in WebCom's restored session.\n\n"
            L"Closing private tabs does not delete downloaded files or bookmarks you saved. Your network provider, workplace and websites may still see your activity.");
        break;
    case ZoomIn: case ZoomOut: case ZoomReset:
        if (tab && tab->controller) {
            double zoom = 1.0; tab->controller->get_ZoomFactor(&zoom);
            constexpr double levels[] = {0.25,0.33,0.50,0.67,0.75,0.80,0.90,1.0,1.10,1.25,1.50,1.75,2.0,2.5,3.0,4.0,5.0};
            double next = 1.0;
            if (command == ZoomIn) {
                next = 5.0;
                for (double level : levels) if (level > zoom + 0.005) { next = level; break; }
            } else if (command == ZoomOut) {
                next = 0.25;
                for (auto it = std::rbegin(levels); it != std::rend(levels); ++it)
                    if (*it < zoom - 0.005) { next = *it; break; }
            }
            tab->controller->put_ZoomFactor(next);
        }
        break;
    case PrintPage:
        if (tab && tab->web) {
            ComPtr<ICoreWebView2_16> printing;
            if (SUCCEEDED(tab->web.As(&printing))) printing->ShowPrintUI(COREWEBVIEW2_PRINT_DIALOG_KIND_BROWSER);
            else Notice(L"Update the WebView2 Runtime to use print preview.");
        }
        break;
    case SearchInPage:
        if (tab && !tab->web && (tab->url==L"webcom:history" || tab->url==L"webcom:bookmarks")) {
            SetFocus(native_filter_); SendMessageW(native_filter_,EM_SETSEL,0,-1); break;
        }
        if (tab && tab->web && environment_) {
            ComPtr<ICoreWebView2_28> access;
            ComPtr<ICoreWebView2Find> find;
            ComPtr<ICoreWebView2Environment15> find_environment;
            ComPtr<ICoreWebView2FindOptions> options;
            if (FAILED(tab->web.As(&access)) || FAILED(access->get_Find(&find)) ||
                FAILED(environment_.As(&find_environment)) || FAILED(find_environment->CreateFindOptions(&options))) {
                Notice(L"Update the WebView2 Runtime to open Find from this menu. Ctrl+F also works when the page has focus.");
                break;
            }
            options->put_FindTerm(L"");
            options->put_SuppressDefaultFindDialog(FALSE);
            options->put_ShouldHighlightAllMatches(TRUE);
            if (tab->controller) tab->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            const auto weak = weak_from_this();
            const HRESULT result = find->Start(options.Get(), MakeCallback<ICoreWebView2FindStartCompletedHandler>(
                [weak](HRESULT error) -> HRESULT {
                    if (auto app = weak.lock(); app && !app->closing_ && FAILED(error)) app->Note(L"Find could not be opened for this document.");
                    return S_OK;
                }).Get());
            if (FAILED(result)) Note(L"Find could not be opened for this document.");
        }
        break;
    case DeveloperTools: if (tab && tab->web) tab->web->OpenDevToolsWindow(); break;
    case Google: Navigate(L"https://www.google.com/"); break;
    case YouTube: Navigate(L"https://www.youtube.com/"); break;
    case GitHub: Navigate(L"https://github.com/"); break;
    case CloseCurrent: if (tab) Close(tab->id); break;
    case ReopenTab: Reopen(); break;
    case Profile: case ProfilesMenu: ProfileMenu(); break;
    case ProfileManager: OpenInternalPage(L"webcom:profiles"); break;
    case OpenAllProfiles: if(auto session=session_.lock())session->OpenAllProfiles(this); break;
    case CloseOtherProfiles: if(auto session=session_.lock())session->CloseOtherProfileWindows(this); break;
    case AddProfile: CreateProfile(); break;
    case DeleteCurrentProfileCmd: DeleteCurrentProfile(); break;
    case ShowSiteCookiesCmd: SiteCookiesMenu(); break;
    case ClearSiteCookiesCmd: ClearSiteCookies(); break;
    case Saver: case ToggleSaver:
        state_.memory_mode = state_.memory_mode == MemoryMode::Saver ? MemoryMode::Balanced : MemoryMode::Saver;
        if(auto session=session_.lock())session->ApplyPreferences();
        Note(state_.memory_mode == MemoryMode::Saver ? L"Memory Saver on. Eligible background tabs sleep after 1 minute."
                                                   : L"Balanced mode. Eligible background tabs sleep after 5 minutes.");
        Dirty(); Tick(); UpdateUI(); break;
    case More: MainMenu(); break;
    case PinTab: case KeepAwake:
        if (tab) {
            if (command == PinTab) {
                const auto id=tab->id;
                tab->activity.pinned = !tab->activity.pinned;
                std::size_t pinned_count=0;
                for(const auto& item:tabs_) if(item->id!=id && item->activity.pinned) ++pinned_count;
                const auto found=std::find_if(tabs_.begin(),tabs_.end(),[&](const auto& item){return item->id==id;});
                if(found!=tabs_.end()) {
                    const std::size_t index=static_cast<std::size_t>(found-tabs_.begin());
                    const std::size_t target=tab->activity.pinned?pinned_count:std::min(pinned_count,tabs_.size()-1);
                    if(index!=target) MoveTabTo(id,target);
                }
            } else tab->activity.protected_from_sleep = !tab->activity.protected_from_sleep;
            if (tab->web && (tab->activity.pinned || tab->activity.protected_from_sleep)) {
                ComPtr<ICoreWebView2_3> lifecycle;
                if (SUCCEEDED(tab->web.As(&lifecycle))) lifecycle->Resume();
                tab->activity.suspended = false;
                LowMemoryTarget(*tab, false);
            }
            Dirty(); UpdateUI();
        }
        break;
    case SuspendNow:
        if (menu_tab_id_) { if (tab) Suspend(*tab, true); }
        else for (auto& t : tabs_) Suspend(*t, true);
        break;
    case DiscardTab:
        if (tab && tab->web && !IsActive(*tab)) {
            if (HasDownload(*tab)) { Notice(L"Finish this tab's download before unloading it."); break; }
            const auto id = tab->id;
            if (MessageBoxW(window_, L"Unload this tab? It will reload when selected. Unsaved text, playback position and page state will be lost.",
                L"Unload tab", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES) {
                tab = Find(id);
                if (tab && !IsActive(*tab) && !HasDownload(*tab) && !closing_) {
                    Release(*tab); tab->restore_pending = true; UpdateUI();
                }
            }
        }
        break;
    case DuplicateTab: if (tab) Add(tab->url, true, tab->private_mode); break;
    case MemoryReport: ReportMemory(); break;
    case Downloads: OpenInternalPage(L"webcom:downloads"); break;
    case TogglePasswords:
        state_.save_passwords = !state_.save_passwords;
        if(auto session=session_.lock())session->ApplyPreferences();
        Note(state_.save_passwords ? L"Password-save prompts are on for normal profiles."
            : L"New password-save prompts are off. Existing saved passwords may still autofill.");
        Dirty(); UpdateUI(); break;
    case ClearData: ClearBrowsingData(); break;
    case OpenInDefault:
        if (tab && IsWebAddress(tab->url)) ShellExecuteW(window_, L"open", tab->url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        else Note(L"Open a website first.");
        break;
    case InstallRuntime:
        ShellExecuteW(window_, L"open", L"https://developer.microsoft.com/microsoft-edge/webview2/#download-section",
                      nullptr, nullptr, SW_SHOWNORMAL); break;
    case NextTab: if (!tabs_.empty()) Select((active_ + 1) % tabs_.size()); break;
    case PreviousTab: if (!tabs_.empty()) Select((active_ + tabs_.size() - 1) % tabs_.size()); break;
    case FullScreen:
        if (fullscreen_ && tab && tab->web)
            tab->web->ExecuteScript(L"if (document.fullscreenElement) document.exitFullscreen();", nullptr);
        ToggleFullscreen(); break;
    case EngineCrashed:
        if(auto session=session_.lock())session->InvalidateEnvironment(static_cast<std::uint64_t>(extra));
        break;
    case TabCrashed:
        if (auto* crashed = Find(static_cast<std::uint64_t>(extra))) {
            Release(*crashed); crashed->failed = true;
            crashed->error = L"This page stopped unexpectedly. Press Reload to recover it.";
            UpdateUI();
        }
        break;
    case About:
        MessageBoxW(window_,
            L"WebCom for Windows 0.5.1 preview\n\nNative C++20 / Win32 with Microsoft Edge WebView2.\n\n"
            L"Simultaneous profile windows, site permission controls, recent-download bubble, searchable tabs, audio controls and sleeping tabs.\n\n"
            L"Google may block account sign-in in WebView2. Chrome Sync is not available.\n\n"
            L"Memory savings depend on pages, tabs and runtime version. This preview has no established RAM-saving percentage.\n\n"
            L"Use the profile menu for local profiles and Shift+Esc for process memory.",
            L"About WebCom", MB_OK | MB_ICONINFORMATION);
        break;
    case Exit: PostMessageW(window_, WM_CLOSE, 0, 0); break;
    default: break;
    }
}

UINT BrowserApp::Impl::Shortcut(UINT key, bool ctrl, bool shift, bool alt) {
    if (ctrl && !alt) {
        if (key >= '1' && key <= '9') return SelectTabBase + key - '1';
        if (key == 'T') return shift ? ReopenTab : NewTab;
        if (key == 'N') return shift ? PrivateTab : NewProfileWindow;
        if (key == 'A' && shift) return TabList;
        if (key == 'H' && !shift) return History;
        if (key == 'B' && shift) return ToggleBookmarks;
        if (key == 'W' || key == VK_F4) return shift ? Exit : CloseCurrent;
        if (key == 'L' || key == 'K' || key == 'E') return FocusAddress;
        if (key == 'R') return Reload;
        if (key == 'J') return Downloads;
        if (key == 'D') return BookmarkPage;
        if (key == 'O' && shift) return BookmarkManager;
        if (key == 'P') return shift ? ProfileManager : PrintPage;
        if (key == VK_OEM_PLUS || key == VK_ADD) return ZoomIn;
        if (key == VK_OEM_MINUS || key == VK_SUBTRACT) return ZoomOut;
        if (key == '0' || key == VK_NUMPAD0) return ZoomReset;
        if (shift && key == VK_DELETE) return ClearData;
        if (key == VK_TAB) return shift ? PreviousTab : NextTab;
        if (key == VK_NEXT) return shift ? MoveTabRight : NextTab;
        if (key == VK_PRIOR) return shift ? MoveTabLeft : PreviousTab;
    }
    if (alt && !ctrl) {
        if (key == VK_LEFT) return Back;
        if (key == VK_RIGHT) return Forward;
        if (key == 'D') return FocusAddress;
        if (key == VK_HOME) return Home;
        if (key == 'F' || key == 'E') return More;
    }
    if (!ctrl && !alt) {
        if (key == VK_F5) return Reload;
        if (key == VK_F6) return FocusAddress;
        if (key == VK_F11) return FullScreen;
        if (key == VK_F12) return DeveloperTools;
        if (key == VK_ESCAPE && shift) return MemoryReport;
        if (key == VK_ESCAPE && fullscreen_) return FullScreen;
    }
    return 0;
}

bool BrowserApp::Impl::HandleKeyMessage(const MSG& message) {
    if (message.message != WM_KEYDOWN && message.message != WM_SYSKEYDOWN) return false;
    if(message.wParam==VK_ESCAPE && (dragged_tab_id_ || pressed_close_tab_id_)){
        CancelTabDrag();return true;
    }
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    const UINT command = Shortcut(static_cast<UINT>(message.wParam), ctrl, shift, alt);
    if (command) { Dispatch(command); return true; }
    const bool native_control = message.hwnd == window_ || message.hwnd == address_ ||
        message.hwnd == home_search_ || message.hwnd == tab_strip_ ||
        std::find(toolbar_.begin(), toolbar_.end(), message.hwnd) != toolbar_.end() ||
        std::find(bookmark_buttons_.begin(), bookmark_buttons_.end(), message.hwnd) != bookmark_buttons_.end() ||
        std::find(home_buttons_.begin(), home_buttons_.end(), message.hwnd) != home_buttons_.end();
    if (native_control && ctrl && !alt && message.wParam == 'F') { Dispatch(SearchInPage); return true; }
    if (native_control && !ctrl && !alt && (message.wParam == VK_NEXT || message.wParam == VK_PRIOR) &&
        (ScrollPrivate(message.wParam == VK_NEXT ? D(160) : -D(160)) ||
         ScrollInternal(message.wParam == VK_NEXT ? D(160) : -D(160)))) return true;
    if (message.wParam == VK_TAB && !ctrl && !alt && native_control) {
        HWND next = GetNextDlgTabItem(window_, GetFocus(), shift);
        if (next) { SetFocus(next); return true; }
    }
    return false;
}

LRESULT CALLBACK BrowserApp::Impl::WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    Impl* app = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    try { const auto keep_alive=app->weak_from_this().lock(); return app->Message(message, wparam, lparam); }
    catch (...) {
        // C++ exceptions cannot unwind through the Win32 callback ABI.
        if (message == WM_NCCREATE) return FALSE;
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

LRESULT CALLBACK BrowserApp::Impl::EditProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                           UINT_PTR, DWORD_PTR context) {
    auto* app = reinterpret_cast<Impl*>(context);
    if (!app) return DefSubclassProc(window, message, wparam, lparam);
    const auto keep_alive = app->weak_from_this().lock();
    if (!keep_alive) return DefSubclassProc(window, message, wparam, lparam);
    if(message==WM_LBUTTONDOWN && window==app->address_ && GetFocus()!=window){
        SetFocus(window);SendMessageW(window,EM_SETSEL,0,-1);return 0;
    }
    if(message==WM_KEYDOWN && wparam=='A' && (GetKeyState(VK_CONTROL)&0x8000)){
        SendMessageW(window,EM_SETSEL,0,-1);return 0;
    }
    if(message==WM_KEYDOWN && window==app->address_ && IsWindowVisible(app->suggestion_panel_)) {
        if(wparam==VK_DOWN || wparam==VK_UP) {
            const int count=static_cast<int>(app->omnibox_suggestions_.size());
            if(count>0) {
                if(wparam==VK_DOWN) app->omnibox_selected_=std::min(count-1,app->omnibox_selected_+1);
                else app->omnibox_selected_=app->omnibox_selected_<0?count-1:std::max(0,app->omnibox_selected_-1);
                app->omnibox_hot_=app->omnibox_selected_;
                InvalidateRect(app->suggestion_panel_,nullptr,FALSE);
            }
            return 0;
        }
        if(wparam==VK_RETURN && app->omnibox_selected_>=0) {
            app->ApplyOmniboxSuggestion(static_cast<std::size_t>(app->omnibox_selected_)); return 0;
        }
        if(wparam==VK_ESCAPE) { app->HideOmniboxSuggestions(); return 0; }
    }
    if(message==WM_CHAR && wparam==1 && (GetKeyState(VK_CONTROL)&0x8000))return 0;
    if (message == WM_MOUSEWHEEL && app->ScrollPrivate(-GET_WHEEL_DELTA_WPARAM(wparam) * app->D(48) / WHEEL_DELTA)) return 0;
    if (message == WM_PAINT) {
        const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
        if (!GetWindowTextLengthW(window) && (window == app->home_search_ || GetFocus() != window)) {
            HDC dc = GetDC(window); RECT r{}; GetClientRect(window, &r);
            Text(dc, L"Search Google or type a URL", r, app->font_, app->kMuted);
            ReleaseDC(window, dc);
        }
        return result;
    }
    if (message == WM_KEYDOWN && wparam == VK_RETURN) {
        app->Navigate(ControlText(window));
        return 0;
    }
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
        if (auto* tab = app->Active()) {
            SetWindowTextW(window, window == app->address_ && !IsHome(tab->url) ? tab->url.c_str() : L"");
            app->address_editing_ = false;
            if (tab->controller) tab->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            app->Layout();
        }
        return 0;
    }
    if (message == WM_CHAR && (wparam == VK_RETURN || wparam == VK_ESCAPE)) return 0;
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, EditProc, 1);
    return DefSubclassProc(window, message, wparam, lparam);
}

LRESULT CALLBACK BrowserApp::Impl::SuggestionProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR, DWORD_PTR context) {
    auto* app=reinterpret_cast<Impl*>(context);
    if(!app)return DefSubclassProc(window,message,wparam,lparam);
    const auto keep_alive=app->weak_from_this().lock();
    if(!keep_alive)return DefSubclassProc(window,message,wparam,lparam);
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT) {
        PAINTSTRUCT ps{};HDC output=BeginPaint(window,&ps);RECT bounds{};GetClientRect(window,&bounds);
        HDC buffered=app->native_chrome_.suggestions.Begin(output,bounds.right,bounds.bottom);
        HDC dc=buffered?buffered:output;
        if(buffered)IntersectClipRect(dc,ps.rcPaint.left,ps.rcPaint.top,ps.rcPaint.right,ps.rcPaint.bottom);
        Fill(dc,bounds,app->kCaption);
        Rounded(dc,{0,0,bounds.right,bounds.bottom},app->kSurface,app->kBorder,app->D(14));
        const int row=app->D(52),top=app->D(5);
        for(std::size_t i=0;i<app->omnibox_suggestions_.size();++i) {
            RECT r{app->D(5),top+static_cast<int>(i)*row,bounds.right-app->D(5),top+static_cast<int>(i+1)*row};
            unsigned hover_amount=static_cast<int>(i)==app->omnibox_hot_?100u:0u;
            if(app->omnibox_hover_start_)hover_amount=ChromeHoverAmount(static_cast<int>(i),app->omnibox_hover_from_,app->omnibox_hover_to_,
                GetTickCount64()-app->omnibox_hover_start_);
            if(static_cast<int>(i)==app->omnibox_selected_)hover_amount=100;
            if(hover_amount){
                const auto fill=MixChromeColor(app->kSurface,app->kHover,std::max(1u,hover_amount*70/100));
                Rounded(dc,r,fill,fill,app->D(10));
            }
            const auto& item=app->omnibox_suggestions_[i];
            Glyph glyph=Glyph::Search;
            if(item.kind==OmniboxSuggestionKind::OpenTab)glyph=Glyph::Globe;
            else if(item.kind==OmniboxSuggestionKind::Bookmark)glyph=Glyph::Star;
            else if(item.kind==OmniboxSuggestionKind::History)glyph=Glyph::Reload;
            Icon(dc,{r.left+app->D(12),r.top+app->D(15),r.left+app->D(32),r.top+app->D(35)},glyph,
                 item.kind==OmniboxSuggestionKind::Search?app->kAccent:app->kMuted);
            Text(dc,item.title,{r.left+app->D(44),r.top+app->D(5),r.right-app->D(12),r.top+app->D(27)},
                 app->font_,app->kInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
            Text(dc,item.detail,{r.left+app->D(44),r.top+app->D(27),r.right-app->D(12),r.bottom-app->D(4)},
                 app->small_font_,app->kMuted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        }
        if(buffered)app->native_chrome_.suggestions.Present(output,ps.rcPaint);
        EndPaint(window,&ps);return 0;
    }
    if(message==WM_MOUSEMOVE) {
        const int y=GET_Y_LPARAM(lparam)-app->D(5),row=app->D(52);
        const int index=y>=0?y/std::max(1,row):-1;
        const int next=index>=0 && static_cast<std::size_t>(index)<app->omnibox_suggestions_.size()?index:-1;
        if(next!=app->omnibox_hot_){
            app->omnibox_hover_from_=app->omnibox_hot_;app->omnibox_hover_to_=next;app->omnibox_hover_start_=GetTickCount64();
            app->omnibox_hot_=next;SetTimer(app->window_,kAnimationFrame,16,nullptr);InvalidateRect(window,nullptr,FALSE);
        }
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);return 0;
    }
    if(message==WM_MOUSELEAVE){
        if(app->omnibox_hot_>=0){app->omnibox_hover_from_=app->omnibox_hot_;app->omnibox_hover_to_=-1;app->omnibox_hover_start_=GetTickCount64();SetTimer(app->window_,kAnimationFrame,16,nullptr);}
        app->omnibox_hot_=-1;InvalidateRect(window,nullptr,FALSE);return 0;
    }
    if(message==WM_LBUTTONUP) {
        const int y=GET_Y_LPARAM(lparam)-app->D(5),row=app->D(52);
        const int index=y>=0?y/std::max(1,row):-1;
        if(index>=0 && static_cast<std::size_t>(index)<app->omnibox_suggestions_.size())
            app->ApplyOmniboxSuggestion(static_cast<std::size_t>(index));
        return 0;
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,SuggestionProc,1);
    return DefSubclassProc(window,message,wparam,lparam);
}

LRESULT CALLBACK BrowserApp::Impl::TabProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                          UINT_PTR, DWORD_PTR context) {
    auto* app=reinterpret_cast<Impl*>(context);
    if(!app)return DefSubclassProc(window,message,wparam,lparam);
    const auto keep_alive=app->weak_from_this().lock();
    if(!keep_alive)return DefSubclassProc(window,message,wparam,lparam);
    const auto close_hit=[&](int index,POINT point){
        if(index<0 || static_cast<std::size_t>(index)>=app->tabs_.size())return false;
        if(app->tabs_[static_cast<std::size_t>(index)]->activity.pinned) return false;
        const RECT tab=app->DisplayTabRect(static_cast<std::size_t>(index));
        RECT close{tab.right-app->D(28),app->D(10),tab.right-app->D(4),app->D(34)};
        return PtInRect(&close,point)!=FALSE;
    };
    if(message==WM_PAINT){app->PaintTabs();return 0;}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_NCHITTEST){
        POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};ScreenToClient(window,&point);
        return app->HitTab(point)>=0?HTCLIENT:HTTRANSPARENT;
    }
    if(message==WM_LBUTTONDOWN){
        const POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        const int index=app->HitTab(point);
        if(index>=0 && static_cast<std::size_t>(index)<app->tabs_.size()){
            app->CancelTabDrag();
            const auto id=app->tabs_[static_cast<std::size_t>(index)]->id;
            if(close_hit(index,point))app->pressed_close_tab_id_=id;
            else {app->dragged_tab_id_=id;app->tab_drag_start_=point;app->drag_target_index_=index;}
            SetCapture(window);
            InvalidateRect(window,nullptr,FALSE);
            if(!app->pressed_close_tab_id_)app->Select(static_cast<std::size_t>(index));
            return 0;
        }
    }
    if(message==WM_LBUTTONUP){
        const POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        const int hit=app->HitTab(point);
        const auto close_id=app->pressed_close_tab_id_;
        const auto dragged=app->dragged_tab_id_;
        const bool dragging=app->tab_dragging_;
        const int target=app->drag_target_index_;
        const bool should_close=close_id && hit>=0 && static_cast<std::size_t>(hit)<app->tabs_.size() &&
            app->tabs_[static_cast<std::size_t>(hit)]->id==close_id && close_hit(hit,point);
        POINT screen_point=point;ClientToScreen(window,&screen_point);
        std::string drop_profile;
        if(dragging)if(auto session=app->session_.lock())drop_profile=session->ProfileForWindow(WindowFromPoint(screen_point));
        app->CancelTabDrag();
        if(should_close)PostMessageW(app->window_,kCloseTab,0,static_cast<LPARAM>(close_id));
        else if(dragging && !drop_profile.empty() && drop_profile!=app->profile_name_){
            if(auto session=app->session_.lock())session->MoveTabToProfile(app,dragged,drop_profile);
        } else if(dragging && target>=0)app->MoveTabTo(dragged,static_cast<std::size_t>(target));
        else if(dragged)if(auto* tab=app->Active();tab && tab->controller)
            tab->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return 0;
    }
    if(message==WM_MBUTTONUP){
        const int hit=app->HitTab({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});
        if(hit>=0 && static_cast<std::size_t>(hit)<app->tabs_.size())
            PostMessageW(app->window_,kCloseTab,0,static_cast<LPARAM>(app->tabs_[static_cast<std::size_t>(hit)]->id));
        return 0;
    }
    if(message==WM_MOUSEMOVE){
        const POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        const int hit=app->HitTab(point);
        if(GetCapture()==window && app->dragged_tab_id_){
            if(std::abs(point.x-app->tab_drag_start_.x)>app->D(5) || std::abs(point.y-app->tab_drag_start_.y)>app->D(5))
                app->tab_dragging_=true;
            if(app->tab_dragging_){
                int source=-1,target=-1;
                for(std::size_t i=0;i<app->tabs_.size();++i)if(app->tabs_[i]->id==app->dragged_tab_id_)source=static_cast<int>(i);
                if(point.y>=-app->D(12) && point.y<=app->D(54)){
                    if(hit>=0){
                        target=hit;const auto bounds=app->DisplayTabRect(static_cast<std::size_t>(hit));
                        if(source<hit && point.x<(bounds.left+bounds.right)/2)--target;
                        if(source>hit && point.x>(bounds.left+bounds.right)/2)++target;
                    } else {
                        for(std::size_t i=0;i<app->tab_rects_.size();++i){
                            const auto bounds=app->DisplayTabRect(i);if(IsRectEmpty(&bounds))continue;
                            if(point.x<bounds.left){target=static_cast<int>(i);break;}
                            target=static_cast<int>(i);
                        }
                    }
                }
                app->drag_target_index_=target;
                SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));InvalidateRect(window,nullptr,FALSE);
                return 0;
            }
        }
        const bool close=close_hit(hit,point);
        if(hit!=app->hot_tab_ || close!=app->hot_close_){
            if(hit!=app->hot_tab_){
                app->hover_animation_from_=app->hot_tab_;
                app->hover_animation_to_=hit;
                app->hover_animation_start_=GetTickCount64();
                SetTimer(app->window_,kAnimationFrame,16,nullptr);
            }
            app->hot_tab_=hit;app->hot_close_=close;InvalidateRect(window,nullptr,FALSE);
        }
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);return 0;
    }
    if(message==WM_CAPTURECHANGED || message==WM_CANCELMODE){app->CancelTabDrag();return 0;}
    if(message==WM_MOUSELEAVE && GetCapture()!=window){
        if(app->hot_tab_>=0){
            app->hover_animation_from_=app->hot_tab_;
            app->hover_animation_to_=-1;
            app->hover_animation_start_=GetTickCount64();
            SetTimer(app->window_,kAnimationFrame,16,nullptr);
        }
        app->hot_tab_=-1;app->hot_close_=false;InvalidateRect(window,nullptr,FALSE);return 0;
    }
    if(message==WM_SETCURSOR && app->tab_dragging_){SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));return TRUE;}
    if(message==WM_SETFOCUS || message==WM_KILLFOCUS)InvalidateRect(window,nullptr,FALSE);
    if(message==WM_KEYDOWN && (wparam==VK_LEFT || wparam==VK_RIGHT)){
        app->Dispatch(wparam==VK_LEFT?PreviousTab:NextTab);if(IsWindow(window))SetFocus(window);return 0;
    }
    if(message==WM_KEYDOWN && wparam==VK_DELETE){app->Dispatch(CloseCurrent);return 0;}
    if(message==WM_KEYDOWN && wparam==VK_ESCAPE){app->CancelTabDrag();return 0;}
    if(message==WM_GETDLGCODE)return DLGC_WANTARROWS;
    if(message==WM_CONTEXTMENU){
        app->CancelTabDrag();app->TabMenu({GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});return 0;
    }
    if(message==WM_NCDESTROY){app->CancelTabDrag();RemoveWindowSubclass(window,TabProc,1);}
    return DefSubclassProc(window,message,wparam,lparam);
}

LRESULT CALLBACK BrowserApp::Impl::ButtonProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                             UINT_PTR, DWORD_PTR context) {
    auto* app = reinterpret_cast<Impl*>(context);
    if (!app) return DefSubclassProc(window, message, wparam, lparam);
    const auto keep_alive = app->weak_from_this().lock();
    if (!keep_alive) return DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_MOUSEMOVE) {
        if (app->hovered_button_ != window) {
            HWND old = app->hovered_button_; app->hovered_button_ = window;
            if (old && IsWindow(old)) InvalidateRect(old, nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0}; TrackMouseEvent(&track);
    }
    if (message == WM_MOUSELEAVE && app->hovered_button_ == window) {
        app->hovered_button_ = nullptr; InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_CONTEXTMENU) {
        const UINT id = static_cast<UINT>(GetDlgCtrlID(window));
        if (id >= BookmarkBase && id < BookmarkBase + app->bookmarks_.size()) {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (point.x == -1 && point.y == -1) { RECT r{}; GetWindowRect(window, &r); point = {r.left,r.bottom}; }
            app->BookmarksMenu(point, static_cast<int>(id - BookmarkBase)); return 0;
        }
    }
    if (message == WM_NCDESTROY) {
        if (app->hovered_button_ == window) app->hovered_button_ = nullptr;
        RemoveWindowSubclass(window, ButtonProc, 1);
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

LRESULT BrowserApp::Impl::Message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_ACTIVATE:
        if(LOWORD(wparam)==WA_INACTIVE) HideOmniboxSuggestions();
        else if(!closing_){
            if(auto session=session_.lock())session->ActivateProfile(profile_name_);
            Dirty();
        }
        break;
    case WM_MOUSEWHEEL: {
        const int delta=-GET_WHEEL_DELTA_WPARAM(wparam) * D(48) / WHEEL_DELTA;
        if (ScrollPrivate(delta) || ScrollInternal(delta)) return 0;
        break;
    }
    case WM_MOUSEMOVE: {
        const POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        if(GetCapture()==window_ && bookmark_drag_source_>=0){
            if(!bookmark_dragging_ && (std::abs(point.x-bookmark_drag_start_.x)>D(5) || std::abs(point.y-bookmark_drag_start_.y)>D(5)))
                bookmark_dragging_=true;
            if(bookmark_dragging_){
                int target=bookmark_drag_target_;
                for(const auto& action:native_actions_)if(action.command>=BookmarkBase && action.command<BookmarkBase+bookmarks_.size()){
                    if(point.y<(action.rect.top+action.rect.bottom)/2){target=static_cast<int>(action.command-BookmarkBase);break;}
                    target=static_cast<int>(action.command-BookmarkBase);
                }
                if(target!=bookmark_drag_target_){bookmark_drag_target_=target;const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);}
                SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));return 0;
            }
        }
        UINT hot=0;
        if (auto* tab=Active(); tab && !tab->web && InternalPageFor(tab->url)!=InternalPage::None &&
            InternalPageFor(tab->url)!=InternalPage::NewTab) {
            for(auto it=native_actions_.rbegin();it!=native_actions_.rend();++it) if(it->command && PtInRect(&it->rect,point)){hot=it->command;break;}
        }
        if(hot_native_command_!=hot){hot_native_command_=hot;const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);}
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window_,0};TrackMouseEvent(&track);
        break;
    }
    case WM_MOUSELEAVE:
        if(hot_native_command_){hot_native_command_=0;const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);}
        break;
    case WM_LBUTTONDOWN: {
        const POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        if(auto* tab=Active();tab && !tab->web && InternalPageFor(tab->url)==InternalPage::Bookmarks && native_filter_ && GetWindowTextLengthW(native_filter_)==0){
            for(const auto& action:native_actions_)if(action.command>=BookmarkBase && action.command<BookmarkBase+bookmarks_.size() && PtInRect(&action.rect,point)){
                const int handle_right=action.rect.left+D(34);
                if(point.x<=handle_right){
                    bookmark_drag_source_=static_cast<int>(action.command-BookmarkBase);
                    bookmark_drag_target_=bookmark_drag_source_;bookmark_drag_start_=point;bookmark_dragging_=false;
                    SetCapture(window_);return 0;
                }
            }
        }
        if(!fullscreen_ && PtInRect(&native_chrome_.omnibox,point)){
            FocusOmnibox();return 0;
        }
        if(IsWindowVisible(home_search_) && PtInRect(&native_chrome_.home_search_rect,point)){
            SetFocus(home_search_);return 0;
        }
        break;
    }
    case WM_LBUTTONUP: {
        const POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        if(bookmark_drag_source_>=0){
            const int source=bookmark_drag_source_,target=bookmark_drag_target_;const bool dragging=bookmark_dragging_;
            bookmark_drag_source_=-1;bookmark_drag_target_=-1;bookmark_dragging_=false;
            if(GetCapture()==window_)ReleaseCapture();
            if(dragging && source>=0 && target>=0)MoveBookmark(static_cast<std::size_t>(source),static_cast<std::size_t>(target));
            else {const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);}
            return 0;
        }
        if(HitNativeAction(point)) return 0;
        break;
    }
    case WM_CAPTURECHANGED:
        if(bookmark_drag_source_>=0){bookmark_drag_source_=-1;bookmark_drag_target_=-1;bookmark_dragging_=false;const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);}
        break;
    case WM_NCCALCSIZE:
        if (wparam) {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
            if (IsZoomed(window_) && !fullscreen_) {
                MONITORINFO monitor{}; monitor.cbSize=sizeof(monitor);
                if (GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor))
                    params->rgrc[0] = monitor.rcWork;
            }
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}; ScreenToClient(window_, &point);
        RECT r{}; GetClientRect(window_, &r);
        if (!fullscreen_ && !IsZoomed(window_)) {
            const int border = D(6);
            const bool left = point.x < border, right = point.x >= r.right - border;
            const bool top = point.y < border, bottom = point.y >= r.bottom - border;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT; if (right) return HTRIGHT;
            if (top) return HTTOP; if (bottom) return HTBOTTOM;
        }
        if (!fullscreen_ && point.y < D(40)) {
            HWND child = ChildWindowFromPointEx(window_, point, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
            if (child == tab_strip_) {
                POINT tab_point = point; MapWindowPoints(window_, tab_strip_, &tab_point, 1);
                if (HitTab(tab_point) < 0) return HTCAPTION;
            } else if (child == window_ || !child) return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_NCACTIVATE: return DefWindowProcW(window_, message, wparam, -1);
    case WM_SIZE:
        if(wparam==SIZE_MINIMIZED){
            native_chrome_.frame.Reset();native_chrome_.tabs.Reset();native_chrome_.button.Reset();
            if(auto* tab=Active())SetVisible(*tab,false);
            return 0;
        }
        Layout();
        if (auto* tab = Active()) SetVisible(*tab, true);
        return 0;
    case WM_MOVE:
        for (auto& tab : tabs_) if (tab->controller) tab->controller->NotifyParentWindowPositionChanged();
        return 0;
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wparam);
        const auto* rect = reinterpret_cast<const RECT*>(lparam);
        Fonts();
        SetWindowPos(window_, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Layout(); return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize = {D(560), D(480)};
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: Paint(); return 0;
    case WM_DRAWITEM: DrawItem(*reinterpret_cast<const DRAWITEMSTRUCT*>(lparam)); return TRUE;
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wparam);
        const HWND control = reinterpret_cast<HWND>(lparam);
        const bool search = control == home_search_ || control == native_filter_, focused = GetFocus() == address_;
        SetTextColor(dc, kInk); SetBkColor(dc, search ? kSearch : focused ? kFrame : kSurface);
        return reinterpret_cast<LRESULT>(search ? search_brush_ : focused ? edit_brush_ : idle_edit_brush_);
    }
    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(lparam) == toast_) {
            HDC dc = reinterpret_cast<HDC>(wparam); SetTextColor(dc, kInk); SetBkColor(dc, kSurface);
            return reinterpret_cast<LRESULT>(toast_brush_);
        }
        break;
    case WM_COMMAND:
        if(LOWORD(wparam)==HomeSearch){
            if(HIWORD(wparam)==EN_SETFOCUS || HIWORD(wparam)==EN_KILLFOCUS)
                InvalidateRect(window_,&native_chrome_.home_search_rect,FALSE);
            return 0;
        }
        if(LOWORD(wparam)==NativeFilter){
            if(HIWORD(wparam)==EN_CHANGE || HIWORD(wparam)==EN_SETFOCUS || HIWORD(wparam)==EN_KILLFOCUS){
                native_scroll_=0; const RECT content=ContentBounds(); InvalidateRect(window_,&content,FALSE);
            }
            return 0;
        }
        if (LOWORD(wparam) == Address) {
            if (HIWORD(wparam) == EN_SETFOCUS) { address_editing_ = true; Layout(); UpdateOmniboxSuggestions(); }
            if (HIWORD(wparam) == EN_KILLFOCUS) { address_editing_ = false; HideOmniboxSuggestions(); Layout(); }
            if (HIWORD(wparam) == EN_CHANGE && !changing_address_ && GetFocus() == address_) {
                address_editing_ = true; UpdateOmniboxSuggestions();
            }
            return 0;
        }
        if (HIWORD(wparam) == BN_CLICKED) Dispatch(LOWORD(wparam));
        return 0;
    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<NMHDR*>(lparam);
        if (header->hwndFrom == tooltips_ && header->code == TTN_GETDISPINFOW) {
            auto* info = reinterpret_cast<NMTTDISPINFOW*>(lparam);
            const auto label = ControlText(reinterpret_cast<HWND>(header->idFrom));
            lstrcpynW(info->szText, label.c_str(), 80);
            info->lpszText = info->szText;
            return 0;
        }
        break;
    }
    case WM_TIMER:
        if (wparam == kUiFrame) {
            KillTimer(window_,kUiFrame);ui_frame_pending_=false;
            const auto address_tab=std::exchange(queued_address_tab_,0);
            UpdateUI(address_tab && Active() && Active()->id==address_tab && !address_editing_ && !menu_open_);
        }
        if (wparam == kAnimationFrame) {
            const auto now=GetTickCount64();
            const bool tab_animating=tab_transition_until_ && now<tab_transition_until_;
            const bool hover_animating=hover_animation_start_ && now-hover_animation_start_<100;
            const bool reflow_animating=tab_reflow_start_ && now-tab_reflow_start_<160;
            const bool omnibox_animating=omnibox_hover_start_ && now-omnibox_hover_start_<100;
            if(tab_strip_)InvalidateRect(tab_strip_,nullptr,FALSE);
            if(omnibox_animating && suggestion_panel_ && IsWindowVisible(suggestion_panel_))InvalidateRect(suggestion_panel_,nullptr,FALSE);
            if(tab_animating && window_){
                RECT chrome{};GetClientRect(window_,&chrome);chrome.bottom=ContentTop();
                InvalidateRect(window_,&chrome,FALSE);
            }
            if(!tab_animating)tab_transition_until_=0;
            if(!hover_animating){hover_animation_start_=0;hover_animation_from_=-1;hover_animation_to_=-1;}
            if(!reflow_animating){tab_reflow_start_=0;tab_reflow_from_.clear();}
            if(!omnibox_animating){omnibox_hover_start_=0;omnibox_hover_from_=-1;omnibox_hover_to_=-1;}
            if(!tab_animating && !hover_animating && !reflow_animating && !omnibox_animating)KillTimer(window_,kAnimationFrame);
        }
        if (wparam == kDownloadTick) {
            bool active_download=false, changed=false;
            for(auto& item:download_entries_) {
                if(!item->operation || item->event_driven) continue;
                const auto before_state=item->state; const auto before_bytes=item->received; const bool before_paused=item->paused;
                RefreshDownload(*item);
                changed = changed || before_state!=item->state || before_bytes!=item->received || before_paused!=item->paused;
                if(item->state==COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS) active_download=true;
                else if(!item->paused) item->operation.Reset();
            }
            if(changed) {
                if(auto* current=Active();current && current->url==L"webcom:downloads") {
                    const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);
                }
            }
            if(!active_download){KillTimer(window_,kDownloadTick);SaveDownloads();}
        }
        if (wparam == kTick) Tick();
        if (wparam == kSave) { KillTimer(window_, kSave); Save(false); }
        if (wparam == kToast) { KillTimer(window_, kToast); note_until_ = 0; ShowWindow(toast_, SW_HIDE); }
        return 0;
    case kNotice: {
        queued_notice_ = false;
        const auto text = std::exchange(notice_text_, L"");
        if (!text.empty()) MessageBoxW(window_, text.c_str(), L"WebCom", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    case kCloseTab: Close(static_cast<std::uint64_t>(lparam)); return 0;
    case kSelectTab:
        for (std::size_t i = 0; i < tabs_.size(); ++i)
            if (tabs_[i]->id == static_cast<std::uint64_t>(lparam)) { Select(i); break; }
        return 0;
    case kRunAction: Dispatch(static_cast<UINT>(wparam), lparam); return 0;
    case kPageFullscreen:
        if (auto* current = Find(static_cast<std::uint64_t>(lparam)); current && IsActive(*current) && current->web) {
            BOOL requested = FALSE;
            if (SUCCEEDED(current->web->get_ContainsFullScreenElement(&requested)) &&
                (requested != FALSE) != fullscreen_) ToggleFullscreen();
        }
        return 0;
    case WM_COPYDATA: {
        const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lparam);
        if(data && data->dwData==BrowserApp::OpenLaunchMessage){
            if(!data->lpData || data->cbData<16 || data->cbData>MaxLaunchPacketBytes)return FALSE;
            const auto request=DecodeLaunchRequest({static_cast<const std::uint8_t*>(data->lpData),data->cbData});
            if(!request)return FALSE;
            if(auto session=session_.lock())return session->OpenProfile(request->profile,Wide(request->address),request->private_mode,SW_SHOWNORMAL,true)?TRUE:FALSE;
            return FALSE;
        }
        if (!data || (data->dwData != BrowserApp::OpenAddressMessage && data->dwData != BrowserApp::OpenPrivateAddressMessage) || !data->lpData ||
            data->cbData < sizeof(wchar_t) || data->cbData > 32770 || data->cbData % sizeof(wchar_t)) return FALSE;
        const auto* text = static_cast<const wchar_t*>(data->lpData);
        const auto count = data->cbData / sizeof(wchar_t);
        if (text[count - 1] != L'\0') return FALSE;
        const std::wstring input(text, count - 1);
        const auto resolved = ResolveAddress(Utf8(input));
        if (!resolved.allowed) return FALSE;
        if(auto session=session_.lock())return session->OpenProfile(profile_name_,Wide(resolved.url),data->dwData==BrowserApp::OpenPrivateAddressMessage,SW_SHOWNORMAL,true)?TRUE:FALSE;
        return FALSE;
    }
    case WM_CLOSE: {
        if (closing_) return 0;
        // Closing is immediate by user preference. Save normal metadata before
        // releasing controllers; a failed save preserves the previous atomic file.
        Save(false); SaveDownloads();
        closing_ = true;
        CancelTabDrag();
        KillTimer(window_, kTick); KillTimer(window_, kSave); KillTimer(window_, kUiFrame); KillTimer(window_, kToast);
        KillTimer(window_, kAnimationFrame); KillTimer(window_, kDownloadTick);
        for (auto& tab : tabs_) Release(*tab);
        environment_.Reset();
        DestroyWindow(window_);
        return 0;
    }
    case WM_QUERYENDSESSION: Save(false); return TRUE;
    case WM_ENDSESSION:
        if (wparam) { Save(false); closing_ = true; }
        return 0;
    case WM_DESTROY: return 0;
    case WM_NCDESTROY: {
        const HWND destroyed=window_;
        SetWindowLongPtrW(destroyed,GWLP_USERDATA,0);
        window_=nullptr;
        if(auto session=session_.lock())session->OnWindowDestroyed(this);
        return DefWindowProcW(destroyed,message,wparam,lparam);
    }
    default: break;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

#include "profile_host_impl.inl"

} // namespace webcom
