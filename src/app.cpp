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
#include "saved_sessions.h"
#include "reader_model.h"
#include "capture_model.h"
#include "external_protocol.h"
#include "selection_capture_model.h"
#include "profile_template.h"
#include "startup_model.h"
#include "update_model.h"
#include "update_runtime_model.h"
#include "update_retry_model.h"
#include "reliability_report_model.h"
#include "update_recovery_model.h"
#include "runtime_log.h"
#include "update_transport_win32.h"
#include "extension_model.h"
#include "extension_sources.h"
#include "release_config.h"
#include "onboarding_model.h"
#include "diagnostics_model.h"
#include "diagnostics_bundle.h"
#include "annotation_editor.h"
#include "win32_com_compat.h"
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include "com_callback.h"
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <windowsx.h>
#include <oleacc.h>
#include <oleauto.h>
#include <gdiplus.h>
#include "favicon_image.h"
#include <shlwapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
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
constexpr UINT kUpdateHealth = 7;
constexpr UINT kNotice = WM_APP + 1;
constexpr UINT kCloseTab = WM_APP + 2;
constexpr UINT kRunAction = WM_APP + 3;
constexpr UINT kPageFullscreen = WM_APP + 4;
constexpr UINT kSelectTab = WM_APP + 5;
constexpr UINT kUpdateCheckComplete = WM_APP + 6;
constexpr UINT kUpdateStageComplete = WM_APP + 7;
constexpr std::string_view kWebComVersion = "2.0.0";
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
    MemoryReport, Downloads, TogglePasswords, ToggleAutofill, ClearSavedPasswords, ClearAutofillData, OpenInDefault, About,
    FocusAddress, NextTab, PreviousTab, FullScreen, Exit, ClearData,
    EngineCrashed = 400, TabCrashed, InstallRuntime,
    BookmarkMenu, PasswordMenu, SettingsMenu, ShortcutsPage, DiagnosticsPage, RecoveryPage, UpdatesPage, ExtensionsPage, ExtensionsRefresh, ExtensionsLoadUnpacked, WelcomePage, OpenDefaultApps, WelcomeStartBrowsing, CopyDiagnostics, ExportDiagnosticsBundle, ExportRecoveryReport, OpenDataFolder, UpdateChannelMenu, UpdateChannelStable, UpdateChannelBeta, ToggleUpdateChecks, CheckForUpdates, StageAvailableUpdate, InstallStagedUpdate, RollbackPreviousVersion, ClearUpdateTransaction, ZoomIn, ZoomOut, ZoomReset, PrintPage, OpenFile,
    SearchInPage, DeveloperTools, HelpMenu, RemoveBookmark, PrivateHelp, History, ClearHistory, HistoryDeleteMatches, HistoryDeleteWeek, HistoryDeleteOlderMonth,
    ClearDownloadsList, OpenDownloadsFolder, MuteTab, MoveTabLeft, MoveTabRight, NewProfileWindow,
    AppearanceMenu, ThemeMenu, ProfileColorMenu, ToggleBookmarks, ToggleReducedMotion, ToggleHighContrast, ColorAutomatic, ColorCustom, AppearanceReset, BookmarkManager,
    NativeFilter, ImportBookmarks, ExportBookmarks, NewBookmarkFolder, NewTabGroup, RemoveTabGroup,
    ToggleTabGroup, RenameTabGroup, CloseTabGroup, ProfileManager, OpenAllProfiles, CloseOtherProfiles, RenameBookmarkFolderCmd, CreateBookmarkFolderCmd, ResetSitePermissionsCmd,
    ShowSiteCookiesCmd, ClearSiteCookiesCmd, ShowCertificateDetailsCmd, DeleteCurrentProfileCmd, RenameProfileDisplayNameCmd, ExportProfileSetupCmd, ImportProfileSetupCmd, CreateProfileDesktopShortcutCmd,
    WorkspaceMenu, AddWorkspace, RenameWorkspaceCmd, DeleteWorkspaceCmd, ToggleVerticalTabs, ToggleVerticalTabsCollapse, SplitView, CloseSplitView, QuickCommands, CapturePage, CaptureViewport, CaptureFullPage, CaptureSelection, CaptureViewportClipboard, CaptureSelectionClipboard, AnnotateViewport, AnnotateSelection,
    NextWorkspace, PreviousWorkspace,
    SessionsManager, SaveWorkspaceSession, PrivacyManager, PrivacyRefresh, PrivacyClearAllSiteData, PrivacyResetAllPermissions, ReaderMode, ReaderBackToPage,
    ReaderTextSmaller, ReaderTextLarger, ReaderWidthCycle, ReaderFontToggle, ReaderCopyText, ReaderSaveText, ReaderExportMenu, ReaderSaveHtml, ReaderSaveMarkdown,
    StartupMenu, StartupContinue, StartupNewTab, StartupSpecificPages, StartupAddCurrentPage, StartupAddPage, StartupClearPages, FocusCycle, FocusCycleReverse,
    ProfileBase = 1000, SelectTabBase = 2000, BookmarkBase = 3000, OpenTabBase = 4000,
    ThemeBase = 5000, ColorBase = 5100, HistoryBase = 6000, DownloadBase = 7000, RemoveBookmarkBase = 8000,
    MoveProfileBase = 9000, GroupBase = 10000, BookmarkFolderBase = 11000, BookmarkBarFolderBase = 12000,
    GroupColorBase = 13000, DeleteHistoryBase = 14000,
    PauseDownloadBase = 15000, CancelDownloadBase = 15300, RetryDownloadBase = 15600,
    SitePermissionBase = 16000, MoveWorkspaceBase = 17000, WorkspaceBase = 18000, WorkspaceColorBase = 18100,
    SessionBase = 19000, DeleteSessionBase = 19200, PrivacyCookieBase = 19400, PrivacyPermissionBase = 19600, BookmarkFolderDropBase = 20000, DeleteBookmarkFolderBase = 20200, StartupPageBase = 20400, ExtensionToggleBase = 21000, ExtensionRemoveBase = 21200, ExtensionDetailsBase = 21400,
    ExtensionOpenFolderBase = 21600, ExtensionReloadBase = 21800, ExtensionSourceDetailsBase = 22000, ExtensionForgetSourceBase = 22200
};

COLORREF NativeColor(std::uint32_t rgb) {
    return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

std::uint64_t UnixNowSeconds() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    return seconds > 0 ? static_cast<std::uint64_t>(seconds) : 0;
}

constexpr std::array<std::uint32_t,8> kProfileColors{
    0xA8C7FA,0x81C995,0xFDD663,0xF6AEA9,0xC7B3F4,0x78D9EC,0xFFB77D,0xBEC6D3};

struct CoString {
    LPWSTR value = nullptr;
    ~CoString() { CoTaskMemFree(value); }
    std::wstring str() const { return value ? value : L""; }
};

struct UpdateCheckCompletion {
    UpdateCheckEvaluation evaluation;
};

struct UpdateStageCompletion {
    bool success = false;
    UpdateManifest manifest;
    std::filesystem::path package;
    std::string message;
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

bool SystemHighContrastEnabled() {
    HIGHCONTRASTW state{};
    state.cbSize = sizeof(state);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(state), &state, 0) &&
           (state.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool TryGetCookieManager(const ComPtr<ICoreWebView2>& web, ComPtr<ICoreWebView2CookieManager>& manager) {
    manager.Reset();
    if (!web) return false;
    ComPtr<ICoreWebView2_2> web2;
    if (FAILED(web.As(&web2)) || !web2) return false;
    return SUCCEEDED(web2->get_CookieManager(&manager)) && manager.Get() != nullptr;
}

enum class InternalPage { None, NewTab, Welcome, History, Downloads, Settings, Shortcuts, Diagnostics, Recovery, Updates, Extensions, Bookmarks, Profiles, Sessions, Privacy, Reader };

InternalPage InternalPageFor(std::wstring_view value) {
    if (IsHome(value)) return InternalPage::NewTab;
    if (value == L"webcom:welcome") return InternalPage::Welcome;
    if (value == L"webcom:history") return InternalPage::History;
    if (value == L"webcom:downloads") return InternalPage::Downloads;
    if (value == L"webcom:settings") return InternalPage::Settings;
    if (value == L"webcom:shortcuts") return InternalPage::Shortcuts;
    if (value == L"webcom:diagnostics") return InternalPage::Diagnostics;
    if (value == L"webcom:recovery") return InternalPage::Recovery;
    if (value == L"webcom:updates") return InternalPage::Updates;
    if (value == L"webcom:extensions") return InternalPage::Extensions;
    if (value == L"webcom:bookmarks") return InternalPage::Bookmarks;
    if (value == L"webcom:profiles") return InternalPage::Profiles;
    if (value == L"webcom:sessions") return InternalPage::Sessions;
    if (value == L"webcom:privacy") return InternalPage::Privacy;
    if (value == L"webcom:reader") return InternalPage::Reader;
    return InternalPage::None;
}

bool IsInternalPage(std::wstring_view value) { return InternalPageFor(value) != InternalPage::None; }

std::wstring InternalPageTitle(std::wstring_view value, bool private_mode = false) {
    switch (InternalPageFor(value)) {
    case InternalPage::NewTab: return private_mode ? L"New Incognito Tab" : L"New Tab";
    case InternalPage::Welcome: return L"Welcome to WebCom";
    case InternalPage::History: return L"History";
    case InternalPage::Downloads: return L"Downloads";
    case InternalPage::Settings: return L"Settings";
    case InternalPage::Shortcuts: return L"Keyboard Shortcuts";
    case InternalPage::Diagnostics: return L"Support Diagnostics";
    case InternalPage::Recovery: return L"Recovery & Reliability";
    case InternalPage::Updates: return L"Updates";
    case InternalPage::Extensions: return L"Extensions";
    case InternalPage::Bookmarks: return L"Bookmarks";
    case InternalPage::Profiles: return L"Profiles";
    case InternalPage::Sessions: return L"Saved Sessions";
    case InternalPage::Privacy: return L"Privacy Manager";
    case InternalPage::Reader: return L"Reader Mode";
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

bool IsLocalFileUrl(std::wstring_view value) {
    return webcom::IsLocalFileAddress(Utf8(value));
}

bool IsNavigableAddress(std::wstring_view value) {
    return IsWebAddress(value) || IsLocalFileUrl(value);
}

std::optional<std::string> CanonicalStartupPage(std::wstring_view value) {
    if (!IsWebAddress(value)) return std::nullopt;
    const auto resolved = ResolveAddress(Utf8(value));
    if (!resolved.allowed || resolved.url.empty()) return std::nullopt;
    const auto scheme = resolved.url.find("://");
    if (scheme == std::string::npos) return std::nullopt;
    const auto authority_start = scheme + 3;
    const auto authority_end = resolved.url.find_first_of("/?#", authority_start);
    const auto authority = resolved.url.substr(authority_start, authority_end - authority_start);
    if (authority.empty() || authority.find('@') != std::string::npos) return std::nullopt;
    return resolved.url;
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

std::wstring CertificateTimeLabel(double unix_seconds) {
    if (!(unix_seconds > 0.0)) return L"Unknown";
    constexpr double kWindowsEpochOffsetSeconds = 11644473600.0;
    const double ticks = (unix_seconds + kWindowsEpochOffsetSeconds) * 10000000.0;
    if (ticks <= 0.0 || ticks > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) return L"Unknown";
    ULARGE_INTEGER value{}; value.QuadPart = static_cast<std::uint64_t>(ticks);
    FILETIME file_time{value.LowPart, value.HighPart};
    SYSTEMTIME utc{}, local{};
    if (!FileTimeToSystemTime(&file_time, &utc) ||
        !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return L"Unknown";
    wchar_t date[64]{}, time[64]{};
    if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, date, 64, nullptr) ||
        !GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, nullptr, time, 64)) return L"Unknown";
    return std::wstring(date) + L" " + time;
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
        named_sessions_ = LoadNamedSessions(root_, profile_name_);
        extension_sources_ = LoadExtensionSources(root_, profile_name_);
        RefreshExtensionSourceHealth();
        update_settings_ = LoadUpdateSettings(root_);
        update_transaction_ = LoadUpdateTransaction(root_);
        update_retry_ = LoadUpdateRetryState(root_);
        recovery_package_ = LoadRecoveryPackage(root_);
        update_runtime_config_ = ReleaseRuntimeConfig{
            release_config::kStableManifestUrl, release_config::kBetaManifestUrl, release_config::kPublisherSha256};
    }
    ~Impl();
    bool Create(int show_command, const std::wstring& initial_address, bool start_private);
    bool HandleKeyMessage(const MSG& message);
private:
    friend class BrowserApp::Session;
    class TabStripAccessible;
    struct CookieSummary {
        std::wstring name;
        std::wstring domain;
        std::wstring path;
        bool secure = false;
        bool http_only = false;
    };
    struct CertificateDetails {
        std::wstring request_uri;
        std::wstring subject;
        std::wstring issuer;
        double valid_from = 0.0;
        double valid_to = 0.0;
    };
    struct PrivacyCookieDomain {
        std::wstring domain;
        unsigned count = 0;
    };
    struct Tab {
        std::uint64_t id = 0;
        std::uint64_t generation = 0;
        UINT64 navigation_id = 0;
        std::wstring url = L"webcom:newtab";
        std::wstring title = L"New tab";
        std::wstring error;
        std::wstring group;
        std::wstring workspace = L"Main";
        bool private_mode = false;
        bool ephemeral = false;
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
        std::optional<CertificateDetails> certificate_details;
        std::wstring reader_source;
        std::wstring reader_text;
        std::uint64_t reader_source_tab_id = 0;
        int reader_text_scale = 0;
        int reader_width_mode = 1;
        bool reader_serif = false;
        TabActivity activity;
        ComPtr<ICoreWebView2Controller> controller;
        ComPtr<ICoreWebView2> web;
        ComPtr<ICoreWebView2NewWindowRequestedEventArgs> popup_args;
        ComPtr<ICoreWebView2Deferral> popup_deferral;
        std::vector<ComPtr<ICoreWebView2DownloadOperation>> downloads;
        EventRegistrationToken navigation{}, completed{}, source{}, title_changed{}, history{}, popup{},
            close_requested{}, process_failed{}, accelerator{}, audio{}, muted_changed{}, download{}, fullscreen{}, favicon_changed{}, permission{}, certificate{};
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
    struct ExtensionEntry {
        std::wstring id;
        std::wstring name;
        bool enabled = false;
        ComPtr<ICoreWebView2BrowserExtension> extension;
    };
    struct ExtensionSourceHealth {
        ExtensionFolderInfo manifest;
        ExtensionSourceSnapshot snapshot;
        bool baseline_known = false;
        bool changed = false;
    };
    struct NativeAction { RECT rect{}; UINT command = 0; };
    struct WorkspaceDropTarget { std::size_t workspace_index = 0; RECT rect{}; };
    struct TabReflowOrigin { std::uint64_t id = 0; RECT rect{}; };
    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr, tab_strip_ = nullptr, address_ = nullptr, home_search_ = nullptr, native_filter_ = nullptr, suggestion_panel_ = nullptr;
    ComPtr<IAccessible> tab_accessible_;
    TabStripAccessible* tab_accessible_impl_ = nullptr;
    HWND tooltips_ = nullptr, toast_ = nullptr, hovered_button_ = nullptr;
    std::vector<HWND> toolbar_;
    std::array<HWND, 4> home_buttons_{};
    std::vector<HWND> bookmark_buttons_;
    std::vector<Bookmark> bookmarks_;
    std::vector<std::string> explicit_bookmark_folders_;
    std::vector<std::wstring> bookmark_folders_;
    std::vector<SavedTabGroup> tab_groups_;
    std::vector<SavedWorkspace> workspaces_;
    std::wstring active_workspace_ = L"Main";
    bool vertical_tabs_ = false;
    bool vertical_tabs_collapsed_ = false;
    std::uint64_t split_tab_id_ = 0;
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
    int drag_workspace_target_ = -1;
    int bookmark_drag_source_ = -1, bookmark_drag_target_ = -1, bookmark_drag_folder_target_ = -1;
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
    std::vector<NamedSession> named_sessions_;
    std::vector<PrivacyCookieDomain> privacy_cookie_domains_;
    UpdateSettings update_settings_{};
    UpdateTransaction update_transaction_{};
    UpdateRetryState update_retry_{};
    std::optional<RecoveryPackage> recovery_package_;
    ComPtr<ICoreWebView2Profile> extension_profile_;
    std::vector<ExtensionEntry> extensions_;
    std::vector<ExtensionSourceRecord> extension_sources_;
    std::vector<ExtensionSourceHealth> extension_source_health_;
    std::uint64_t extension_bootstrap_tab_id_ = 0;
    bool extensions_loading_ = false;
    bool extensions_supported_ = true;
    std::wstring extensions_status_;
    ReleaseRuntimeConfig update_runtime_config_{};
    std::optional<UpdateManifest> available_update_;
    bool update_checking_ = false, update_staging_ = false, update_health_owner_ = false;
    std::wstring update_runtime_status_;
    bool privacy_loading_ = false, privacy_loaded_ = false;
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
    void CreateBookmarkFolder();
    void RenameBookmarkFolderTree(std::wstring folder);
    void DeleteBookmarkFolderTree(std::wstring folder);
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
    void OpenLocalFile();
    void OpenExternalProtocol(std::wstring uri);
    std::uint64_t Add(std::wstring url = L"webcom:newtab", bool select = true, bool private_mode = false);
    void Select(std::size_t index);
    void MoveTab(std::uint64_t id, int direction);
    void MoveTabTo(std::uint64_t id, std::size_t target);
    void AssignTabGroup(std::uint64_t id, std::wstring group);
    std::vector<std::wstring> TabGroups() const;
    std::vector<std::wstring> WorkspaceNames() const;
    SavedWorkspace* FindWorkspace(std::wstring_view name);
    const SavedWorkspace* FindWorkspace(std::wstring_view name) const;
    SavedWorkspace& EnsureWorkspace(std::wstring_view name);
    void WorkspaceMenuPopup();
    void SwitchWorkspace(std::wstring_view name);
    void CreateWorkspace();
    void RenameWorkspace();
    void DeleteWorkspace();
    void AssignTabWorkspace(std::uint64_t id, std::wstring workspace);
    void ToggleVerticalTabsMode();
    void ToggleVerticalTabsCollapseMode();
    void CycleWorkspace(int direction);
    Tab* SplitTab();
    const Tab* SplitTab() const;
    bool IsSplitVisible(const Tab& tab) const;
    RECT ControllerBoundsFor(const Tab& tab) const;
    void ApplyTabVisibility();
    void SetSplitTab(std::uint64_t id);
    void ClearSplitView();
    void ShowQuickCommands();
    std::vector<WorkspaceDropTarget> WorkspaceDropTargets(RECT bounds) const;
    void PaintWorkspaceDropTargets(HDC dc, const std::vector<WorkspaceDropTarget>& targets) const;
    void CaptureMenuPopup();
    void SavePageCapture(bool full_page);
    void SaveSelectionCapture();
    void CopyVisibleCaptureToClipboard();
    void CopySelectionCaptureToClipboard();
    void AnnotateVisibleCapture();
    void AnnotateSelectionCapture();
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
    SavedProfile* CurrentSavedProfile();
    const SavedProfile* CurrentSavedProfile() const;
    std::wstring ProfileDisplayName(std::string_view profile, const Appearance* known = nullptr) const;
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
    void ShowCertificateDetails();
    void RefreshSiteCookies(Tab& tab);
    void SiteCookiesMenu();
    void ClearSiteCookies();
    void SaveSitePermissionsNow();
    void SyncRuntimeSitePermission(Tab& tab, std::string_view origin, SitePermissionKind kind, SitePermissionDecision decision);
    void OpenInternalPage(std::wstring url);
    void SaveCurrentWorkspaceSession();
    void RestoreNamedSession(std::size_t index);
    void DeleteNamedSessionUi(std::size_t index);
    void SaveNamedSessionsNow();
    void RefreshPrivacyManager();
    std::vector<std::string> PrivacyPermissionOrigins() const;
    void ClearPrivacyCookieDomain(std::size_t index);
    void ResetPrivacyPermissionOrigin(std::size_t index);
    void ClearAllSiteDataFromPrivacy();
    void EnsureExtensionProfile();
    void CaptureExtensionProfile(Tab& tab);
    void RefreshExtensions();
    void RefreshExtensionSourceHealth();
    void LoadUnpackedExtension();
    void InstallExtensionFromSource(const std::filesystem::path& folder, bool reloading = false);
    void ToggleExtension(std::size_t index);
    void RemoveExtension(std::size_t index);
    void ShowExtensionDetails(std::size_t index);
    void ShowExtensionSourceDetails(std::size_t source_index);
    void OpenExtensionSourceFolder(std::size_t source_index);
    void ReloadExtensionSource(std::size_t source_index, std::optional<std::size_t> installed_index = std::nullopt);
    void ForgetExtensionSourceMapping(std::size_t source_index);
    bool SaveExtensionSourcesNow();
    void OpenReaderMode();
    void ReturnFromReader();
    void CopyReaderText();
    void SaveReaderText();
    void ReaderExportMenuPopup();
    void SaveReaderHtml();
    void SaveReaderMarkdown();
    DiagnosticsSnapshot DiagnosticsSnapshotNow() const;
    std::wstring DiagnosticsTextNow() const;
    void CopyDiagnosticsText();
    void ExportDiagnosticsBundleNow();
    void ExportRecoveryReportNow();
    ReliabilityReportSnapshot ReliabilitySnapshotNow() const;
    std::uint64_t UpdateRetryRemainingNow() const;
    void OpenDataFolderNow();
    void UpdateChannelMenuPopup();
    void SetUpdateChannel(ReleaseChannel channel);
    void StartUpdateCheck();
    void StageAvailableUpdateNow();
    void InstallStagedUpdateNow();
    void RollbackPreviousVersionNow();
    void HandleUpdateCheckCompletion(UpdateCheckCompletion* completion);
    void HandleUpdateStageCompletion(UpdateStageCompletion* completion);
    void PrepareUpdateHealthState();
    void ConfirmUpdateHealth();
    void OpenDefaultAppsSettings();
    bool IsDefaultBrowserNow() const;
    void PersistReaderDefaults(const Tab& reader);
    bool ScrollInternal(int delta);
    void PaintInternalPage(HDC dc, const RECT& content, Tab& tab);
    bool HitNativeAction(POINT point);
    RECT DisplayTabRect(std::size_t index) const;
    void MainMenu();
    void ProfileMenu();
    void TabMenu(POINT point);
    void CreateProfile();
    void SwitchProfile(std::size_t index);
    void RenameProfileDisplayName();
    void ExportProfileSetup();
    void ImportProfileSetup();
    void CreateProfileDesktopShortcut();
    void StartupMenuPopup();
    void CycleChromeFocus(bool reverse);
    void DeleteCurrentProfile();
    void DeleteCurrentProfileNow(Tab& source);
    void ReportMemory();
    void ClearBrowsingData();
    void ClearProfileDataKind(COREWEBVIEW2_BROWSING_DATA_KINDS kind, const std::wstring& title,
                              const std::wstring& question, const std::wstring& success);
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

class BrowserApp::Impl::TabStripAccessible final : public IAccessible {
public:
    explicit TabStripAccessible(Impl* app) : app_(app) {}
    void Detach() { app_ = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid == IID_IUnknown || iid == IID_IDispatch || iid == IID_IAccessible) {
            *object = static_cast<IAccessible*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto value = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (!value) delete this;
        return value;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override { if (!count) return E_POINTER; *count = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return DISP_E_UNKNOWNNAME; }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override { return DISP_E_MEMBERNOTFOUND; }

    HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** parent) override {
        if (!parent) return E_POINTER;
        *parent = nullptr;
        if (!app_ || !app_->window_) return S_FALSE;
        IAccessible* accessible = nullptr;
        const HRESULT hr = AccessibleObjectFromWindow(app_->window_, OBJID_WINDOW, IID_IAccessible,
            reinterpret_cast<void**>(&accessible));
        if (SUCCEEDED(hr) && accessible) *parent = accessible;
        return SUCCEEDED(hr) && accessible ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accChildCount(long* count) override {
        if (!count) return E_POINTER;
        *count = app_ ? static_cast<long>(app_->VisibleTabIndices().size()) : 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accChild(VARIANT, IDispatch** child) override {
        if (!child) return E_POINTER;
        *child = nullptr;
        return S_FALSE; // Tabs are simple MSAA child IDs of this accessible object.
    }
    HRESULT STDMETHODCALLTYPE get_accName(VARIANT child, BSTR* name) override {
        if (!name) return E_POINTER;
        *name = nullptr;
        if (IsSelf(child)) return CopyText(L"WebCom tabs", name);
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        const auto& tab = *app_->tabs_[actual];
        std::wstring title = tab.title.empty() ? L"New tab" : tab.title;
        if (tab.private_mode) title = L"Incognito: " + title;
        return CopyText(title, name);
    }
    HRESULT STDMETHODCALLTYPE get_accValue(VARIANT child, BSTR* value) override {
        if (!value) return E_POINTER;
        *value = nullptr;
        if (IsSelf(child)) return S_FALSE;
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        const auto& tab = *app_->tabs_[actual];
        return CopyText(tab.url, value);
    }
    HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT child, BSTR* description) override {
        if (!description) return E_POINTER;
        *description = nullptr;
        if (IsSelf(child)) return CopyText(L"Open browser tabs. Use Left and Right arrows or Ctrl+Tab to switch tabs.", description);
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        const auto& tab = *app_->tabs_[actual];
        std::vector<std::wstring> parts;
        if (tab.activity.pinned) parts.push_back(L"Pinned");
        if (tab.private_mode) parts.push_back(L"Incognito");
        if (tab.muted) parts.push_back(L"Muted"); else if (tab.activity.audible) parts.push_back(L"Playing audio");
        if (tab.activity.loading || tab.creating) parts.push_back(L"Loading");
        if (tab.activity.suspended) parts.push_back(L"Sleeping");
        if (actual == app_->active_) parts.push_back(L"Selected");
        if (parts.empty()) parts.push_back(L"Browser tab");
        std::wstring text;
        for (std::size_t i = 0; i < parts.size(); ++i) { if (i) text += L", "; text += parts[i]; }
        return CopyText(text, description);
    }
    HRESULT STDMETHODCALLTYPE get_accRole(VARIANT child, VARIANT* role) override {
        if (!role) return E_POINTER;
        VariantInit(role); role->vt = VT_I4;
        if (IsSelf(child)) { role->lVal = ROLE_SYSTEM_PAGETABLIST; return S_OK; }
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        role->lVal = ROLE_SYSTEM_PAGETAB; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accState(VARIANT child, VARIANT* state) override {
        if (!state) return E_POINTER;
        VariantInit(state); state->vt = VT_I4;
        if (IsSelf(child)) { state->lVal = STATE_SYSTEM_FOCUSABLE; return S_OK; }
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        LONG flags = STATE_SYSTEM_SELECTABLE | STATE_SYSTEM_FOCUSABLE;
        if (actual == app_->active_) flags |= STATE_SYSTEM_SELECTED;
        if (actual == app_->active_ && GetFocus() == app_->tab_strip_) flags |= STATE_SYSTEM_FOCUSED;
        state->lVal = flags; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT, BSTR* help) override { if (!help) return E_POINTER; *help = nullptr; return S_FALSE; }
    HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* help_file, VARIANT, long* topic) override {
        if (!help_file || !topic) return E_POINTER; *help_file = nullptr; *topic = -1; return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT child, BSTR* shortcut) override {
        if (!shortcut) return E_POINTER;
        *shortcut = nullptr;
        if (IsSelf(child)) return CopyText(L"Ctrl+Tab", shortcut);
        std::size_t actual = 0;
        long child_id = 0;
        if (!Resolve(child, actual, &child_id)) return E_INVALIDARG;
        const auto visible = app_->VisibleTabIndices();
        std::wstring text;
        if (child_id >= 1 && child_id <= 8) text = L"Ctrl+" + std::to_wstring(child_id);
        else if (!visible.empty() && actual == visible.back()) text = L"Ctrl+9";
        if (text.empty()) return S_FALSE;
        return CopyText(text, shortcut);
    }
    HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* focus) override { return SelectedVariant(focus); }
    HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* selection) override { return SelectedVariant(selection); }
    HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT child, BSTR* action) override {
        if (!action) return E_POINTER;
        *action = nullptr;
        if (IsSelf(child)) return S_FALSE;
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        return CopyText(L"Switch to tab", action);
    }
    HRESULT STDMETHODCALLTYPE accSelect(long flags, VARIANT child) override {
        if (!app_) return E_FAIL;
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        if (!(flags & (SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION))) return E_INVALIDARG;
        app_->Select(actual);
        if (flags & SELFLAG_TAKEFOCUS) SetFocus(app_->tab_strip_);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE accLocation(long* left, long* top, long* width, long* height, VARIANT child) override {
        if (!left || !top || !width || !height) return E_POINTER;
        if (!app_ || !app_->tab_strip_) return E_FAIL;
        RECT rect{};
        if (IsSelf(child)) GetClientRect(app_->tab_strip_, &rect);
        else {
            std::size_t actual = 0;
            if (!Resolve(child, actual)) return E_INVALIDARG;
            rect = app_->DisplayTabRect(actual);
        }
        POINT origin{rect.left, rect.top};
        ClientToScreen(app_->tab_strip_, &origin);
        *left = origin.x; *top = origin.y;
        *width = std::max<LONG>(0, rect.right - rect.left);
        *height = std::max<LONG>(0, rect.bottom - rect.top);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE accNavigate(long direction, VARIANT start, VARIANT* end) override {
        if (!end) return E_POINTER;
        VariantInit(end);
        if (!app_) return E_FAIL;
        const auto visible = app_->VisibleTabIndices();
        if (visible.empty()) return S_FALSE;
        if (IsSelf(start)) {
            if (direction == NAVDIR_FIRSTCHILD) return ChildVariant(1, end);
            if (direction == NAVDIR_LASTCHILD) return ChildVariant(static_cast<long>(visible.size()), end);
            return S_FALSE;
        }
        std::size_t actual = 0;
        long child_id = 0;
        if (!Resolve(start, actual, &child_id)) return E_INVALIDARG;
        if (direction == NAVDIR_NEXT && child_id < static_cast<long>(visible.size())) return ChildVariant(child_id + 1, end);
        if (direction == NAVDIR_PREVIOUS && child_id > 1) return ChildVariant(child_id - 1, end);
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accHitTest(long x, long y, VARIANT* child) override {
        if (!child) return E_POINTER;
        VariantInit(child);
        if (!app_ || !app_->tab_strip_) return E_FAIL;
        POINT point{x, y}; ScreenToClient(app_->tab_strip_, &point);
        const int actual = app_->HitTab(point);
        if (actual < 0) { child->vt = VT_I4; child->lVal = CHILDID_SELF; return S_OK; }
        const auto visible = app_->VisibleTabIndices();
        const auto found = std::find(visible.begin(), visible.end(), static_cast<std::size_t>(actual));
        if (found == visible.end()) { child->vt = VT_I4; child->lVal = CHILDID_SELF; return S_OK; }
        return ChildVariant(static_cast<long>(found - visible.begin()) + 1, child);
    }
    HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT child) override {
        if (!app_) return E_FAIL;
        std::size_t actual = 0;
        if (!Resolve(child, actual)) return E_INVALIDARG;
        app_->Select(actual); return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_accName(VARIANT, BSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE put_accValue(VARIANT, BSTR) override { return E_NOTIMPL; }

private:
    static bool IsSelf(const VARIANT& child) { return child.vt == VT_I4 && child.lVal == CHILDID_SELF; }
    static HRESULT CopyText(std::wstring_view text, BSTR* output) {
        *output = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
        return *output || text.empty() ? S_OK : E_OUTOFMEMORY;
    }
    static HRESULT ChildVariant(long id, VARIANT* output) {
        VariantInit(output); output->vt = VT_I4; output->lVal = id; return S_OK;
    }
    bool Resolve(const VARIANT& child, std::size_t& actual, long* child_id = nullptr) const {
        if (!app_ || child.vt != VT_I4 || child.lVal <= 0) return false;
        const auto visible = app_->VisibleTabIndices();
        if (child.lVal > static_cast<long>(visible.size())) return false;
        actual = visible[static_cast<std::size_t>(child.lVal - 1)];
        if (child_id) *child_id = child.lVal;
        return actual < app_->tabs_.size();
    }
    HRESULT SelectedVariant(VARIANT* output) const {
        if (!output) return E_POINTER;
        VariantInit(output);
        if (!app_) return E_FAIL;
        const auto visible = app_->VisibleTabIndices();
        const auto found = std::find(visible.begin(), visible.end(), app_->active_);
        if (found == visible.end()) { output->vt = VT_EMPTY; return S_FALSE; }
        return ChildVariant(static_cast<long>(found - visible.begin()) + 1, output);
    }

    LONG refs_ = 1;
    Impl* app_ = nullptr;
};

BrowserApp::Impl::~Impl() {
    closing_ = true;
    if (tab_accessible_impl_) tab_accessible_impl_->Detach();
    tab_accessible_impl_ = nullptr;
    tab_accessible_.Reset();
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
    if (!start_private && initial_address.empty()) {
        if (auto session = session_.lock(); session && session->TakeFirstRunWelcome()) {
            if (auto* welcome = Active()) {
                welcome->url = L"webcom:welcome";
                welcome->title = InternalPageTitle(welcome->url);
                welcome->restore_pending = false;
                welcome->ephemeral = true;
            }
        }
    }
    Layout();
    ShowWindow(window_, show_command == SW_SHOWNORMAL ? SW_SHOWMAXIMIZED : show_command);
    UpdateWindow(window_);
    PrepareUpdateHealthState();
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
        if (InternalPageFor(tab->url) == InternalPage::Extensions) EnsureExtensionProfile();
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
    const auto profile_display=ProfileDisplayName(profile_name_,&appearance_);
    const std::wstring profile_label=tab->private_mode?L"Incognito - "+profile_display+L" profile":L"Profile: "+profile_display;
    if(ControlText(GetDlgItem(window_,Profile))!=profile_label)SetWindowTextW(GetDlgItem(window_,Profile),profile_label.c_str());
    std::wstring title = tab->title + L" - WebCom - " + profile_display;
    if(workspaces_.size()>1) title += L" [" + active_workspace_ + L"]";
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
        state_.profiles.push_back(SavedProfile{profile_name_, {}, 0, {}, true, true, {{"Main",0}}, "Main", false, false, StartupMode::Continue, {}});
        it = std::prev(state_.profiles.end());
    }
    workspaces_=it->workspaces;
    if(workspaces_.empty())workspaces_.push_back({"Main",0});
    active_workspace_=Wide(it->active_workspace.empty()?workspaces_.front().name:it->active_workspace);
    vertical_tabs_=it->vertical_tabs;
    vertical_tabs_collapsed_=it->vertical_tabs_collapsed;
    split_tab_id_=0;
    const auto session=session_.lock();
    const bool force_recovered=session && session->ForceRecoveredTabs();
    const auto startup=BuildStartupPlan(*it,force_recovered);
    tab_groups_=startup.restore_saved_session?it->groups:std::vector<SavedTabGroup>{};
    if(startup.restore_saved_session){
        for (const auto& saved : it->tabs) {
            auto tab = std::make_unique<Tab>();
            tab->id = next_id_++;
            tab->url = Wide(saved.url);
            if (!IsInternalPage(tab->url) && !IsNavigableAddress(tab->url)) tab->url = L"webcom:newtab";
            tab->title = Wide(saved.title);
            if (tab->title.empty()) tab->title = IsInternalPage(tab->url) ? InternalPageTitle(tab->url) : tab->url;
            tab->activity.pinned = saved.pinned;
            tab->activity.protected_from_sleep = saved.keep_awake;
            tab->group = Wide(saved.group);
            tab->workspace = Wide(saved.workspace.empty()?std::string("Main"):saved.workspace);
            EnsureWorkspace(tab->workspace);
            tab->activity.last_active_ms = GetTickCount64();
            tab->restore_pending = !IsInternalPage(tab->url);
            tabs_.push_back(std::move(tab));
        }
    }else{
        for(const auto& url:startup.urls){
            auto tab=std::make_unique<Tab>();
            tab->id=next_id_++;
            tab->url=Wide(url);
            if(!IsInternalPage(tab->url) && !IsNavigableAddress(tab->url))tab->url=L"webcom:newtab";
            tab->title=IsInternalPage(tab->url)?InternalPageTitle(tab->url):tab->url;
            tab->workspace=active_workspace_.empty()?L"Main":active_workspace_;
            tab->activity.last_active_ms=GetTickCount64();
            tab->restore_pending=!IsInternalPage(tab->url);
            tabs_.push_back(std::move(tab));
        }
    }
    for(const auto& tab:tabs_)if(!tab->group.empty())EnsureTabGroup(tab->group);
    if (tabs_.empty()) {
        auto tab = std::make_unique<Tab>();
        tab->id = next_id_++;
        tab->workspace = active_workspace_.empty() ? L"Main" : active_workspace_;
        tab->activity.last_active_ms = GetTickCount64();
        tabs_.push_back(std::move(tab));
    }
    active_ = startup.restore_saved_session ? std::min(it->active_tab, tabs_.size() - 1) : 0;
    if(tabs_[active_]->workspace!=active_workspace_) {
        const auto found=std::find_if(tabs_.begin(),tabs_.end(),[&](const auto& tab){return tab->workspace==active_workspace_;});
        if(found!=tabs_.end())active_=static_cast<std::size_t>(found-tabs_.begin());
        else active_workspace_=tabs_[active_]->workspace;
    }
    if(!tabs_[active_]->group.empty())if(auto* group=FindTabGroup(tabs_[active_]->group))group->collapsed=false;
    tabs_[active_]->activity.active = true;
    bookmarks_ = LoadBookmarks(root_, profile_name_);
    explicit_bookmark_folders_ = LoadBookmarkFolders(root_, profile_name_);
    RebuildBookmarks();
    SyncTabs();
}

void BrowserApp::Impl::CaptureProfile() {
    auto it = std::find_if(state_.profiles.begin(), state_.profiles.end(),
        [&](const SavedProfile& p) { return p.name == profile_name_; });
    if (it == state_.profiles.end()) return;
    it->tabs.clear();
    it->groups.clear();
    it->workspaces=workspaces_;
    it->active_workspace=Utf8(active_workspace_);
    it->vertical_tabs=vertical_tabs_;
    it->vertical_tabs_collapsed=vertical_tabs_collapsed_;
    it->active_tab = 0;
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        const auto& tab = *tabs_[i];
        if (tab.private_mode || tab.ephemeral) continue;
        if (i == active_) it->active_tab = it->tabs.size();
        std::string url = Utf8(tab.url);
        const auto resolved = ResolveAddress(url);
        if (!resolved.allowed || url.size() > 16384 || (!IsInternalPage(tab.url) && !IsNavigableAddress(tab.url)))
            url = "webcom:newtab";
        else url = resolved.url;
        const std::wstring title = SafeTitle(tab.title);
        std::string utf_title = Utf8(title);
        if (utf_title.empty()) utf_title = "New tab";
        it->tabs.push_back(SavedTab{url, utf_title, tab.activity.pinned, tab.activity.protected_from_sleep, Utf8(tab.group), Utf8(tab.workspace)});
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
    tab->workspace = active_workspace_.empty()?L"Main":active_workspace_;
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
    if(_wcsicmp(tabs_[index]->workspace.c_str(), active_workspace_.c_str()) != 0) {
        ClearSplitView();
        active_workspace_ = tabs_[index]->workspace;
        first_visible_tab_ = 0;
    }
    if(!tabs_[index]->group.empty())if(auto* group=FindTabGroup(tabs_[index]->group))
        if(group->collapsed){group->collapsed=false;Dirty();}
    Tab* old=Active();
    const auto target_id=tabs_[index]->id;
    if(split_tab_id_ && old && target_id!=old->id){
        if(target_id==split_tab_id_)split_tab_id_=old->id;
        else {auto* secondary=SplitTab();if(secondary)SetVisible(*secondary,false);split_tab_id_=0;}
    }
    const bool changed=!old || old->id!=tabs_[index]->id;
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
    if (InternalPageFor(tab.url) == InternalPage::Extensions) EnsureExtensionProfile();
    ApplyTabVisibility();
    UpdateUI(true);
    if(changed && tab_strip_){
        const auto visible=VisibleTabIndices();
        const auto found=std::find(visible.begin(),visible.end(),active_);
        if(found!=visible.end())
            NotifyWinEvent(EVENT_OBJECT_SELECTION,tab_strip_,OBJID_CLIENT,static_cast<LONG>(found-visible.begin())+1);
    }
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
    const auto visible=VisibleTabIndices();
    const auto current=std::find_if(visible.begin(),visible.end(),[&](std::size_t index){return tabs_[index]->id==id;});
    if(current==visible.end())return;
    const auto position=static_cast<std::size_t>(current-visible.begin());
    if((direction<0 && position==0) || (direction>0 && position+1==visible.size()))return;
    const auto next_position=direction<0?position-1:position+1;
    MoveTabTo(id,visible[next_position]);
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

std::vector<std::wstring> BrowserApp::Impl::WorkspaceNames() const {
    std::vector<std::wstring> names;
    names.reserve(workspaces_.size());
    for(const auto& workspace:workspaces_){
        const auto name=Wide(workspace.name);
        if(!name.empty())names.push_back(name);
    }
    if(names.empty())names.push_back(L"Main");
    return names;
}

SavedWorkspace* BrowserApp::Impl::FindWorkspace(std::wstring_view name) {
    const std::wstring wanted(name);
    for(auto& workspace:workspaces_)if(_wcsicmp(Wide(workspace.name).c_str(),wanted.c_str())==0)return &workspace;
    return nullptr;
}

const SavedWorkspace* BrowserApp::Impl::FindWorkspace(std::wstring_view name) const {
    const std::wstring wanted(name);
    for(const auto& workspace:workspaces_)if(_wcsicmp(Wide(workspace.name).c_str(),wanted.c_str())==0)return &workspace;
    return nullptr;
}

SavedWorkspace& BrowserApp::Impl::EnsureWorkspace(std::wstring_view name) {
    if(auto* existing=FindWorkspace(name))return *existing;
    std::wstring clean(name.empty()?L"Main":std::wstring(name));
    if(clean.size()>40)clean.resize(40);
    const auto utf=Utf8(clean);
    const auto hash=std::hash<std::string>{}(utf);
    if(workspaces_.size()>=32)return workspaces_.front();
    workspaces_.push_back({utf,static_cast<std::uint8_t>(hash%kProfileColors.size())});
    return workspaces_.back();
}

void BrowserApp::Impl::SwitchWorkspace(std::wstring_view name) {
    auto* workspace=FindWorkspace(name);
    if(!workspace)return;
    const auto next=Wide(workspace->name);
    if(_wcsicmp(next.c_str(),active_workspace_.c_str())==0)return;
    ClearSplitView();
    tab_reflow_start_=0;tab_reflow_from_.clear();
    active_workspace_=next;
    std::size_t target=tabs_.size();
    for(std::size_t i=0;i<tabs_.size();++i)if(_wcsicmp(tabs_[i]->workspace.c_str(),active_workspace_.c_str())==0){target=i;break;}
    if(target==tabs_.size()){
        Add(L"webcom:newtab",true,false);
    }else Select(target);
    first_visible_tab_=0;
    Dirty();UpdateUI(true);
    Note(L"Workspace: "+active_workspace_);
}

void BrowserApp::Impl::CreateWorkspace() {
    if(workspaces_.size()>=32){Notice(L"This profile already has 32 workspaces.");return;}
    const auto name=AskName(window_,instance_,&palette_,L"New workspace",L"&Workspace name",
        L"Create a separate tab space for a project, topic or context.",L"",L"Create");
    if(!name || name->empty())return;
    std::wstring clean=*name;if(clean.size()>40)clean.resize(40);
    if(FindWorkspace(clean)){Notice(L"A workspace with that name already exists.");return;}
    EnsureWorkspace(clean);active_workspace_=clean;first_visible_tab_=0;
    Add(L"webcom:newtab",true,false);Dirty();UpdateUI(true);
}

void BrowserApp::Impl::RenameWorkspace() {
    auto* workspace=FindWorkspace(active_workspace_);if(!workspace)return;
    const auto renamed=AskName(window_,instance_,&palette_,L"Rename workspace",L"&Workspace name",
        L"Tabs in this workspace keep their pages and groups.",active_workspace_.c_str(),L"Rename");
    if(!renamed || renamed->empty() || _wcsicmp(renamed->c_str(),active_workspace_.c_str())==0)return;
    std::wstring clean=*renamed;if(clean.size()>40)clean.resize(40);
    if(FindWorkspace(clean)){Notice(L"A workspace with that name already exists.");return;}
    const auto old=active_workspace_;workspace->name=Utf8(clean);
    for(auto& tab:tabs_)if(_wcsicmp(tab->workspace.c_str(),old.c_str())==0)tab->workspace=clean;
    active_workspace_=clean;Dirty();UpdateUI();
}

void BrowserApp::Impl::DeleteWorkspace() {
    if(workspaces_.size()<=1){Notice(L"WebCom must keep at least one workspace.");return;}
    const auto current=active_workspace_;
    if(MessageBoxW(window_,(L"Delete the ‘"+current+L"’ workspace?\n\nIts tabs will move to another workspace; no page data is deleted.").c_str(),
        L"Delete workspace",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)return;
    auto replacement=Wide(workspaces_.front().name);
    if(_wcsicmp(replacement.c_str(),current.c_str())==0)replacement=Wide(workspaces_[1].name);
    for(auto& tab:tabs_)if(_wcsicmp(tab->workspace.c_str(),current.c_str())==0)tab->workspace=replacement;
    std::erase_if(workspaces_,[&](const SavedWorkspace& workspace){return _wcsicmp(Wide(workspace.name).c_str(),current.c_str())==0;});
    active_workspace_=replacement;first_visible_tab_=0;ClearSplitView();
    std::size_t target=0;for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->workspace==replacement){target=i;break;}
    Select(target);Dirty();UpdateUI(true);
}

void BrowserApp::Impl::AssignTabWorkspace(std::uint64_t id,std::wstring workspace) {
    auto* tab=Find(id);if(!tab || workspace.empty())return;
    auto* target=FindWorkspace(workspace);if(!target)return;
    const auto next=Wide(target->name);
    if(_wcsicmp(tab->workspace.c_str(),next.c_str())==0)return;
    const bool active=IsActive(*tab);
    if(active)ClearSplitView();
    tab=Find(id);if(!tab)return;
    tab->workspace=next;
    if(active){
        active_workspace_=next;first_visible_tab_=0;
        for(std::size_t i=0;i<tabs_.size();++i)if(tabs_[i]->id==id){active_=i;break;}
        ApplyTabVisibility();Dirty();UpdateUI(true);
        Note(L"Moved tab to workspace: "+next);
    } else {Dirty();UpdateUI();}
}

void BrowserApp::Impl::ToggleVerticalTabsMode() {
    vertical_tabs_=!vertical_tabs_;first_visible_tab_=0;CancelTabDrag();
    tab_reflow_start_=0;tab_reflow_from_.clear();
    if(auto* profile=CurrentSavedProfile())profile->vertical_tabs=vertical_tabs_;
    Dirty();Layout();ApplyTabVisibility();InvalidateRect(window_,nullptr,FALSE);
    Note(vertical_tabs_?L"Vertical tabs on.":L"Horizontal tabs on.");
}

void BrowserApp::Impl::ToggleVerticalTabsCollapseMode() {
    if(!vertical_tabs_){
        vertical_tabs_=true;
        if(auto* profile=CurrentSavedProfile())profile->vertical_tabs=true;
    }
    vertical_tabs_collapsed_=!vertical_tabs_collapsed_;first_visible_tab_=0;CancelTabDrag();
    tab_reflow_start_=0;tab_reflow_from_.clear();
    if(auto* profile=CurrentSavedProfile())profile->vertical_tabs_collapsed=vertical_tabs_collapsed_;
    Dirty();Layout();ApplyTabVisibility();InvalidateRect(window_,nullptr,FALSE);
    Note(vertical_tabs_collapsed_?L"Vertical tab rail collapsed.":L"Vertical tab rail expanded.");
}

void BrowserApp::Impl::CycleWorkspace(int direction) {
    const auto names = WorkspaceNames();
    if (names.size() < 2 || direction == 0) return;
    std::size_t current = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (_wcsicmp(names[i].c_str(), active_workspace_.c_str()) == 0) { current = i; break; }
    }
    const auto count = static_cast<int>(names.size());
    int next = static_cast<int>(current) + (direction > 0 ? 1 : -1);
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;
    SwitchWorkspace(names[static_cast<std::size_t>(next)]);
}

void BrowserApp::Impl::WorkspaceMenuPopup() {
    if(menu_open_ || closing_ || !window_)return;
    const auto names=WorkspaceNames();
    const MenuItem divider{0,L"",L"",L"",false,false,true};
    std::vector<MenuItem> items;
    for(std::size_t i=0;i<names.size();++i){
        MenuItem item{WorkspaceBase+static_cast<UINT>(i),names[i],L"",L"\uE8B7",true,_wcsicmp(names[i].c_str(),active_workspace_.c_str())==0};
        if(const auto* meta=FindWorkspace(names[i]))item.swatch_rgb=kProfileColors[meta->color%kProfileColors.size()];
        items.push_back(std::move(item));
    }
    items.push_back(divider);
    items.push_back({AddWorkspace,L"New workspace...",L"",L"\uE710",workspaces_.size()<32});
    items.push_back({RenameWorkspaceCmd,L"Rename current workspace...",L"",L"\uE8AC"});
    items.push_back({DeleteWorkspaceCmd,L"Delete current workspace...",L"",L"\uE74D",workspaces_.size()>1});
    items.push_back(divider);
    static constexpr std::array<const wchar_t*,8> color_names{L"Blue",L"Green",L"Yellow",L"Red",L"Purple",L"Cyan",L"Orange",L"Gray"};
    const auto* current_workspace=FindWorkspace(active_workspace_);
    for(std::size_t i=0;i<color_names.size();++i){
        MenuItem color{WorkspaceColorBase+static_cast<UINT>(i),L"Workspace color: "+std::wstring(color_names[i]),L"",L"",true,current_workspace && current_workspace->color==i};
        color.swatch_rgb=kProfileColors[i];items.push_back(std::move(color));
    }
    items.push_back(divider);
    items.push_back({ToggleVerticalTabs,vertical_tabs_?L"Use horizontal tabs":L"Use vertical tabs",L"",L"\uE8A9",true,vertical_tabs_});
    if(vertical_tabs_)items.push_back({ToggleVerticalTabsCollapse,vertical_tabs_collapsed_?L"Expand vertical tab rail":L"Collapse vertical tab rail",L"",L"\uE8A9"});
    POINT anchor{};
    if(vertical_tabs_){anchor={D(8),ContentTop()+D(44)};ClientToScreen(window_,&anchor);}
    else {RECT more{};GetWindowRect(GetDlgItem(window_,More),&more);anchor={more.left,more.bottom};}
    UINT selected=0;
    {menu_open_=true;struct Reset{bool& v;~Reset(){v=false;}}reset{menu_open_};selected=ShowDarkMenu(window_,anchor,items,dpi_,340,false,&palette_);}
    if(!selected || closing_)return;
    if(selected>=WorkspaceBase && selected<WorkspaceBase+names.size())SwitchWorkspace(names[selected-WorkspaceBase]);
    else Dispatch(selected);
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
    std::vector<std::size_t> candidates;
    std::vector<std::pair<std::wstring,bool>> model;
    for(std::size_t i=0;i<tabs_.size();++i){
        if(_wcsicmp(tabs_[i]->workspace.c_str(),active_workspace_.c_str())!=0)continue;
        candidates.push_back(i);model.push_back({tabs_[i]->group,IsGroupCollapsed(tabs_[i]->group)});
    }
    const auto relative=MakeChromeVisibleTabIndices(model,std::wstring{});
    std::vector<std::size_t> result;result.reserve(relative.size());
    for(const auto index:relative)if(index<candidates.size())result.push_back(candidates[index]);
    return result;
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
    dragged_tab_id_=0;pressed_close_tab_id_=0;tab_dragging_=false;drag_target_index_=-1;drag_workspace_target_=-1;
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
        ComPtr<ICoreWebView2_14> certificate_api;
        if (SUCCEEDED(tab.web.As(&certificate_api))) certificate_api->remove_ServerCertificateErrorDetected(tab.certificate);
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
    if(split_tab_id_ && (id==split_tab_id_ || IsActive(**it)))ClearSplitView();
    if (extension_bootstrap_tab_id_ == id) extension_bootstrap_tab_id_ = 0;
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
    if (!tab.private_mode && !tab.ephemeral) {
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
        if(split_tab_id_) ClearSplitView();
        tab = Active();
        if(!tab) return;
        Release(*tab);
        tab->favicon.reset();
        tab->favicon_uri.clear();
        tab->title = InternalPageTitle(tab->url, tab->private_mode);
        native_scroll_ = 0;
        if (IsHome(tab->url)) SetWindowTextW(home_search_, L"");
        UpdateUI(true);
        if (InternalPageFor(tab->url) == InternalPage::Extensions) EnsureExtensionProfile();
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

void BrowserApp::Impl::OpenLocalFile() {
    if (closing_) return;
    wchar_t file[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = L"Web files (*.html;*.htm;*.pdf)\0*.html;*.htm;*.pdf\0HTML documents (*.html;*.htm)\0*.html;*.htm\0PDF documents (*.pdf)\0*.pdf\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = static_cast<DWORD>(std::size(file));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return;
    const auto resolved = ResolveAddress(Utf8(file));
    if (!resolved.allowed || !webcom::IsLocalFileAddress(resolved.url)) {
        Notice(L"WebCom opens local .html, .htm and .pdf files from local drives only.");
        return;
    }
    Navigate(file);
}

void BrowserApp::Impl::OpenExternalProtocol(std::wstring uri) {
    const auto resolved = ResolveExternalProtocol(Utf8(uri));
    if (!resolved.allowed) { Note(Wide(resolved.error)); return; }
    std::wstring visible = uri;
    if (visible.size() > 220) visible = visible.substr(0, 217) + L"...";
    const auto app_name = Wide(ExternalProtocolName(resolved.kind));
    const std::wstring message = L"Open this link in your " + app_name + L"?\n\n" + visible +
        L"\n\nOnly email, telephone and SMS links are allowed to leave WebCom automatically. Other custom schemes stay blocked.";
    if (MessageBoxW(window_, message.c_str(), L"Open external application",
        MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES || closing_) return;
    const auto launched = reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (launched <= 32) Notice(L"Windows could not find an application registered for this link type.");
}

BrowserApp::Impl::Tab* BrowserApp::Impl::SplitTab() {
    auto* tab=split_tab_id_?Find(split_tab_id_):nullptr;
    if(!tab || IsActive(*tab)){split_tab_id_=0;return nullptr;}
    return tab;
}

const BrowserApp::Impl::Tab* BrowserApp::Impl::SplitTab() const {
    if(!split_tab_id_)return nullptr;
    for(const auto& tab:tabs_)if(tab->id==split_tab_id_ && !IsActive(*tab))return tab.get();
    return nullptr;
}

bool BrowserApp::Impl::IsSplitVisible(const Tab& tab) const {
    return IsActive(tab) || (split_tab_id_ && tab.id==split_tab_id_);
}

RECT BrowserApp::Impl::ControllerBoundsFor(const Tab& tab) const {
    RECT bounds=ContentBounds();
    const auto* secondary=SplitTab();
    if(!secondary || (!IsActive(tab) && tab.id!=secondary->id))return bounds;
    const LONG gap=static_cast<LONG>(D(3));
    const LONG middle=bounds.left+(bounds.right-bounds.left)/2;
    if(IsActive(tab))bounds.right=std::max(bounds.left,middle-gap);
    else bounds.left=std::min(bounds.right,middle+gap);
    return bounds;
}

void BrowserApp::Impl::ApplyTabVisibility() {
    for(auto& item:tabs_)SetVisible(*item,IsSplitVisible(*item));
}

void BrowserApp::Impl::SetSplitTab(std::uint64_t id) {
    auto* primary=Active();auto* secondary=Find(id);
    if(!primary || !secondary || primary->id==secondary->id)return;
    if(primary->workspace!=secondary->workspace){Notice(L"Split view tabs must be in the same workspace.");return;}
    if(IsInternalPage(primary->url) || IsInternalPage(secondary->url)){
        Notice(L"Split view currently supports website tabs. Open both pages as websites first.");return;
    }
    split_tab_id_=secondary->id;
    Ensure(*primary);Ensure(*secondary);
    if(primary->activity.suspended){ComPtr<ICoreWebView2_3> life;if(SUCCEEDED(primary->web.As(&life)))life->Resume();primary->activity.suspended=false;}
    if(secondary->activity.suspended){ComPtr<ICoreWebView2_3> life;if(secondary->web && SUCCEEDED(secondary->web.As(&life)))life->Resume();secondary->activity.suspended=false;}
    ApplyTabVisibility();Dirty();UpdateUI();
    Note(L"Split view on. Select either split tab to make it primary.");
}

void BrowserApp::Impl::ClearSplitView() {
    const auto old=split_tab_id_;split_tab_id_=0;
    if(auto* secondary=Find(old))SetVisible(*secondary,false);
    if(auto* primary=Active())SetVisible(*primary,true);
    if(old){Dirty();UpdateUI();}
}

void BrowserApp::Impl::SetVisible(Tab& tab,bool visible) {
    if(!tab.controller)return;
    const bool show=visible && !IsIconic(window_);
    if(show){
        const RECT bounds=ControllerBoundsFor(tab);
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
            app->CaptureExtensionProfile(*current);
            app->Attach(*current);
            app->SetVisible(*current, app->IsSplitVisible(*current));
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

SavedProfile* BrowserApp::Impl::CurrentSavedProfile() {
    const auto found = std::find_if(state_.profiles.begin(), state_.profiles.end(),
        [&](const SavedProfile& profile) { return profile.name == profile_name_; });
    return found == state_.profiles.end() ? nullptr : &*found;
}

const SavedProfile* BrowserApp::Impl::CurrentSavedProfile() const {
    const auto found = std::find_if(state_.profiles.begin(), state_.profiles.end(),
        [&](const SavedProfile& profile) { return profile.name == profile_name_; });
    return found == state_.profiles.end() ? nullptr : &*found;
}

std::wstring BrowserApp::Impl::ProfileDisplayName(std::string_view profile, const Appearance* known) const {
    const Appearance loaded = known ? *known : LoadAppearance(root_, profile);
    return Wide(loaded.display_name.empty() ? std::string(profile) : loaded.display_name);
}

void BrowserApp::Impl::ConfigurePasswords(Tab& tab) {
    if (!tab.web) return;
    const auto* profile = CurrentSavedProfile();
    const bool save_passwords = profile ? profile->save_passwords : state_.save_passwords;
    const bool general_autofill = profile ? profile->general_autofill : true;
    ComPtr<ICoreWebView2Settings> settings;
    ComPtr<ICoreWebView2Settings4> credentials;
    if (SUCCEEDED(tab.web->get_Settings(&settings)) && SUCCEEDED(settings.As(&credentials))) {
        credentials->put_IsPasswordAutosaveEnabled(save_passwords && !tab.private_mode ? TRUE : FALSE);
        credentials->put_IsGeneralAutofillEnabled(general_autofill && !tab.private_mode ? TRUE : FALSE);
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
    const auto id = tab.id;
    const auto generation = tab.generation;
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
            if (address != L"about:blank" && !IsNavigableAddress(address)) {
                args->put_Cancel(TRUE);
                const auto external = ResolveExternalProtocol(Utf8(address));
                if (external.allowed) app->OpenExternalProtocol(address);
                else app->Note(L"WebCom blocked this non-web address. Only confirmed mailto:, tel: and sms: links may open another application.");
                return S_OK;
            }
            current->activity.loading = true;
            ++current->favicon_request;current->favicon_uri.clear();current->favicon.reset();
            current->cookies_known = false;
            current->cookies_loading = false;
            current->site_cookie_count = 0;
            current->site_cookies.clear();
            current->certificate_error.reset();
            current->certificate_details.reset();
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
                current->certificate_details.reset();
                app->RefreshSiteCookies(*current);
            }
            app->RequestFavicon(*current);
            if(successful) app->RecordHistory(*current);
            app->QueueUI();
            app->Dirty();
            return S_OK;
        }).Get(), &tab.completed);
    ComPtr<ICoreWebView2_14> certificate_api;
    if (SUCCEEDED(tab.web.As(&certificate_api))) {
        certificate_api->add_ServerCertificateErrorDetected(
            MakeCallback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
                [get](ICoreWebView2*, ICoreWebView2ServerCertificateErrorDetectedEventArgs* args) -> HRESULT {
                    auto [app, current] = get();
                    if (!current || !args) return S_OK;
                    COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                    if (FAILED(args->get_ErrorStatus(&status)) || !IsCertificateErrorStatus(status)) return S_OK;
                    CoString request;
                    args->get_RequestUri(&request.value);
                    const auto request_uri = request.str();
                    const auto request_origin = SiteOrigin(Utf8(request_uri));
                    const auto current_origin = SiteOrigin(Utf8(current->url));
                    if (!request_origin.empty() && !current_origin.empty() && request_origin != current_origin) return S_OK;
                    current->certificate_error = status;
                    CertificateDetails details;
                    details.request_uri = request_uri;
                    ComPtr<ICoreWebView2Certificate> certificate;
                    if (SUCCEEDED(args->get_ServerCertificate(&certificate)) && certificate) {
                        CoString subject, issuer;
                        certificate->get_Subject(&subject.value);
                        certificate->get_Issuer(&issuer.value);
                        certificate->get_ValidFrom(&details.valid_from);
                        certificate->get_ValidTo(&details.valid_to);
                        details.subject = subject.str();
                        details.issuer = issuer.str();
                    }
                    current->certificate_details = std::move(details);
                    app->QueueUI();
                    return S_OK;
                }).Get(), &tab.certificate);
    }
    tab.web->add_SourceChanged(MakeCallback<ICoreWebView2SourceChangedEventHandler>(
        [get](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
            auto [app, current] = get();
            if (!current) return S_OK;
            CoString source;
            if (SUCCEEDED(sender->get_Source(&source.value))) {
                const auto url = source.str();
                if (IsNavigableAddress(url) || url == L"about:blank") {
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
            if (!IsNavigableAddress(url) && url != L"about:blank" && !url.empty()) {
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
    if (!tab.web || IsSplitVisible(tab) || tab.activity.loading || tab.activity.audible || tab.activity.pinned ||
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
            if (app->IsSplitVisible(*current) || current->activity.loading || current->activity.pinned || current->activity.protected_from_sleep ||
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
        if (IsSplitVisible(*tab)) { tab->activity.last_active_ms = now; continue; }
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
        const auto a = line.find('\t');
        const auto b = a == std::string::npos ? std::string::npos : line.find('\t', a + 1);
        if(a==std::string::npos || b==std::string::npos) continue;
        std::uint64_t when=0;
        try { when=std::stoull(line.substr(0,a)); } catch (...) { continue; }
        const auto url=UnescapeField(std::string_view(line).substr(a+1,b-a-1));
        const auto title=UnescapeField(std::string_view(line).substr(b+1));
        const auto wide_url = Wide(url);
        const auto wide_title = Wide(title);
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
    if (!TryGetCookieManager(tab.web, manager)) return;
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
    if(!TryGetCookieManager(tab->web, manager)){ Note(L"Cookies could not be accessed for this site."); return; }
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
    if (tab && IsLocalFileUrl(tab->url)) {
        MessageBoxW(window_,
            L"This page is a local file on your device.\n\nWebCom does not store local-file visits in History and does not expose website cookie or permission controls for local files. Broad file-to-file access is not enabled.",
            L"Local file", MB_OK | MB_ICONINFORMATION);
        return;
    }
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
    if (tab->certificate_details)
        items.push_back({ShowCertificateDetailsCmd,L"Certificate details",L"Subject, issuer and validity",L"\uE946"});
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
    if(selected==ShowCertificateDetailsCmd){ ShowCertificateDetails(); return; }
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

void BrowserApp::Impl::ShowCertificateDetails() {
    const auto* tab = Active();
    if (!tab || !tab->certificate_error || !tab->certificate_details) {
        Notice(L"Detailed certificate metadata is available when WebView2 reports a certificate verification problem.");
        return;
    }
    const auto& certificate = *tab->certificate_details;
    std::wostringstream body;
    body << L"Certificate verification problem\n\n"
         << CertificateErrorLabel(*tab->certificate_error) << L"\n\n"
         << L"Requested address\n" << (certificate.request_uri.empty() ? tab->url : certificate.request_uri) << L"\n\n"
         << L"Subject\n" << (certificate.subject.empty() ? L"Unavailable" : certificate.subject) << L"\n\n"
         << L"Issuer\n" << (certificate.issuer.empty() ? L"Unavailable" : certificate.issuer) << L"\n\n"
         << L"Valid from\n" << CertificateTimeLabel(certificate.valid_from) << L"\n\n"
         << L"Valid until\n" << CertificateTimeLabel(certificate.valid_to) << L"\n\n"
         << L"WebCom does not bypass certificate errors. WebView2's normal TLS error handling remains in control.";
    MessageBoxW(window_, body.str().c_str(), L"WebCom - Certificate details", MB_OK | MB_ICONWARNING);
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

void BrowserApp::Impl::SaveNamedSessionsNow() {
    std::string error;
    if (!SaveNamedSessions(root_, profile_name_, named_sessions_, &error))
        Notice(L"Saved sessions could not be written. " + Wide(error));
}

void BrowserApp::Impl::SaveCurrentWorkspaceSession() {
    std::vector<NamedSessionTab> saved_tabs;
    for (const auto& tab : tabs_) {
        if (tab->private_mode || _wcsicmp(tab->workspace.c_str(), active_workspace_.c_str()) != 0 || !IsWebAddress(tab->url)) continue;
        saved_tabs.push_back({Utf8(tab->url), Utf8(tab->title), tab->activity.pinned, Utf8(tab->workspace)});
    }
    if (saved_tabs.empty()) { Notice(L"Open at least one website tab in this workspace before saving a session."); return; }
    const auto name = AskName(window_, instance_, &palette_, L"Save named session", L"&Session name",
        L"Saves the website tabs in the current workspace. Reopening a session creates new tabs.", active_workspace_.c_str(), L"Save");
    if (!name || name->empty()) return;
    const auto utf_name = Utf8(*name);
    const auto existing = std::find_if(named_sessions_.begin(), named_sessions_.end(), [&](const NamedSession& item) {
        return _stricmp(item.name.c_str(), utf_name.c_str()) == 0;
    });
    if (existing != named_sessions_.end()) {
        const auto question = L"Replace the existing saved session ‘" + *name + L"’?";
        if (MessageBoxW(window_, question.c_str(), L"Replace saved session", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
    }
    NamedSession session;
    session.name = utf_name;
    session.created_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    session.tabs = std::move(saved_tabs);
    std::string error;
    if (!UpsertNamedSession(named_sessions_, std::move(session), &error)) { Notice(Wide(error)); return; }
    if (!SaveNamedSessions(root_, profile_name_, named_sessions_, &error)) { Notice(L"The session could not be saved. " + Wide(error)); return; }
    native_scroll_ = 0; QueueUI(); Note(L"Saved current workspace as a named session.");
}

void BrowserApp::Impl::RestoreNamedSession(std::size_t index) {
    if (index >= named_sessions_.size()) return;
    const auto session = named_sessions_[index];
    if (session.tabs.empty()) return;
    std::uint64_t first_id = 0;
    for (const auto& saved : session.tabs) {
        const auto workspace = Wide(saved.workspace.empty() ? std::string("Main") : saved.workspace);
        EnsureWorkspace(workspace);
        const auto id = Add(Wide(saved.url), false, false);
        if (auto* tab = Find(id)) {
            tab->workspace = workspace;
            tab->activity.pinned = saved.pinned;
            if (!saved.title.empty()) tab->title = Wide(saved.title);
        }
        if (!first_id) first_id = id;
    }
    if (auto* first = Find(first_id)) {
        active_workspace_ = first->workspace;
        for (std::size_t i = 0; i < tabs_.size(); ++i) if (tabs_[i]->id == first_id) { Select(i); break; }
    }
    first_visible_tab_ = 0; Dirty(); UpdateUI(true); Note(L"Opened saved session: " + Wide(session.name));
}

void BrowserApp::Impl::DeleteNamedSessionUi(std::size_t index) {
    if (index >= named_sessions_.size()) return;
    const auto name = Wide(named_sessions_[index].name);
    const auto question = L"Delete the saved session ‘" + name + L"’?\n\nOpen browser tabs are not affected.";
    if (MessageBoxW(window_, question.c_str(), L"Delete saved session", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
    named_sessions_.erase(named_sessions_.begin() + static_cast<std::ptrdiff_t>(index));
    SaveNamedSessionsNow(); native_scroll_ = 0; QueueUI(); Note(L"Saved session deleted.");
}

std::vector<std::string> BrowserApp::Impl::PrivacyPermissionOrigins() const {
    std::vector<std::string> origins;
    for (const auto& rule : site_permissions_) {
        if (rule.origin.empty()) continue;
        if (std::find(origins.begin(), origins.end(), rule.origin) == origins.end()) origins.push_back(rule.origin);
    }
    std::sort(origins.begin(), origins.end());
    return origins;
}

void BrowserApp::Impl::RefreshPrivacyManager() {
    if (privacy_loading_ || closing_) return;
    Tab* source = nullptr;
    for (auto& tab : tabs_) if (!tab->private_mode && tab->web) { source = tab.get(); break; }
    if (!source) { privacy_loaded_ = false; privacy_cookie_domains_.clear(); QueueUI(); return; }
    ComPtr<ICoreWebView2CookieManager> manager;
    if (!TryGetCookieManager(source->web, manager)) { privacy_loaded_ = false; privacy_cookie_domains_.clear(); QueueUI(); return; }
    privacy_loading_ = true;
    const auto weak = weak_from_this();
    const HRESULT result = manager->GetCookies(L"", MakeCallback<ICoreWebView2GetCookiesCompletedHandler>(
        [weak](HRESULT error, ICoreWebView2CookieList* list) -> HRESULT {
            auto app = weak.lock();
            if (!app || app->closing_) return S_OK;
            app->privacy_loading_ = false;
            app->privacy_cookie_domains_.clear();
            app->privacy_loaded_ = SUCCEEDED(error) && list;
            if (app->privacy_loaded_) {
                UINT count = 0;
                if (FAILED(list->get_Count(&count))) app->privacy_loaded_ = false;
                for (UINT i = 0; app->privacy_loaded_ && i < count; ++i) {
                    ComPtr<ICoreWebView2Cookie> cookie;
                    if (FAILED(list->GetValueAtIndex(i, &cookie)) || !cookie) continue;
                    CoString domain; if (FAILED(cookie->get_Domain(&domain.value))) continue;
                    auto value = domain.str(); if (value.empty()) continue;
                    auto found = std::find_if(app->privacy_cookie_domains_.begin(), app->privacy_cookie_domains_.end(), [&](const PrivacyCookieDomain& item) {
                        return _wcsicmp(item.domain.c_str(), value.c_str()) == 0;
                    });
                    if (found == app->privacy_cookie_domains_.end()) app->privacy_cookie_domains_.push_back({value, 1});
                    else ++found->count;
                }
                std::sort(app->privacy_cookie_domains_.begin(), app->privacy_cookie_domains_.end(), [](const auto& a, const auto& b) {
                    return _wcsicmp(a.domain.c_str(), b.domain.c_str()) < 0;
                });
            }
            app->native_scroll_ = 0; app->QueueUI();
            return S_OK;
        }).Get());
    if (FAILED(result)) { privacy_loading_ = false; privacy_loaded_ = false; QueueUI(); }
}

void BrowserApp::Impl::ClearPrivacyCookieDomain(std::size_t index) {
    if (index >= privacy_cookie_domains_.size()) return;
    Tab* source = nullptr;
    for (auto& tab : tabs_) if (!tab->private_mode && tab->web) { source = tab.get(); break; }
    if (!source) { Notice(L"Open a normal website tab once, then try again."); return; }
    ComPtr<ICoreWebView2CookieManager> manager;
    if (!TryGetCookieManager(source->web, manager)) { Notice(L"Cookies are not available from this WebView2 Runtime."); return; }
    const auto domain = privacy_cookie_domains_[index].domain;
    const auto question = L"Clear cookies for domain ‘" + domain + L"’?\n\nThis can sign you out of websites using that cookie domain.";
    if (MessageBoxW(window_, question.c_str(), L"Clear site cookies", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return;
    const auto weak = weak_from_this();
    const HRESULT result = manager->GetCookies(L"", MakeCallback<ICoreWebView2GetCookiesCompletedHandler>(
        [weak, manager, domain](HRESULT error, ICoreWebView2CookieList* list) -> HRESULT {
            auto app = weak.lock(); if (!app || app->closing_) return S_OK;
            if (FAILED(error) || !list) { app->Notice(L"The cookie list could not be read."); return S_OK; }
            UINT count = 0, removed = 0; if (FAILED(list->get_Count(&count))) return S_OK;
            for (UINT i = 0; i < count; ++i) {
                ComPtr<ICoreWebView2Cookie> cookie; if (FAILED(list->GetValueAtIndex(i, &cookie)) || !cookie) continue;
                CoString saved_domain; if (FAILED(cookie->get_Domain(&saved_domain.value))) continue;
                if (_wcsicmp(saved_domain.str().c_str(), domain.c_str()) == 0 && SUCCEEDED(manager->DeleteCookie(cookie.Get()))) ++removed;
            }
            app->privacy_loaded_ = false; app->RefreshPrivacyManager();
            app->Note(L"Cleared " + std::to_wstring(removed) + L" cookie" + (removed == 1 ? L"." : L"s."));
            return S_OK;
        }).Get());
    if (FAILED(result)) Notice(L"WebCom could not start the cookie clear operation.");
}

void BrowserApp::Impl::ResetPrivacyPermissionOrigin(std::size_t index) {
    const auto origins = PrivacyPermissionOrigins();
    if (index >= origins.size()) return;
    const auto origin = origins[index];
    ResetSitePermissions(site_permissions_, origin);
    SaveSitePermissionsNow(); native_scroll_ = 0; QueueUI();
    Note(L"Permission exceptions reset for " + Wide(origin) + L". Reload open pages if needed.");
}

void BrowserApp::Impl::OpenReaderMode() {
    auto* source = Active();
    if (!source || !source->web || !IsWebAddress(source->url)) { Notice(L"Open a normal webpage first."); return; }
    const auto source_id = source->id;
    const auto source_generation = source->generation;
    const auto source_url = source->url;
    const auto source_title = source->title;
    const auto source_workspace = source->workspace;
    const bool source_private = source->private_mode;
    static constexpr wchar_t script[] =
        L"(() => {"
        L"const root=document.querySelector('article,main,[role=main]')||document.body;"
        L"if(!root)return '';"
        L"const copy=root.cloneNode(true);"
        L"copy.querySelectorAll('script,style,noscript,nav,header,footer,aside,form,button,svg,canvas,video,audio').forEach(e=>e.remove());"
        L"let text=(copy.innerText||copy.textContent||'').replace(/\\u00a0/g,' ').replace(/[ \\t]+\\n/g,'\\n').replace(/\\n{3,}/g,'\\n\\n').trim();"
        L"return text.slice(0,300000);"
        L"})()";
    const auto weak = weak_from_this();
    const HRESULT result = source->web->ExecuteScript(script,
        MakeCallback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [weak, source_id, source_generation, source_url, source_title, source_workspace, source_private](HRESULT error, LPCWSTR json) -> HRESULT {
                auto app = weak.lock(); if (!app || app->closing_) return S_OK;
                auto* live = app->Find(source_id);
                if (!live || live->generation != source_generation || FAILED(error)) { app->Notice(L"Reader Mode could not extract this page."); return S_OK; }
                const auto text = DecodeJsonString(json ? std::wstring_view(json) : std::wstring_view{});
                if (!text || text->size() < 80) { app->Notice(L"Reader Mode could not find enough readable text on this page."); return S_OK; }
                const auto id = app->Add(L"webcom:reader", true, false);
                if (auto* reader = app->Find(id)) {
                    reader->ephemeral = true;
                    reader->reader_source = source_url;
                    reader->reader_source_tab_id = source_id;
                    reader->reader_text = *text;
                    reader->reader_text_scale = app->appearance_.reader_text_scale;
                    reader->reader_width_mode = app->appearance_.reader_width_mode;
                    reader->reader_serif = app->appearance_.reader_serif;
                    reader->workspace = source_workspace;
                    reader->title = source_title.empty() ? L"Reader Mode" : L"Reader: " + source_title;
                    reader->activity.last_active_ms = GetTickCount64();
                    app->active_workspace_ = source_workspace;
                    app->native_scroll_ = 0;
                    app->UpdateUI(true);
                    app->Note(source_private ? L"Reader Mode opened from an Incognito page. The extracted text stays only in memory." : L"Reader Mode opened in a temporary native tab.");
                }
                return S_OK;
            }).Get());
    if (FAILED(result)) Notice(L"Reader Mode could not start. " + HResultText(result));
}

void BrowserApp::Impl::PersistReaderDefaults(const Tab& reader) {
    if (InternalPageFor(reader.url) != InternalPage::Reader) return;
    auto next = appearance_;
    next.reader_text_scale = std::clamp(reader.reader_text_scale, -2, 4);
    next.reader_width_mode = std::clamp(reader.reader_width_mode, 0, 2);
    next.reader_serif = reader.reader_serif;
    if (next.reader_text_scale == appearance_.reader_text_scale &&
        next.reader_width_mode == appearance_.reader_width_mode &&
        next.reader_serif == appearance_.reader_serif) return;
    std::string error;
    if (!SaveAppearance(root_, profile_name_, next, &error)) {
        Note(L"Reader defaults changed for this tab, but could not be saved. " + Wide(error));
        return;
    }
    appearance_ = std::move(next);
}

void BrowserApp::Impl::ReturnFromReader() {
    auto* reader = Active(); if (!reader || InternalPageFor(reader->url) != InternalPage::Reader) return;
    if (reader->reader_source_tab_id) {
        for (std::size_t i = 0; i < tabs_.size(); ++i) if (tabs_[i]->id == reader->reader_source_tab_id) { Select(i); return; }
    }
    if (IsWebAddress(reader->reader_source)) Add(reader->reader_source, true, false);
}

void BrowserApp::Impl::CopyReaderText() {
    auto* reader = Active();
    if (!reader || InternalPageFor(reader->url) != InternalPage::Reader || reader->reader_text.empty()) return;
    if (!OpenClipboard(window_)) { Notice(L"The clipboard is currently unavailable."); return; }
    struct ClipboardCloser { ~ClipboardCloser(){ CloseClipboard(); } } closer;
    if (!EmptyClipboard()) { Notice(L"The clipboard could not be cleared."); return; }
    const std::size_t bytes = (reader->reader_text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) { Notice(L"Not enough memory to copy the Reader text."); return; }
    void* data = GlobalLock(memory);
    if (!data) { GlobalFree(memory); Notice(L"The Reader text could not be copied."); return; }
    memcpy(data, reader->reader_text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory); Notice(L"The Reader text could not be copied."); return;
    }
    Note(L"Reader text copied to the clipboard.");
}

void BrowserApp::Impl::SaveReaderText() {
    auto* reader = Active();
    if (!reader || InternalPageFor(reader->url) != InternalPage::Reader || reader->reader_text.empty()) return;
    wchar_t file[MAX_PATH]=L"WebCom-reader.txt";
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"Text document (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=file; dialog.nMaxFile=MAX_PATH; dialog.lpstrDefExt=L"txt";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog)) return;
    const std::string utf = Utf8(reader->reader_text);
    std::ofstream output(std::filesystem::path(file),std::ios::binary|std::ios::trunc);
    static constexpr unsigned char bom[]{0xef,0xbb,0xbf};
    if (!output || !output.write(reinterpret_cast<const char*>(bom),3) ||
        !output.write(utf.data(),static_cast<std::streamsize>(utf.size()))) {
        output.close(); DeleteFileW(file); Notice(L"The Reader text could not be saved."); return;
    }
    output.close(); Note(L"Reader text saved: "+std::filesystem::path(file).filename().wstring());
}


void BrowserApp::Impl::ReaderExportMenuPopup() {
    auto* reader = Active();
    if (!reader || InternalPageFor(reader->url) != InternalPage::Reader || reader->reader_text.empty()) return;
    const std::vector<MenuItem> items{
        {ReaderSaveText,L"Plain text (.txt)",L"UTF-8 text",L"\uE8A5"},
        {ReaderSaveHtml,L"Reader HTML (.html)",L"Print-friendly formatting",L"\uE8A5"},
        {ReaderSaveMarkdown,L"Markdown (.md)",L"Title, source and article text",L"\uE8A5"}
    };
    RECT client{}; GetClientRect(window_,&client);
    POINT anchor{std::max(D(8),static_cast<int>(client.right)/2-D(180)),fullscreen_?D(20):D(120)};
    ClientToScreen(window_,&anchor);
    menu_open_=true; struct Reset{bool& value;~Reset(){value=false;}} reset{menu_open_};
    const UINT selected=ShowDarkMenu(window_,anchor,items,dpi_,360,false,&palette_);
    if(selected && !closing_) Dispatch(selected);
}

void BrowserApp::Impl::SaveReaderHtml() {
    auto* reader=Active();
    if(!reader || InternalPageFor(reader->url)!=InternalPage::Reader || reader->reader_text.empty())return;
    wchar_t file[MAX_PATH]=L"WebCom-reader.html";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"HTML document (*.html)\0*.html\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"html";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    std::wstring title=reader->title.starts_with(L"Reader: ")?reader->title.substr(8):reader->title;
    const auto html=BuildReaderHtml(title,reader->reader_source,reader->reader_text,reader->reader_text_scale,reader->reader_width_mode,reader->reader_serif);
    std::ofstream output(std::filesystem::path(file),std::ios::binary|std::ios::trunc);
    if(!output || !output.write(html.data(),static_cast<std::streamsize>(html.size()))){output.close();DeleteFileW(file);Notice(L"The Reader HTML could not be saved.");return;}
    output.close();Note(L"Reader HTML saved: "+std::filesystem::path(file).filename().wstring());
}

void BrowserApp::Impl::SaveReaderMarkdown() {
    auto* reader=Active();
    if(!reader || InternalPageFor(reader->url)!=InternalPage::Reader || reader->reader_text.empty())return;
    wchar_t file[MAX_PATH]=L"WebCom-reader.md";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"Markdown document (*.md)\0*.md\0Text document (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"md";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    std::wstring title=reader->title.starts_with(L"Reader: ")?reader->title.substr(8):reader->title;
    const auto markdown=BuildReaderMarkdown(title,reader->reader_source,reader->reader_text);
    std::ofstream output(std::filesystem::path(file),std::ios::binary|std::ios::trunc);
    static constexpr unsigned char bom[]{0xef,0xbb,0xbf};
    if(!output || !output.write(reinterpret_cast<const char*>(bom),3) || !output.write(markdown.data(),static_cast<std::streamsize>(markdown.size()))){output.close();DeleteFileW(file);Notice(L"The Reader Markdown could not be saved.");return;}
    output.close();Note(L"Reader Markdown saved: "+std::filesystem::path(file).filename().wstring());
}

DiagnosticsSnapshot BrowserApp::Impl::DiagnosticsSnapshotNow() const {
    DiagnosticsSnapshot snapshot;
    snapshot.version=L"2.0.0";
    if(environment_){CoString runtime; if(SUCCEEDED(environment_->get_BrowserVersionString(&runtime.value)))snapshot.runtime_version=runtime.str();}
    snapshot.profile_key=Wide(profile_name_);
    snapshot.profile_label=ProfileDisplayName(profile_name_,&appearance_);
    snapshot.update_channel=Wide(ReleaseChannelName(update_settings_.channel));
    snapshot.active_workspace=active_workspace_;
    snapshot.data_path=root_.wstring();
    snapshot.tabs=static_cast<unsigned>(tabs_.size());
    snapshot.workspaces=static_cast<unsigned>(workspaces_.size());
    snapshot.vertical_tabs=vertical_tabs_;
    snapshot.reduced_motion=appearance_.reduce_motion;
    snapshot.high_contrast=appearance_.high_contrast || SystemHighContrastEnabled();
    for(const auto& tab:tabs_){if(tab->web)++snapshot.web_tabs;if(tab->activity.suspended)++snapshot.sleeping_tabs;}
    return snapshot;
}

std::wstring BrowserApp::Impl::DiagnosticsTextNow() const { return BuildDiagnosticsText(DiagnosticsSnapshotNow()); }

std::uint64_t BrowserApp::Impl::UpdateRetryRemainingNow() const {
    return UpdateRetryRemainingSeconds(update_retry_, UnixNowSeconds());
}

ReliabilityReportSnapshot BrowserApp::Impl::ReliabilitySnapshotNow() const {
    ReliabilityReportSnapshot snapshot;
    snapshot.version = std::string(kWebComVersion);
    if (auto session = session_.lock()) snapshot.previous_session_unclean = session->PreviousRuntimeUnclean();
    snapshot.update_phase = UpdateTransactionPhaseName(update_transaction_.phase);
    snapshot.update_target = update_transaction_.target_version;
    snapshot.update_previous = update_transaction_.previous_version;
    snapshot.failed_health_starts = update_transaction_.failed_health_starts;
    snapshot.rollback_recommended = UpdateRollbackRecommended(update_transaction_);
    if (recovery_package_) snapshot.recovery_version = recovery_package_->version;
    snapshot.update_retry_failures = update_retry_.consecutive_failures;
    snapshot.update_retry_remaining_seconds = static_cast<unsigned>(std::min<std::uint64_t>(UpdateRetryRemainingNow(), 86400));
    snapshot.update_retry_category = update_retry_.last_failure_category;
    return snapshot;
}

void BrowserApp::Impl::ExportRecoveryReportNow() {
    wchar_t file[MAX_PATH]{};
    wcscpy_s(file,L"WebCom-recovery-report.zip");
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"ZIP archive (*.zip)\0*.zip\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"zip";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    std::vector<DiagnosticsBundleEntry> entries{
        {"recovery.txt",BuildReliabilityReportText(ReliabilitySnapshotNow())},
        {"diagnostics.txt",Utf8(DiagnosticsTextNow())},
        {"privacy.txt",ReliabilityReportPrivacyText()}
    };
    const auto runtime=ReadRuntimeLogBundleText(root_);
    if(!runtime.empty())entries.push_back({"runtime.txt",runtime});
    std::string error;
    const auto zip=BuildDiagnosticsZip(entries,&error);
    if(!zip){Notice(L"Recovery report could not be created. "+Wide(error));return;}
    std::ofstream output(std::filesystem::path(file),std::ios::binary|std::ios::trunc);
    if(!output){Notice(L"Recovery report could not be written.");return;}
    output.write(reinterpret_cast<const char*>(zip->data()),static_cast<std::streamsize>(zip->size()));
    output.close();
    if(!output){DeleteFileW(file);Notice(L"Recovery report could not be finished.");return;}
    AppendRuntimeLog(root_,RuntimeLogLevel::Info,"support.recovery_report_exported","explicit local export",nullptr);
    Note(L"Recovery report saved: "+std::filesystem::path(file).filename().wstring());
}

void BrowserApp::Impl::CopyDiagnosticsText() {
    const auto text=DiagnosticsTextNow();
    if(!OpenClipboard(window_)){Notice(L"The clipboard is currently unavailable.");return;}
    struct ClipboardCloser{~ClipboardCloser(){CloseClipboard();}} closer;
    if(!EmptyClipboard()){Notice(L"The clipboard could not be cleared.");return;}
    const std::size_t bytes=(text.size()+1)*sizeof(wchar_t);
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory){Notice(L"Not enough memory to copy diagnostics.");return;}
    void* data=GlobalLock(memory);if(!data){GlobalFree(memory);Notice(L"Diagnostics could not be copied.");return;}
    memcpy(data,text.c_str(),bytes);GlobalUnlock(memory);
    if(!SetClipboardData(CF_UNICODETEXT,memory)){GlobalFree(memory);Notice(L"Diagnostics could not be copied.");return;}
    Note(L"Support diagnostics copied to the clipboard.");
}

void BrowserApp::Impl::ExportDiagnosticsBundleNow() {
    wchar_t file[MAX_PATH]{};
    wcscpy_s(file,L"WebCom-support-diagnostics.zip");
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"ZIP archive (*.zip)\0*.zip\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.lpstrDefExt=L"zip";
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&dialog))return;
    const int include_log=MessageBoxW(window_,
        L"Include WebCom's bounded local runtime log in this support ZIP?\n\nThe log records browser/update lifecycle events and redacts web/file URLs. It does not intentionally record cookie values, passwords or form contents. Choose No for diagnostics without the log.",
        L"Include runtime log?",MB_YESNOCANCEL|MB_DEFBUTTON2|MB_ICONQUESTION);
    if(include_log==IDCANCEL)return;
    std::vector<DiagnosticsBundleEntry> entries{
        {"diagnostics.txt",Utf8(DiagnosticsTextNow())},
        {"updates.txt",std::string("Release channel: ")+ReleaseChannelName(update_settings_.channel)+"\r\nUpdate checks consent: "+(update_settings_.checks_enabled?"enabled":"disabled")+"\r\nManifest endpoint configured: "+(ManifestUrlForChannel(update_runtime_config_,update_settings_.channel)?"yes":"no")+"\r\nPinned publisher configured: "+(IsTrustedPublisherFingerprint(update_runtime_config_.publisher_sha256)?"yes":"no")+"\r\nTransaction phase: "+UpdateTransactionPhaseName(update_transaction_.phase)+"\r\nRecovery installer: "+(recovery_package_?("preserved "+recovery_package_->version):"not available")+"\r\nUpdate retry failures: "+std::to_string(update_retry_.consecutive_failures)+"\r\nUpdate retry remaining seconds: "+std::to_string(UpdateRetryRemainingNow())+"\r\nPrevious session unclean: "+((session_.lock()&&session_.lock()->PreviousRuntimeUnclean())?"yes":"no")+"\r\nAutomatic download: disabled\r\nAutomatic install: disabled\r\nAutomatic rollback: disabled\r\nSigned manifest schema: v2 Authenticode + pinned publisher SHA-256\r\n"},
        {"privacy.txt","This support bundle intentionally excludes browsing URLs, cookie values, saved password values, form contents and authentication data.\r\nThe optional runtime log redacts web/file URLs but can contain operational error/status text; review the files before sharing them.\r\n"}
    };
    if(include_log==IDYES){const auto runtime=ReadRuntimeLogBundleText(root_);if(!runtime.empty())entries.push_back({"runtime.txt",runtime});}
    std::string error;
    const auto zip=BuildDiagnosticsZip(entries,&error);
    if(!zip){Notice(L"Diagnostics bundle could not be created. "+Wide(error));return;}
    std::ofstream output(std::filesystem::path(file),std::ios::binary|std::ios::trunc);
    if(!output){Notice(L"Diagnostics bundle could not be written.");return;}
    output.write(reinterpret_cast<const char*>(zip->data()),static_cast<std::streamsize>(zip->size()));
    output.close();
    if(!output){DeleteFileW(file);Notice(L"Diagnostics bundle could not be finished.");return;}
    Note(L"Support diagnostics bundle saved: "+std::filesystem::path(file).filename().wstring());
}

void BrowserApp::Impl::OpenDataFolderNow() {
    const auto path=root_.wstring();
    if(reinterpret_cast<INT_PTR>(ShellExecuteW(window_,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
        Notice(L"The WebCom data folder could not be opened.");
}

void BrowserApp::Impl::SetUpdateChannel(ReleaseChannel channel) {
    if(update_settings_.channel==channel)return;
    UpdateSettings next=update_settings_;next.channel=channel;std::string error;
    if(!SaveUpdateSettings(root_,next,&error)){Notice(L"Update channel could not be saved. "+Wide(error));return;}
    update_settings_=next;available_update_.reset();update_runtime_status_.clear();
    ResetUpdateRetry(update_retry_);SaveUpdateRetryState(root_,update_retry_,nullptr);QueueUI();
    Note(channel==ReleaseChannel::Beta?L"Update channel set to Beta.":L"Update channel set to Stable.");
}

void BrowserApp::Impl::StartUpdateCheck() {
    if (update_checking_ || update_staging_) return;
    const auto retry_remaining = UpdateRetryRemainingNow();
    if (retry_remaining) {
        update_runtime_status_ = L"Update checks are temporarily rate-limited after repeated failures. Try again in " + std::to_wstring(retry_remaining) + L" second" + (retry_remaining==1?L".":L"s.");
        AppendRuntimeLog(root_,RuntimeLogLevel::Warning,"update.check.backoff","remaining_seconds="+std::to_string(retry_remaining),nullptr);
        QueueUI(); Notice(update_runtime_status_); return;
    }
    if (!update_settings_.checks_enabled) {
        Notice(L"Enable update checks first. WebCom only contacts the configured release endpoint after your consent.");
        return;
    }
    const auto endpoint = ManifestUrlForChannel(update_runtime_config_, update_settings_.channel);
    if (!endpoint || !IsTrustedPublisherFingerprint(update_runtime_config_.publisher_sha256)) {
        update_runtime_status_ = L"This build has no trusted update endpoint/publisher configured.";
        QueueUI();
        Notice(update_runtime_status_ + L" Release builds must set the manifest URL and pinned publisher SHA-256 at build time.");
        return;
    }
    update_checking_ = true;
    AppendRuntimeLog(root_,RuntimeLogLevel::Info,"update.check.start","channel="+ReleaseChannelName(update_settings_.channel),nullptr);
    available_update_.reset();
    update_runtime_status_ = L"Checking the signed release manifest...";
    QueueUI();
    const HWND target = window_;
    const auto config = update_runtime_config_;
    const auto channel = update_settings_.channel;
    const std::string url = *endpoint;
    std::thread([target, config, channel, url] {
        auto* completion = new UpdateCheckCompletion();
        std::string error;
        const auto text = FetchUpdateManifestHttps(url, 64u * 1024u, &error);
        if (!text) {
            completion->evaluation.status = UpdateCheckStatus::ManifestRejected;
            completion->evaluation.message = error.empty() ? "The release manifest could not be downloaded." : error;
        } else {
            completion->evaluation = EvaluateUpdateManifest(kWebComVersion, channel, config, *text);
        }
        if (!PostMessageW(target, kUpdateCheckComplete, 0, reinterpret_cast<LPARAM>(completion))) delete completion;
    }).detach();
}

void BrowserApp::Impl::HandleUpdateCheckCompletion(UpdateCheckCompletion* raw) {
    std::unique_ptr<UpdateCheckCompletion> completion(raw);
    update_checking_ = false;
    if (!completion) return;
    update_runtime_status_ = Wide(completion->evaluation.message);
    const bool check_succeeded = completion->evaluation.status==UpdateCheckStatus::UpdateAvailable || completion->evaluation.status==UpdateCheckStatus::NoUpdate;
    std::string retry_error;
    if (check_succeeded) {
        ResetUpdateRetry(update_retry_);
        SaveUpdateRetryState(root_,update_retry_,&retry_error);
    } else {
        const std::string category = "status-" + std::to_string(static_cast<int>(completion->evaluation.status));
        if (RegisterUpdateRetryFailure(update_retry_,UnixNowSeconds(),category,&retry_error))
            SaveUpdateRetryState(root_,update_retry_,&retry_error);
    }
    AppendRuntimeLog(root_,completion->evaluation.status==UpdateCheckStatus::UpdateAvailable?RuntimeLogLevel::Info:RuntimeLogLevel::Warning,
                     "update.check.complete","status="+std::to_string(static_cast<int>(completion->evaluation.status))+" retry_failures="+std::to_string(update_retry_.consecutive_failures),nullptr);
    if (completion->evaluation.status == UpdateCheckStatus::UpdateAvailable && completion->evaluation.manifest) {
        available_update_ = completion->evaluation.manifest;
        update_runtime_status_ = L"WebCom " + Wide(available_update_->version) + L" is available and its signed manifest passed the build trust policy.";
        Note(L"WebCom " + Wide(available_update_->version) + L" is available.");
    } else {
        available_update_.reset();
        if (completion->evaluation.status == UpdateCheckStatus::NoUpdate) Note(L"WebCom is up to date on this release channel.");
        else Notice(L"Update check did not produce an installable release. " + update_runtime_status_);
    }
    QueueUI();
}

void BrowserApp::Impl::StageAvailableUpdateNow() {
    if (update_checking_ || update_staging_) return;
    if (!available_update_) { Notice(L"Check for updates first."); return; }
    if (update_transaction_.phase != UpdateTransactionPhase::None) {
        Notice(L"A staged update transaction already exists. Clear or finish it before staging another release.");
        return;
    }
    const auto package = StagedUpdatePackagePath(root_, available_update_->version);
    if (package.empty()) { Notice(L"The update staging path could not be created safely."); return; }
    update_staging_ = true;
    AppendRuntimeLog(root_,RuntimeLogLevel::Info,"update.stage.start","version="+available_update_->version,nullptr);
    update_runtime_status_ = L"Downloading and verifying WebCom " + Wide(available_update_->version) + L"...";
    QueueUI();
    const HWND target = window_;
    const auto manifest = *available_update_;
    const std::string publisher = update_runtime_config_.publisher_sha256;
    std::thread([target, manifest, publisher, package] {
        auto* completion = new UpdateStageCompletion();
        completion->manifest = manifest;
        completion->package = package;
        std::string error;
        completion->success = DownloadAndVerifyUpdatePackage(manifest, publisher, package, &error);
        completion->message = completion->success ? "The installer passed size, SHA-256, Authenticode and publisher verification." : error;
        if (!PostMessageW(target, kUpdateStageComplete, 0, reinterpret_cast<LPARAM>(completion))) delete completion;
    }).detach();
}

void BrowserApp::Impl::HandleUpdateStageCompletion(UpdateStageCompletion* raw) {
    std::unique_ptr<UpdateStageCompletion> completion(raw);
    update_staging_ = false;
    if (!completion) return;
    if (!completion->success) {
        update_runtime_status_ = Wide(completion->message.empty() ? "Update staging failed." : completion->message);
        AppendRuntimeLog(root_,RuntimeLogLevel::Error,"update.stage.failed",completion->message,nullptr);
        Notice(L"The update was not staged. " + update_runtime_status_);
        QueueUI();
        return;
    }
    UpdateTransaction next = update_transaction_;
    std::string error;
    if (!StageVerifiedUpdate(next, completion->manifest, kWebComVersion, &error) ||
        !SaveUpdateTransaction(root_, next, &error)) {
        update_runtime_status_ = L"The verified installer was downloaded but its transaction state could not be committed.";
        Notice(update_runtime_status_ + L" " + Wide(error));
        QueueUI();
        return;
    }
    update_transaction_ = next;
    AppendRuntimeLog(root_,RuntimeLogLevel::Info,"update.stage.verified","version="+next.target_version,nullptr);
    update_runtime_status_ = L"WebCom " + Wide(next.target_version) + L" is staged and verified. Installation still requires your explicit action.";
    Note(L"Signed WebCom update staged and verified.");
    QueueUI();
}

void BrowserApp::Impl::InstallStagedUpdateNow() {
    if (update_transaction_.phase != UpdateTransactionPhase::VerifiedPackage) {
        Notice(L"There is no verified staged update ready to install."); return;
    }
    const auto package = StagedUpdatePackagePath(root_, update_transaction_.target_version);
    if (package.empty()) { Notice(L"The staged update path is invalid."); return; }
    UpdateManifest verification{};
    verification.version = update_transaction_.target_version;
    verification.size_bytes = update_transaction_.package_size;
    verification.sha256 = update_transaction_.package_sha256;
    verification.publisher_sha256 = update_runtime_config_.publisher_sha256;
    std::string error;
    if (!VerifyStagedUpdatePackage(package, verification, update_runtime_config_.publisher_sha256, &error)) {
        Notice(L"The staged installer failed re-verification and will not run. " + Wide(error)); return;
    }
    const std::wstring question = L"Install WebCom " + Wide(update_transaction_.target_version) +
        L" now?\n\nThe installer has just been re-verified. WebCom windows will close after Windows accepts the installer launch. This action may show a UAC prompt.";
    if (MessageBoxW(window_, question.c_str(), L"Install verified WebCom update", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
    UpdateTransaction scheduled = update_transaction_;
    if (!MarkUpdateInstallScheduled(scheduled, &error) || !SaveUpdateTransaction(root_, scheduled, &error)) {
        Notice(L"The update install state could not be saved. " + Wide(error)); return;
    }
    update_transaction_ = scheduled;
    const std::wstring parameters = L"/CLOSEAPPLICATIONS /NORESTART";
    SHELLEXECUTEINFOW launch{}; launch.cbSize = sizeof(launch); launch.fMask = SEE_MASK_NOCLOSEPROCESS;
    launch.hwnd = window_; launch.lpVerb = L"runas"; launch.lpFile = package.c_str(); launch.lpParameters = parameters.c_str();
    launch.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&launch)) {
        UpdateTransaction revert = update_transaction_;
        revert.phase = UpdateTransactionPhase::VerifiedPackage;
        SaveUpdateTransaction(root_, revert, nullptr);
        update_transaction_ = revert;
        Notice(L"Windows did not launch the verified installer. The package remains staged.");
        QueueUI();
        return;
    }
    if (launch.hProcess) CloseHandle(launch.hProcess);
    AppendRuntimeLog(root_,RuntimeLogLevel::Info,"update.install.launched","version="+update_transaction_.target_version,nullptr);
    if (auto session = session_.lock()) session->CloseOtherProfileWindows(this);
    PostMessageW(window_, WM_CLOSE, 0, 0);
}

void BrowserApp::Impl::RollbackPreviousVersionNow() {
    recovery_package_ = LoadRecoveryPackage(root_);
    const auto readiness = EvaluateRecoveryReadiness(update_transaction_, recovery_package_,
                                                     kWebComVersion, update_runtime_config_.publisher_sha256);
    if (!readiness.ready()) {
        AppendRuntimeLog(root_,RuntimeLogLevel::Warning,"update.rollback.not_ready",
                         RecoveryReadinessName(readiness.status),nullptr);
        Notice(L"Rollback is not ready. " + Wide(readiness.message));
        return;
    }
    const auto package = RecoveryPackagePath(root_, recovery_package_->version);
    UpdateManifest verification{};
    verification.version = recovery_package_->version;
    verification.size_bytes = recovery_package_->size_bytes;
    verification.sha256 = recovery_package_->sha256;
    verification.publisher_sha256 = recovery_package_->publisher_sha256;
    std::string error;
    if (!VerifyStagedUpdatePackage(package, verification, update_runtime_config_.publisher_sha256, &error)) {
        AppendRuntimeLog(root_,RuntimeLogLevel::Error,"update.rollback.rejected",error,nullptr);
        Notice(L"The preserved recovery installer failed re-verification and will not run. " + Wide(error));
        return;
    }
    const std::wstring question=L"Roll back to WebCom "+Wide(recovery_package_->version)+
        L"?\n\nThe preserved previous installer has just been re-verified. Windows may show a UAC prompt. WebCom will close only after Windows accepts the installer launch.";
    if(MessageBoxW(window_,question.c_str(),L"Rollback WebCom",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return;
    const std::wstring parameters=L"/CLOSEAPPLICATIONS /NORESTART";
    SHELLEXECUTEINFOW launch{};launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS;launch.hwnd=window_;launch.lpVerb=L"runas";launch.lpFile=package.c_str();launch.lpParameters=parameters.c_str();launch.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&launch)){
        AppendRuntimeLog(root_,RuntimeLogLevel::Error,"update.rollback.launch_failed","Windows did not accept the recovery installer launch",nullptr);
        Notice(L"Windows did not launch the verified recovery installer. No rollback was performed.");return;
    }
    if(launch.hProcess)CloseHandle(launch.hProcess);
    AppendRuntimeLog(root_,RuntimeLogLevel::Warning,"update.rollback.launched","version="+recovery_package_->version,nullptr);
    if(auto session=session_.lock())session->CloseOtherProfileWindows(this);
    PostMessageW(window_,WM_CLOSE,0,0);
}

void BrowserApp::Impl::PrepareUpdateHealthState() {
    auto session = session_.lock();
    if (!session || !session->ClaimUpdateHealthOwner()) return;
    update_health_owner_ = true;
    std::string error;
    if (update_transaction_.phase == UpdateTransactionPhase::InstallScheduled &&
        update_transaction_.target_version == kWebComVersion) {
        if (MarkUpdateAwaitingHealthCheck(update_transaction_, &error) &&
            SaveUpdateTransaction(root_, update_transaction_, &error)) {
            update_runtime_status_ = L"Updated WebCom is running. A short local health window is in progress.";
            AppendRuntimeLog(root_,RuntimeLogLevel::Info,"update.health.start","version="+update_transaction_.target_version,nullptr);
            SetTimer(window_, kUpdateHealth, 30000, nullptr);
        }
    } else if (update_transaction_.phase == UpdateTransactionPhase::AwaitingHealthCheck &&
               update_transaction_.target_version == kWebComVersion) {
        if (RecordUpdateHealthFailure(update_transaction_, &error) &&
            SaveUpdateTransaction(root_, update_transaction_, &error)) {
            if (UpdateRollbackRecommended(update_transaction_)) {
                update_runtime_status_ = L"This updated version has failed its startup health window more than once. Rollback is recommended.";
                AppendRuntimeLog(root_,RuntimeLogLevel::Error,"update.health.rollback_recommended","failed_starts="+std::to_string(update_transaction_.failed_health_starts),nullptr);
            } else {
                update_runtime_status_ = L"The previous health window did not finish. WebCom is retrying the local health check.";
                AppendRuntimeLog(root_,RuntimeLogLevel::Warning,"update.health.retry","failed_starts="+std::to_string(update_transaction_.failed_health_starts),nullptr);
            }
            SetTimer(window_, kUpdateHealth, 30000, nullptr);
        }
    } else if (update_transaction_.phase == UpdateTransactionPhase::AwaitingHealthCheck &&
               update_transaction_.previous_version == kWebComVersion) {
        UpdateTransaction cleared;
        if (SaveUpdateTransaction(root_, cleared, &error)) {
            const auto staged_dir=StagedUpdateDirectory(root_,update_transaction_.target_version);
            update_transaction_ = cleared;
            if(!staged_dir.empty()){std::error_code remove_error;std::filesystem::remove_all(staged_dir,remove_error);}
            update_runtime_status_ = L"WebCom detected that the previous version is running again; the pending update transaction was cleared.";
            AppendRuntimeLog(root_,RuntimeLogLevel::Warning,"update.rollback.completed","version="+std::string(kWebComVersion),nullptr);
        }
    }
}

void BrowserApp::Impl::ConfirmUpdateHealth() {
    if (!update_health_owner_) return;
    KillTimer(window_, kUpdateHealth);
    if (update_transaction_.phase != UpdateTransactionPhase::AwaitingHealthCheck ||
        update_transaction_.target_version != kWebComVersion) return;
    const UpdateTransaction healthy = update_transaction_;
    bool recovery_preserved = false;
    std::string recovery_error;
    const auto staged = StagedUpdatePackagePath(root_, healthy.target_version);
    if (!staged.empty() && IsTrustedPublisherFingerprint(update_runtime_config_.publisher_sha256)) {
        UpdateManifest verification{};
        verification.version = healthy.target_version;
        verification.size_bytes = healthy.package_size;
        verification.sha256 = healthy.package_sha256;
        verification.publisher_sha256 = update_runtime_config_.publisher_sha256;
        if (VerifyStagedUpdatePackage(staged, verification, update_runtime_config_.publisher_sha256, &recovery_error)) {
            RecoveryPackage metadata{healthy.target_version, healthy.package_sha256, healthy.package_size, update_runtime_config_.publisher_sha256};
            recovery_preserved = PreserveRecoveryPackage(root_, metadata, staged, &recovery_error);
            if (recovery_preserved) recovery_package_ = metadata;
        }
    }
    if (recovery_preserved)
        AppendRuntimeLog(root_,RuntimeLogLevel::Info,"update.recovery.preserved","version="+healthy.target_version,nullptr);
    else if (!recovery_error.empty())
        AppendRuntimeLog(root_,RuntimeLogLevel::Warning,"update.recovery.unavailable",recovery_error,nullptr);
    std::string error;
    if (MarkUpdateHealthy(update_transaction_, kWebComVersion, &error) &&
        SaveUpdateTransaction(root_, update_transaction_, &error)) {
        update_runtime_status_ = L"WebCom " + Wide(kWebComVersion) + L" passed its local startup health window.";
        if (recovery_preserved) update_runtime_status_ += L" Its verified installer was preserved as the single rollback package for the next update.";
        AppendRuntimeLog(root_,RuntimeLogLevel::Info,"update.health.passed","version="+std::string(kWebComVersion),nullptr);
        Note(L"Updated WebCom passed its startup health check.");
        QueueUI();
    }
}

bool BrowserApp::Impl::IsDefaultBrowserNow() const {
    wchar_t module[MAX_PATH]{};
    const DWORD module_size = GetModuleFileNameW(nullptr, module, static_cast<DWORD>(std::size(module)));
    if (!module_size || module_size >= std::size(module)) return false;
    const auto matches = [&](const wchar_t* protocol) {
        DWORD chars = 0;
        AssocQueryStringW(ASSOCF_NOTRUNCATE, ASSOCSTR_EXECUTABLE, protocol, nullptr, nullptr, &chars);
        if (!chars || chars > 32768) return false;
        std::vector<wchar_t> executable(chars, L'\0');
        if (FAILED(AssocQueryStringW(ASSOCF_NOTRUNCATE, ASSOCSTR_EXECUTABLE, protocol, nullptr,
                                     executable.data(), &chars))) return false;
        return _wcsicmp(module, executable.data()) == 0;
    };
    return matches(L"http") && matches(L"https");
}

void BrowserApp::Impl::OpenDefaultAppsSettings() {
    std::wstring uri = L"ms-settings:defaultapps";
    wchar_t registration[512]{};
    DWORD bytes = sizeof(registration);
    DWORD type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"WebCom",
                     RRF_RT_REG_SZ, &type, registration, &bytes) == ERROR_SUCCESS) {
        uri += L"?registeredAppUser=WebCom";
    } else {
        bytes = sizeof(registration);
        type = 0;
        if (RegGetValueW(HKEY_LOCAL_MACHINE, L"Software\\RegisteredApplications", L"WebCom",
                         RRF_RT_REG_SZ, &type, registration, &bytes) == ERROR_SUCCESS)
            uri += L"?registeredAppMachine=WebCom";
    }
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        const auto fallback = reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL));
        if (fallback <= 32) Notice(L"Windows Default Apps settings could not be opened.");
    }
}

void BrowserApp::Impl::UpdateChannelMenuPopup() {
    const std::vector<MenuItem> items{
        {UpdateChannelStable,L"Stable",L"Recommended public releases",L"\uE73E",true,update_settings_.channel==ReleaseChannel::Stable},
        {UpdateChannelBeta,L"Beta",L"Preview releases for testing",L"\uE73E",true,update_settings_.channel==ReleaseChannel::Beta},
        {0,L"",L"",L"",false,false,true},
        {0,L"Automatic downloads are not enabled yet",L"Use Updates to opt in to release checks; install remains explicit",L"\uE946",false}
    };
    RECT client{};GetClientRect(window_,&client);POINT anchor{client.right-D(24),fullscreen_?D(20):D(100)};ClientToScreen(window_,&anchor);
    menu_open_=true;struct Reset{bool& value;~Reset(){value=false;}}reset{menu_open_};
    const UINT selected=ShowDarkMenu(window_,anchor,items,dpi_,390,true,&palette_);
    if(selected==UpdateChannelStable)SetUpdateChannel(ReleaseChannel::Stable);
    else if(selected==UpdateChannelBeta)SetUpdateChannel(ReleaseChannel::Beta);
}

void BrowserApp::Impl::CaptureExtensionProfile(Tab& tab) {
    if (tab.private_mode || !tab.web) return;
    ComPtr<ICoreWebView2_13> access;
    ComPtr<ICoreWebView2Profile> profile;
    if (FAILED(tab.web.As(&access)) || !access || FAILED(access->get_Profile(&profile)) || !profile) return;
    extension_profile_ = profile;
    extensions_supported_ = true;
    if (extension_bootstrap_tab_id_ == tab.id) {
        extension_bootstrap_tab_id_ = 0;
        PostMessageW(window_, kCloseTab, 0, static_cast<LPARAM>(tab.id));
    }
    if (auto* active = Active(); active && InternalPageFor(active->url) == InternalPage::Extensions)
        RefreshExtensions();
}

void BrowserApp::Impl::EnsureExtensionProfile() {
    if (extension_profile_) { if (!extensions_loading_) RefreshExtensions(); return; }
    for (auto& candidate : tabs_) {
        if (!candidate->private_mode && candidate->web) {
            CaptureExtensionProfile(*candidate);
            if (extension_profile_) return;
        }
    }
    if (extension_bootstrap_tab_id_) return;
    if (!environment_) {
        extensions_status_ = L"Starting the browser engine for extension management...";
        BeginEnvironment(); QueueUI(); return;
    }
    const auto id = Add(L"about:blank", false, false);
    if (!id) { extensions_status_ = L"Could not initialize the extension profile."; QueueUI(); return; }
    auto* bootstrap = Find(id);
    if (!bootstrap) return;
    bootstrap->ephemeral = true;
    bootstrap->title = L"Initializing extensions...";
    bootstrap->activity.protected_from_sleep = true;
    extension_bootstrap_tab_id_ = id;
    extensions_status_ = L"Initializing the current profile's extension engine...";
    Ensure(*bootstrap);
    QueueUI();
}

void BrowserApp::Impl::RefreshExtensionSourceHealth() {
    extension_source_health_.clear();
    extension_source_health_.reserve(extension_sources_.size());
    for (const auto& source : extension_sources_) {
        ExtensionSourceHealth health;
        health.manifest = InspectExtensionFolder(source.folder);
        health.snapshot = SnapshotExtensionSource(source.folder);
        health.baseline_known = !source.source_stamp.empty();
        health.changed = ExtensionSourceChanged(source, health.snapshot);
        extension_source_health_.push_back(std::move(health));
    }
}

void BrowserApp::Impl::RefreshExtensions() {
    if (closing_) return;
    RefreshExtensionSourceHealth();
    if (!extension_profile_) { EnsureExtensionProfile(); return; }
    ComPtr<ICoreWebView2Profile7> profile7;
    if (FAILED(extension_profile_.As(&profile7)) || !profile7) {
        extensions_supported_ = false;
        extensions_loading_ = false;
        extensions_status_ = L"This WebView2 Runtime is too old for browser-extension management. Update the runtime.";
        extensions_.clear(); QueueUI(); return;
    }
    extensions_supported_ = true;
    extensions_loading_ = true;
    extensions_status_ = L"Loading installed extensions...";
    const auto weak = weak_from_this();
    const HRESULT hr = profile7->GetBrowserExtensions(
        MakeCallback<ICoreWebView2ProfileGetBrowserExtensionsCompletedHandler>(
            [weak](HRESULT error, ICoreWebView2BrowserExtensionList* list) -> HRESULT {
                auto app = weak.lock();
                if (!app || app->closing_) return S_OK;
                app->extensions_loading_ = false;
                app->extensions_.clear();
                if (FAILED(error) || !list) {
                    app->extensions_status_ = L"Installed extensions could not be read. " + HResultText(error);
                    app->QueueUI(); return S_OK;
                }
                UINT count = 0;
                if (FAILED(list->get_Count(&count))) {
                    app->extensions_status_ = L"The extension list could not be read.";
                    app->QueueUI(); return S_OK;
                }
                const UINT shown = std::min<UINT>(count, 100u);
                app->extensions_.reserve(shown);
                for (UINT i = 0; i < shown; ++i) {
                    ComPtr<ICoreWebView2BrowserExtension> extension;
                    if (FAILED(list->GetValueAtIndex(i, extension.GetAddressOf())) || !extension) continue;
                    CoString id, name; BOOL enabled = FALSE;
                    extension->get_Id(&id.value);
                    extension->get_Name(&name.value);
                    extension->get_IsEnabled(&enabled);
                    app->extensions_.push_back({id.str(), name.str().empty()?L"Unnamed extension":name.str(), enabled != FALSE, extension});
                }
                app->extensions_status_ = count > shown ?
                    L"Showing the first 100 installed extensions." :
                    (count ? std::to_wstring(count)+L" installed extension"+(count==1?L".":L"s.") : L"No extensions are installed for this profile.");
                app->native_scroll_ = 0;
                app->QueueUI();
                return S_OK;
            }).Get());
    if (FAILED(hr)) {
        extensions_loading_ = false;
        extensions_status_ = L"Extension enumeration could not start. " + HResultText(hr);
        QueueUI();
    }
}

bool BrowserApp::Impl::SaveExtensionSourcesNow() {
    RefreshExtensionSourceHealth();
    std::string error;
    if (SaveExtensionSources(root_, profile_name_, extension_sources_, &error)) return true;
    extensions_status_ = L"Extension source metadata could not be saved. " + Wide(error);
    QueueUI();
    return false;
}

void BrowserApp::Impl::InstallExtensionFromSource(const std::filesystem::path& folder, bool reloading) {
    if (closing_ || !extension_profile_) return;
    const auto info = InspectExtensionFolder(folder);
    if (!info.valid) {
        extensions_status_ = L"The unpacked source cannot be loaded. " + Wide(info.error);
        QueueUI(); return;
    }
    ComPtr<ICoreWebView2Profile7> profile7;
    if (FAILED(extension_profile_.As(&profile7)) || !profile7) {
        extensions_status_ = L"This runtime does not support browser extensions."; QueueUI(); return;
    }
    extensions_status_ = reloading ? L"Reloading unpacked extension..." : L"Installing unpacked extension...";
    QueueUI();
    const auto weak = weak_from_this();
    const HRESULT hr = profile7->AddBrowserExtension(folder.c_str(),
        MakeCallback<ICoreWebView2ProfileAddBrowserExtensionCompletedHandler>(
            [weak, folder, reloading](HRESULT error, ICoreWebView2BrowserExtension* extension) -> HRESULT {
                auto app = weak.lock(); if (!app || app->closing_) return S_OK;
                if (FAILED(error) || !extension) {
                    app->extensions_status_ = (reloading ? L"The extension source could not be reloaded. " : L"The extension could not be installed. ") + HResultText(error);
                    app->QueueUI(); return S_OK;
                }
                CoString id, name; extension->get_Id(&id.value); extension->get_Name(&name.value);
                const auto id_utf8 = Utf8(id.str());
                const auto snapshot = SnapshotExtensionSource(folder);
                const auto stamp = snapshot.available ? snapshot.stamp : std::string{};
                if (!id_utf8.empty() && RememberExtensionSource(app->extension_sources_, id_utf8, folder, stamp))
                    app->SaveExtensionSourcesNow();
                app->Note((reloading ? L"Extension reloaded: " : L"Extension loaded: ") +
                    (name.str().empty()?L"Unnamed extension":name.str()));
                app->RefreshExtensions();
                return S_OK;
            }).Get());
    if (FAILED(hr)) {
        extensions_status_ = (reloading ? L"Extension reload could not start. " : L"Extension installation could not start. ") + HResultText(hr);
        QueueUI();
    }
}

void BrowserApp::Impl::LoadUnpackedExtension() {
    if (closing_) return;
    if (!extension_profile_) {
        EnsureExtensionProfile();
        Note(L"Extension support is initializing. Open Extensions again in a moment if the picker does not appear.");
        return;
    }
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))) || !dialog) {
        Notice(L"The extension folder picker could not be opened."); return;
    }
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options)))
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Load unpacked Chromium extension");
    if (dialog->Show(window_) != S_OK) return;
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item)) || !item) return;
    PWSTR raw = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) || !raw) return;
    const std::filesystem::path folder(raw); CoTaskMemFree(raw);
    const auto info = InspectExtensionFolder(folder);
    if (!info.valid) { Notice(L"This folder cannot be loaded as an unpacked extension.\n\n" + Wide(info.error)); return; }
    std::wstring prompt = L"Load this unpacked extension into the " + ProfileDisplayName(profile_name_, &appearance_) + L" profile?\n\n" +
        Wide(ExtensionCompatibilitySummary(info)) + L"\n\nSource: " + folder.wstring() +
        L"\n\nWebView2 references this folder directly; keep it in place after loading.";
    if (MessageBoxW(window_, prompt.c_str(), L"Load unpacked extension", MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING) != IDYES) return;
    InstallExtensionFromSource(folder, false);
}


void BrowserApp::Impl::ToggleExtension(std::size_t index) {
    if (index >= extensions_.size() || !extensions_[index].extension) return;
    auto extension = extensions_[index].extension;
    const bool next = !extensions_[index].enabled;
    extensions_status_ = next ? L"Enabling extension..." : L"Disabling extension..."; QueueUI();
    const auto weak = weak_from_this();
    const HRESULT hr = extension->Enable(next ? TRUE : FALSE,
        MakeCallback<ICoreWebView2BrowserExtensionEnableCompletedHandler>([weak](HRESULT error)->HRESULT{
            if (auto app=weak.lock(); app && !app->closing_) {
                if (FAILED(error)) { app->extensions_status_=L"The extension state could not be changed. "+HResultText(error); app->QueueUI(); }
                else app->RefreshExtensions();
            }
            return S_OK;
        }).Get());
    if (FAILED(hr)) { extensions_status_=L"The extension state change could not start. "+HResultText(hr); QueueUI(); }
}

void BrowserApp::Impl::RemoveExtension(std::size_t index) {
    if (index >= extensions_.size() || !extensions_[index].extension) return;
    const auto name = extensions_[index].name;
    const auto id = Utf8(extensions_[index].id);
    if (MessageBoxW(window_, (L"Remove \""+name+L"\" from this WebCom profile?\n\nIts saved unpacked-source mapping will also be removed.").c_str(), L"Remove extension", MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING) != IDYES) return;
    auto extension = extensions_[index].extension;
    extensions_status_ = L"Removing extension..."; QueueUI();
    const auto weak = weak_from_this();
    const HRESULT hr = extension->Remove(
        MakeCallback<ICoreWebView2BrowserExtensionRemoveCompletedHandler>([weak,id](HRESULT error)->HRESULT{
            if (auto app=weak.lock(); app && !app->closing_) {
                if (FAILED(error)) { app->extensions_status_=L"The extension could not be removed. "+HResultText(error); app->QueueUI(); }
                else {
                    if (ForgetExtensionSource(app->extension_sources_, id)) app->SaveExtensionSourcesNow();
                    app->RefreshExtensions();
                }
            }
            return S_OK;
        }).Get());
    if (FAILED(hr)) { extensions_status_=L"Extension removal could not start. "+HResultText(hr); QueueUI(); }
}

void BrowserApp::Impl::ShowExtensionDetails(std::size_t index) {
    if (index >= extensions_.size()) return;
    RefreshExtensionSourceHealth();
    const auto& extension = extensions_[index];
    std::wstring body = extension.name + L"\n\nID: " + extension.id +
        L"\nState: " + (extension.enabled?L"Enabled":L"Disabled");
    const auto id = Utf8(extension.id);
    std::size_t source_index = extension_sources_.size();
    for (std::size_t i = 0; i < extension_sources_.size(); ++i)
        if (extension_sources_[i].extension_id == id) { source_index = i; break; }
    if (source_index >= extension_sources_.size()) {
        body += L"\n\nUnpacked source: not recorded by WebCom. This can happen for extensions installed outside WebCom source tracking or by the underlying profile directly.";
    } else {
        const auto& source = extension_sources_[source_index];
        const auto& health = extension_source_health_[source_index];
        body += L"\n\nSource: " + source.folder.wstring() + L"\n\n" + Wide(ExtensionCompatibilitySummary(health.manifest));
        if (health.snapshot.available) {
            body += L"\n\nSource scan: " + std::to_wstring(health.snapshot.file_count) + L" files • " +
                FormatBytes(static_cast<std::int64_t>(health.snapshot.total_bytes));
            if (!health.baseline_known)
                body += L"\nChange baseline: not recorded yet. Reload once to establish it.";
            else if (health.changed)
                body += L"\nChange status: source files changed since the last successful load. Reload is recommended.";
            else
                body += L"\nChange status: source matches the last successful load stamp.";
        } else {
            body += L"\n\nSource change scan unavailable: " + Wide(health.snapshot.error);
        }
        body += L"\n\nThe change stamp is a developer convenience based on source file metadata; it is not a security or trust check.";
    }
    MessageBoxW(window_, body.c_str(), L"Extension details", MB_OK|MB_ICONINFORMATION);
}

void BrowserApp::Impl::ShowExtensionSourceDetails(std::size_t source_index) {
    if (source_index >= extension_sources_.size()) return;
    RefreshExtensionSourceHealth();
    if (source_index >= extension_source_health_.size()) return;
    const auto& source = extension_sources_[source_index];
    const auto& health = extension_source_health_[source_index];
    std::wstring body = L"Saved unpacked source\n\nID: " + Wide(source.extension_id) + L"\nSource: " + source.folder.wstring();
    body += L"\n\n" + Wide(ExtensionCompatibilitySummary(health.manifest));
    if (health.snapshot.available) {
        body += L"\n\nSource scan: " + std::to_wstring(health.snapshot.file_count) + L" files • " +
            FormatBytes(static_cast<std::int64_t>(health.snapshot.total_bytes));
        if (!health.baseline_known)
            body += L"\nChange baseline: not recorded yet. Reload once to establish it.";
        else if (health.changed)
            body += L"\nChange status: changed since the last successful load.";
        else
            body += L"\nChange status: unchanged since the last successful load.";
    } else {
        body += L"\n\nSource change scan unavailable: " + Wide(health.snapshot.error);
    }
    body += L"\n\nThe change stamp is not a security hash. WebView2 remains the authority that validates and loads the extension.";
    MessageBoxW(window_, body.c_str(), L"Unpacked source diagnostics", MB_OK|MB_ICONINFORMATION);
}

void BrowserApp::Impl::OpenExtensionSourceFolder(std::size_t source_index) {
    if (source_index >= extension_sources_.size()) return;
    const auto folder = extension_sources_[source_index].folder.wstring();
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(window_,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL)) <= 32)
        Notice(L"The unpacked extension folder could not be opened.");
}

void BrowserApp::Impl::ReloadExtensionSource(std::size_t source_index, std::optional<std::size_t> installed_index) {
    if (source_index >= extension_sources_.size()) return;
    const auto folder = extension_sources_[source_index].folder;
    const auto info = InspectExtensionFolder(folder);
    if (!info.valid) {
        extensions_status_ = L"The saved unpacked source is no longer loadable. " + Wide(info.error);
        QueueUI(); return;
    }
    const std::wstring prompt = L"Reload this unpacked extension source?\n\n" + Wide(ExtensionCompatibilitySummary(info)) +
        L"\n\nSource: " + folder.wstring();
    if (MessageBoxW(window_,prompt.c_str(),L"Reload unpacked extension",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES) return;
    if (!installed_index || *installed_index >= extensions_.size() || !extensions_[*installed_index].extension) {
        InstallExtensionFromSource(folder,true); return;
    }
    auto extension=extensions_[*installed_index].extension;
    extensions_status_=L"Removing the installed copy before reloading its source...";QueueUI();
    const auto weak=weak_from_this();
    const HRESULT hr=extension->Remove(MakeCallback<ICoreWebView2BrowserExtensionRemoveCompletedHandler>(
        [weak,folder](HRESULT error)->HRESULT{
            if(auto app=weak.lock();app&&!app->closing_){
                if(FAILED(error)){app->extensions_status_=L"The installed copy could not be removed before reload. "+HResultText(error);app->QueueUI();}
                else app->InstallExtensionFromSource(folder,true);
            }
            return S_OK;
        }).Get());
    if(FAILED(hr)){extensions_status_=L"Extension reload could not start. "+HResultText(hr);QueueUI();}
}


void BrowserApp::Impl::ForgetExtensionSourceMapping(std::size_t source_index) {
    if (source_index >= extension_sources_.size()) return;
    const auto id = extension_sources_[source_index].extension_id;
    const auto folder = extension_sources_[source_index].folder.wstring();
    const std::wstring prompt = L"Forget this saved unpacked source?\n\n" + folder +
        L"\n\nThis only removes WebCom's source mapping. It does not delete files from disk and does not remove an installed extension.";
    if (MessageBoxW(window_, prompt.c_str(), L"Forget unpacked source", MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION) != IDYES) return;
    if (ForgetExtensionSource(extension_sources_, id)) {
        SaveExtensionSourcesNow();
        extensions_status_ = L"Saved unpacked source mapping removed.";
        QueueUI();
    }
}


void BrowserApp::Impl::ClearAllSiteDataFromPrivacy() {
    auto* tab = Active();
    if (!tab || !tab->web || tab->private_mode) {
        tab = nullptr; for (auto& candidate : tabs_) if (!candidate->private_mode && candidate->web) { tab = candidate.get(); break; }
    }
    if (!tab) { Notice(L"Open a normal website tab once, then try again."); return; }
    ComPtr<ICoreWebView2_13> access; ComPtr<ICoreWebView2Profile> profile; ComPtr<ICoreWebView2Profile2> clear;
    if (FAILED(tab->web.As(&access)) || FAILED(access->get_Profile(&profile)) || FAILED(profile.As(&clear))) {
        Notice(L"Update the WebView2 Runtime to clear site data."); return;
    }
    if (MessageBoxW(window_, L"Clear all cookies and site storage for this WebCom profile?\n\nThis signs you out of websites but does not delete saved passwords, browsing history or downloaded files.",
        L"Clear all site data", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return;
    const auto weak = weak_from_this();
    const HRESULT result = clear->ClearBrowsingData(COREWEBVIEW2_BROWSING_DATA_KINDS_ALL_SITE,
        MakeCallback<ICoreWebView2ClearBrowsingDataCompletedHandler>([weak](HRESULT error) -> HRESULT {
            if (auto app = weak.lock(); app && !app->closing_) {
                if (SUCCEEDED(error)) { app->privacy_cookie_domains_.clear(); app->privacy_loaded_ = true; app->QueueUI(); app->Notice(L"All cookies and site storage for this profile were cleared."); }
                else app->Notice(L"Some site data could not be cleared. Close busy pages and try again.");
            }
            return S_OK;
        }).Get());
    if (FAILED(result)) Notice(L"Could not start the site-data clear operation. " + HResultText(result));
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
            {OpenFile, L"Open file...", L"Ctrl+O", L"\uE8E5"},
            {NewProfileWindow, L"Open profile window...", L"Ctrl+N", L"\uE77B"},
            {PrivateTab, L"New Incognito tab", L"Ctrl+Shift+N", L"incognito"},
            {ReopenTab, L"Reopen closed tab", L"Ctrl+Shift+T", L"\uE777", !closed_tabs_.empty()},
            divider,
            {ProfilesMenu, private_mode ? L"Incognito" : ProfileDisplayName(profile_name_,&appearance_), L"\u203A",
                private_mode ? L"incognito" : L"\uE77B", true, false, false, true},
            {PasswordMenu, L"Passwords", L"\u203A", L"\uE72E"},
            {Downloads, L"Downloads", L"Ctrl+J", L"\uE896"},
            {History, L"History", L"Ctrl+H", L"\uE81C"},
            {BookmarkMenu, L"Bookmarks", L"›", L""},
            {ExtensionsPage, L"Extensions", L"", L""},
            {TabList, L"Search tabs...", L"Ctrl+Shift+A", L""},
            {QuickCommands, L"Quick commands...", L"Ctrl+Shift+P", L"\uE945"},
            {WorkspaceMenu, L"Workspaces", L"\u203A", L"\uE8B7"},
            {SessionsManager, L"Saved sessions", L"", L"\uE8B7"},
            {SaveWorkspaceSession, L"Save current workspace as session...", L"", L"\uE74E", page},
            {split_tab_id_?CloseSplitView:SplitView, split_tab_id_?L"Close split view":L"Split view", L"", L"\uE8A9", tab && IsWebAddress(tab->url)},
            {PrivacyManager, L"Privacy manager", L"", L"\uE72E"},
            {ClearData, L"Delete browsing data...", L"Ctrl+Shift+Del", L"\uE74D"},
            divider,
            {ZoomReset, L"Zoom", percentage, L"\uE71E", true, false, false, false,
                ZoomOut, ZoomIn, FullScreen, page},
            divider,
            {ReaderMode, L"Reader mode", L"F9", L"\uE8A7", tab && IsWebAddress(tab->url)},
            {PrintPage, L"Print...", L"Ctrl+P", L"\uE749", page},
            {CapturePage, L"Capture screenshot...", L"Ctrl+Shift+S", L"\uE91B", page},
            {SearchInPage, L"Find in page...", L"Ctrl+F", L"\uE721", findable},
            {OpenInDefault, L"Open in default browser", L"", L"\uE8A7", tab && IsWebAddress(tab->url)},
            {DeveloperTools, L"Developer tools", L"F12", L"\uEC7A", page},
            {MemoryReport, L"Performance", L"Shift+Esc", L"\uE9D9"},
            divider,
            {HelpMenu, L"Help", L"›", L""},
            {WelcomePage, L"Getting started", L"", L""},
            {SettingsMenu, L"Settings", L"", L""},
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
        const auto appearance=current?appearance_:LoadAppearance(root_,state_.profiles[i].name);
        const auto display=ProfileDisplayName(state_.profiles[i].name,&appearance);
        std::wstring detail=current?L"This window":running?L"Open":L"";
        if(display!=Wide(state_.profiles[i].name)){
            if(!detail.empty())detail+=L" • ";detail+=L"key "+Wide(state_.profiles[i].name);
        }
        items.push_back({ProfileBase + static_cast<UINT>(i), display,
                         detail, L"\uE77B", true, current, false, current});
        items.back().swatch_rgb=ProfileColor(appearance,state_.profiles[i].name);
    }
    items.push_back({0, L"", L"", L"", false, false, true});
    items.push_back({ProfileManager, L"Manage profiles", L"", L"\uE713"});
    items.push_back({AddProfile, L"Add profile...", L"", L"", state_.profiles.size() < 32});
    items.push_back({RenameProfileDisplayNameCmd,L"Rename display name...",L"Keeps browser data key unchanged",L"\uE8AC"});
    items.push_back({ProfileColorMenu,L"Profile color",L"›",L""});
    items.push_back({ExportProfileSetupCmd,L"Export profile setup...",L"Preferences only • no browsing data",L"\uE8B5"});
    items.push_back({ImportProfileSetupCmd,L"Import profile setup...",L"Applies preferences to this profile",L"\uE8B5"});
    items.push_back({CreateProfileDesktopShortcutCmd,L"Create desktop shortcut",L"Opens this profile directly",L"\uE8CE"});
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
        if (visible >= tab_rects_.size() || IsRectEmpty(&tab_rects_[visible])) {
            const auto indices=VisibleTabIndices();
            if(!indices.empty()) visible=indices[std::min(first_visible_tab_,indices.size()-1)];
        }
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
    items.push_back(divider);
    const auto workspaces=WorkspaceNames();
    for(std::size_t i=0;i<workspaces.size();++i)
        items.push_back({MoveWorkspaceBase+static_cast<UINT>(i),L"Move to workspace: "+workspaces[i],L"",L"\uE8B7",true,_wcsicmp(tab.workspace.c_str(),workspaces[i].c_str())==0});
    const bool can_split_target=IsWebAddress(tab.url) && Active() && IsWebAddress(Active()->url) && tab.workspace==active_workspace_ && tab.id!=Active()->id;
    const bool can_split_active=tab.id==Active()->id && IsWebAddress(tab.url) && std::any_of(tabs_.begin(),tabs_.end(),[&](const auto& item){return item->id!=tab.id && item->workspace==active_workspace_ && IsWebAddress(item->url);});
    items.push_back({split_tab_id_?CloseSplitView:SplitView,split_tab_id_?L"Close split view":L"Open in split view",L"",L"\uE8A9",split_tab_id_ || can_split_target || can_split_active});
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
    else if(selected>=MoveWorkspaceBase && selected<MoveWorkspaceBase+workspaces.size())AssignTabWorkspace(target_id,workspaces[selected-MoveWorkspaceBase]);
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
    const auto* credential_preferences = CurrentSavedProfile();
    const bool save_passwords = credential_preferences ? credential_preferences->save_passwords : state_.save_passwords;
    const bool general_autofill = credential_preferences ? credential_preferences->general_autofill : true;
    std::vector<MenuItem> items;
    if (section == PasswordMenu) {
        items = {
            {TogglePasswords, L"Offer to save passwords", L"", L"\uE72E", true, save_passwords},
            {ToggleAutofill, L"Autofill addresses and forms", L"", L"\uE8D4", true, general_autofill},
            divider,
            {ClearSavedPasswords, L"Clear saved passwords...", L"", L"\uE74D"},
            {ClearAutofillData, L"Clear autofill form data...", L"", L"\uE74D"},
            divider,
            {ProfileManager, L"Manage profiles", L"\u203A", L"\uE77B"}
        };
    } else if (section == HelpMenu) {
        items = {
            {WelcomePage, L"Getting started", L"", L""},
            {ShortcutsPage, L"Keyboard shortcuts", L"", L""},
            {DiagnosticsPage, L"Support diagnostics", L"", L"\uE946"},
            {RecoveryPage, L"Recovery & reliability", L"", L"\uE72E"},
            {UpdateChannelMenu, L"Release channel", Wide(ReleaseChannelName(update_settings_.channel)), L"\uE81C"},
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
            {StartupMenu, L"On startup", L"\u203A", L"\uE81C"},
            {PasswordMenu, L"Passwords and autofill", L"\u203A", L"\uE72E"},
            {ClearData, L"Delete browsing data...", L"Ctrl+Shift+Del", L"\uE74D"},
            divider,
            {BookmarkMenu, L"Bookmarks", L"\u203A", L"\uE734"},
            {ProfileManager, L"Manage profiles", L"\u203A", L"\uE77B"},
            divider,
            {UpdateChannelMenu, L"Release channel", Wide(ReleaseChannelName(update_settings_.channel)), L"\uE81C"},
            {DiagnosticsPage, L"Support diagnostics", L"", L"\uE946"},
            {RecoveryPage, L"Recovery & reliability", L"", L"\uE72E"},
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

void BrowserApp::Impl::StartupMenuPopup() {
    if (menu_open_ || closing_ || !window_ || !IsWindow(window_)) return;
    auto* profile = CurrentSavedProfile();
    if (!profile) return;
    const auto* tab = Active();
    const bool current_page = tab && IsWebAddress(tab->url);
    const MenuItem divider{0,L"",L"",L"",false,false,true};
    std::vector<MenuItem> items{
        {StartupContinue,L"Continue where you left off",L"Restore saved tabs",L"\uE81C",true,profile->startup_mode==StartupMode::Continue},
        {StartupNewTab,L"Open the New Tab page",L"Start with one clean tab",L"\uE8A7",true,profile->startup_mode==StartupMode::NewTab},
        {StartupSpecificPages,L"Open specific pages",profile->startup_pages.empty()?L"No pages configured":std::to_wstring(profile->startup_pages.size())+L" configured",L"\uE774",true,profile->startup_mode==StartupMode::SpecificPages},
        divider,
        {StartupAddCurrentPage,L"Add current page",L"Use the active HTTP(S) page",L"\uE710",current_page && profile->startup_pages.size()<16},
        {StartupAddPage,L"Add page...",L"Enter a complete https:// or http:// address",L"\uE710",profile->startup_pages.size()<16},
        {StartupClearPages,L"Clear configured pages",L"Does not change browsing history or bookmarks",L"\uE74D",!profile->startup_pages.empty()}
    };
    if (!profile->startup_pages.empty()) {
        items.push_back(divider);
        for (std::size_t i=0;i<profile->startup_pages.size();++i)
            items.push_back({StartupPageBase+static_cast<UINT>(i),L"Remove startup page",Wide(profile->startup_pages[i]),L"\uE74D"});
    }
    RECT client{};GetClientRect(window_,&client);
    POINT anchor{std::max(D(8),static_cast<int>(client.right)/2-D(200)),fullscreen_?D(20):D(86)};
    ClientToScreen(window_,&anchor);
    UINT selected=0;
    {
        menu_open_=true;struct Reset{bool& value;~Reset(){value=false;}} reset{menu_open_};
        selected=ShowDarkMenu(window_,anchor,items,dpi_,420,false,&palette_);
    }
    if (!selected || closing_) return;
    profile=CurrentSavedProfile(); if(!profile)return;
    if(selected==StartupContinue) profile->startup_mode=StartupMode::Continue;
    else if(selected==StartupNewTab) profile->startup_mode=StartupMode::NewTab;
    else if(selected==StartupSpecificPages) profile->startup_mode=StartupMode::SpecificPages;
    else if(selected==StartupAddCurrentPage){
        const auto* live=Active(); if(!live)return;
        const auto page=CanonicalStartupPage(live->url); if(!page){Note(L"The current page cannot be used as a startup page.");return;}
        if(std::find_if(profile->startup_pages.begin(),profile->startup_pages.end(),[&](const std::string& item){return _stricmp(item.c_str(),page->c_str())==0;})==profile->startup_pages.end())
            profile->startup_pages.push_back(*page);
        profile->startup_mode=StartupMode::SpecificPages;
    } else if(selected==StartupAddPage){
        const auto value=AskName(window_,instance_,&palette_,L"Add startup page",L"&Page address",
            L"Enter a complete HTTP(S) address. Credentials in URLs are not accepted.",L"https://",L"Add");
        if(!value)return;
        const auto page=CanonicalStartupPage(*value);
        if(!page){Notice(L"Enter a complete http:// or https:// address without embedded credentials.");return;}
        if(std::find_if(profile->startup_pages.begin(),profile->startup_pages.end(),[&](const std::string& item){return _stricmp(item.c_str(),page->c_str())==0;})!=profile->startup_pages.end()){
            Note(L"That startup page is already configured.");return;
        }
        profile->startup_pages.push_back(*page);profile->startup_mode=StartupMode::SpecificPages;
    } else if(selected==StartupClearPages){
        profile->startup_pages.clear();
    } else if(selected>=StartupPageBase && selected<StartupPageBase+profile->startup_pages.size()){
        const auto index=static_cast<std::size_t>(selected-StartupPageBase);
        profile->startup_pages.erase(profile->startup_pages.begin()+static_cast<std::ptrdiff_t>(index));
    } else return;
    Dirty();UpdateUI();
    Note(profile->startup_mode==StartupMode::Continue?L"Startup: continue where you left off.":
         profile->startup_mode==StartupMode::NewTab?L"Startup: open a clean New Tab.":
         profile->startup_pages.empty()?L"Startup: specific pages selected; New Tab will be used until a page is added.":
         L"Startup: open "+std::to_wstring(profile->startup_pages.size())+L" configured page"+(profile->startup_pages.size()==1?L".":L"s."));
}

void BrowserApp::Impl::CycleChromeFocus(bool reverse) {
    if (closing_ || !window_) return;
    const HWND focused=GetFocus();
    auto focus_page=[&](){
        if(auto* tab=Active()){
            if(tab->web && tab->controller){tab->controller->MoveFocus(reverse?COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS:COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT);return;}
        }
        if(tab_strip_)SetFocus(tab_strip_);
    };
    if (focused==address_) {
        if(reverse){if(tab_strip_)SetFocus(tab_strip_);}
        else focus_page();
        return;
    }
    if (focused==tab_strip_) {
        if(reverse) focus_page(); else FocusOmnibox();
        return;
    }
    if(reverse) FocusOmnibox();
    else if(tab_strip_) SetFocus(tab_strip_);
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
    const auto* current_preferences = CurrentSavedProfile();
    const auto new_profile_name = Utf8(*name);
    state_.profiles.push_back({new_profile_name, {{"webcom:newtab", "New tab", false, false}}, 0, {},
        current_preferences ? current_preferences->save_passwords : state_.save_passwords,
        current_preferences ? current_preferences->general_autofill : true,
        {{"Main",0}}, "Main", false, false, StartupMode::Continue, {}});
    static constexpr std::array<const wchar_t*,8> color_names{L"Blue",L"Green",L"Yellow",L"Red",L"Purple",L"Cyan",L"Orange",L"Gray"};
    std::vector<MenuItem> color_items;
    color_items.push_back({1,L"Automatic color",L"Based on profile name",L"",true,true});
    for(std::size_t i=0;i<color_names.size();++i){
        MenuItem item{2+static_cast<UINT>(i),std::wstring(color_names[i]),L"Profile badge and accent",L""};
        item.swatch_rgb=kProfileColors[i];color_items.push_back(std::move(item));
    }
    RECT client{};GetClientRect(window_,&client);POINT anchor{std::max(D(8),static_cast<int>(client.right)/2-D(190)),fullscreen_?D(20):D(86)};ClientToScreen(window_,&anchor);
    UINT color_selected=0;
    {
        menu_open_=true;struct Reset{bool& value;~Reset(){value=false;}} reset{menu_open_};
        color_selected=ShowDarkMenu(window_,anchor,color_items,dpi_,380,false,&palette_);
    }
    if(color_selected>=2 && color_selected<2+kProfileColors.size()){
        Appearance created=LoadAppearance(root_,new_profile_name);
        created.accent_rgb=kProfileColors[color_selected-2];
        std::string appearance_error;
        if(!SaveAppearance(root_,new_profile_name,created,&appearance_error))
            Note(L"The profile was created, but its chosen color could not be saved. "+Wide(appearance_error));
    }
    Dirty();
    SwitchProfile(state_.profiles.size() - 1);
}

void BrowserApp::Impl::SwitchProfile(std::size_t index) {
    if(index>=state_.profiles.size())return;
    // Profiles are separate native windows. Their live tabs and downloads remain
    // running while another profile is opened or brought to the foreground.
    if(auto session=session_.lock())session->OpenProfile(state_.profiles[index].name,L"",false);
}

void BrowserApp::Impl::RenameProfileDisplayName() {
    const auto current=ProfileDisplayName(profile_name_,&appearance_);
    const auto renamed=AskName(window_,instance_,&palette_,L"Rename profile display name",L"&Display name",
        (L"Changes only the visible label. The browser-data key remains '"+Wide(profile_name_)+L"'.").c_str(),
        current.c_str(),L"Rename");
    if(!renamed || renamed->empty() || *renamed==current)return;
    auto next=appearance_;
    next.display_name=*renamed==Wide(profile_name_)?std::string{}:Utf8(*renamed);
    if(!CommitAppearance(next))return;
    UpdateUI();
    Note(L"Profile display name updated. Browser data stays under key '"+Wide(profile_name_)+L"'.");
}

void BrowserApp::Impl::ExportProfileSetup() {
    const auto* saved=CurrentSavedProfile(); if(!saved)return;
    wchar_t file[MAX_PATH]=L"WebCom-profile-setup.webcom-profile";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"WebCom profile setup (*.webcom-profile)\0*.webcom-profile\0All files\0*.*\0";
    dialog.lpstrDefExt=L"webcom-profile";dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(!GetSaveFileNameW(&dialog))return;
    ProfileTemplate value;value.appearance=appearance_;value.save_passwords=saved->save_passwords;
    value.general_autofill=saved->general_autofill;value.vertical_tabs=vertical_tabs_;
    value.vertical_tabs_collapsed=vertical_tabs_collapsed_;
    std::string error;if(!ExportProfileTemplate(file,value,&error)){Notice(L"Profile setup could not be exported. "+Wide(error));return;}
    Note(L"Profile setup exported. It contains preferences only, not passwords, cookies, history or tabs.");
}

void BrowserApp::Impl::ImportProfileSetup() {
    wchar_t file[MAX_PATH]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window_;
    dialog.lpstrFilter=L"WebCom profile setup (*.webcom-profile)\0*.webcom-profile\0All files\0*.*\0";
    dialog.lpstrFile=file;dialog.nMaxFile=MAX_PATH;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog))return;
    ProfileTemplate value;std::string error;if(!ImportProfileTemplate(file,value,&error)){Notice(L"Profile setup could not be imported. "+Wide(error));return;}
    if(MessageBoxW(window_,L"Apply these profile preferences?\n\nThis changes appearance, Reader defaults, password/autofill prompt preferences and tab layout. It does not import passwords, cookies, history, tabs or bookmarks.",L"Import WebCom profile setup",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)return;
    if(!SaveAppearance(root_,profile_name_,value.appearance,&error)){Notice(L"Imported appearance could not be saved. "+Wide(error));return;}
    appearance_=value.appearance;vertical_tabs_=value.vertical_tabs;vertical_tabs_collapsed_=value.vertical_tabs_collapsed;
    if(auto* saved=CurrentSavedProfile()){saved->save_passwords=value.save_passwords;saved->general_autofill=value.general_autofill;saved->vertical_tabs=vertical_tabs_;saved->vertical_tabs_collapsed=vertical_tabs_collapsed_;}
    ApplyAppearance();if(auto session=session_.lock())session->ApplyPreferences();Dirty();UpdateUI(true);
    Note(L"Profile setup imported. Browsing data was not changed.");
}

void BrowserApp::Impl::CreateProfileDesktopShortcut() {
    if (closing_ || profile_name_.empty() || !IsValidProfileName(profile_name_)) return;
    wchar_t executable[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(_countof(executable)));
    if (!length || length >= _countof(executable)) { Notice(L"WebCom could not locate its executable for the shortcut."); return; }
    PWSTR desktop = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_CREATE, nullptr, &desktop)) || !desktop) {
        CoTaskMemFree(desktop); Notice(L"Windows could not locate the Desktop folder."); return;
    }
    std::wstring label = ProfileDisplayName(profile_name_, &appearance_);
    if (label.empty()) label = Wide(profile_name_);
    for (auto& ch : label) {
        if (ch < 0x20 || wcschr(L"<>:\"/\\|?*", ch)) ch = L'_';
    }
    while (!label.empty() && (label.back() == L' ' || label.back() == L'.')) label.pop_back();
    if (label.empty()) label = L"Profile";
    if (label.size() > 80) label.resize(80);
    const std::filesystem::path shortcut = std::filesystem::path(desktop) / (L"WebCom - " + label + L".lnk");
    CoTaskMemFree(desktop);
    if (std::filesystem::exists(shortcut)) {
        if (MessageBoxW(window_, (L"Replace the existing desktop shortcut?\n\n" + shortcut.filename().wstring()).c_str(),
                        L"WebCom profile shortcut", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
    }
    ComPtr<IShellLinkW> link;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (FAILED(result) || !link) { Notice(L"Windows could not create the profile shortcut. " + HResultText(result)); return; }
    const std::filesystem::path executable_path(executable);
    const std::wstring arguments = L"--profile \"" + Wide(profile_name_) + L"\"";
    link->SetPath(executable);
    link->SetArguments(arguments.c_str());
    const std::wstring description = L"Open WebCom profile " + label;
    link->SetDescription(description.c_str());
    link->SetIconLocation(executable, 0);
    const auto parent = executable_path.parent_path().wstring();
    if (!parent.empty()) link->SetWorkingDirectory(parent.c_str());
    ComPtr<IPersistFile> persist;
    result = link.As(&persist);
    if (SUCCEEDED(result)) result = persist->Save(shortcut.c_str(), TRUE);
    if (FAILED(result)) { Notice(L"The desktop shortcut could not be saved. " + HResultText(result)); return; }
    Note(L"Desktop shortcut created: " + shortcut.filename().wstring());
}

void BrowserApp::Impl::DeleteCurrentProfile() {
    if(closing_ || delete_profile_pending_) return;
    if(state_.profiles.size()<=1){Notice(L"WebCom must keep at least one profile. Create another profile before deleting this one.");return;}
    for(auto& tab:tabs_)if(HasDownload(*tab)){Notice(L"Finish or cancel active downloads before deleting this profile.");return;}
    const auto profile_display=ProfileDisplayName(profile_name_,&appearance_);
    const std::wstring message=L"Delete the '"+profile_display+L"' profile?\n\nBrowser-data key: "+Wide(profile_name_)+L"\n\nThis permanently removes its cookies, saved passwords, site storage, history, bookmarks, permissions, appearance settings and saved tabs. Downloaded files on your PC are not deleted.\n\nThis cannot be undone.";
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
    // Do not keep extension/profile COM references alive while Profile8::Delete
    // marks the profile data for removal.
    extension_profile_.Reset();
    extensions_.clear();
    extension_bootstrap_tab_id_=0;
    extensions_loading_=false;
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

void BrowserApp::Impl::ClearProfileDataKind(COREWEBVIEW2_BROWSING_DATA_KINDS kind,
                                                const std::wstring& title,
                                                const std::wstring& question,
                                                const std::wstring& success) {
    auto* tab = Active();
    if (!tab || !tab->web || tab->private_mode) {
        tab = nullptr;
        for (auto& candidate : tabs_)
            if (!candidate->private_mode && candidate->web) { tab = candidate.get(); break; }
    }
    if (!tab) {
        Notice(L"Open a normal webpage in this profile once, then use this command again.");
        return;
    }
    ComPtr<ICoreWebView2_13> access;
    ComPtr<ICoreWebView2Profile> profile;
    ComPtr<ICoreWebView2Profile2> clear;
    if (FAILED(tab->web.As(&access)) || FAILED(access->get_Profile(&profile)) || FAILED(profile.As(&clear))) {
        Notice(L"Update the WebView2 Runtime to clear this profile data.");
        return;
    }
    if (MessageBoxW(window_, question.c_str(), title.c_str(),
        MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES || closing_) return;
    const auto weak = weak_from_this();
    const HRESULT hr = clear->ClearBrowsingData(kind, MakeCallback<ICoreWebView2ClearBrowsingDataCompletedHandler>(
        [weak, success](HRESULT error) -> HRESULT {
            if (auto app = weak.lock(); app && !app->closing_)
                app->Notice(SUCCEEDED(error) ? success : L"Some profile data could not be cleared. Close busy pages and try again.");
            return S_OK;
        }).Get());
    if (FAILED(hr)) Notice(L"Could not start the clear-data operation. " + HResultText(hr));
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
    const auto question = L"Clear all browsing data for profile \"" + ProfileDisplayName(profile_name_,&appearance_) +
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
    if(command>=WorkspaceColorBase && command<WorkspaceColorBase+kProfileColors.size()){
        if(auto* workspace=FindWorkspace(active_workspace_)){
            workspace->color=static_cast<std::uint8_t>(command-WorkspaceColorBase);
            Dirty();UpdateUI();
        }
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
    if (command >= PrivacyCookieBase && command < PrivacyCookieBase + privacy_cookie_domains_.size()) {
        ClearPrivacyCookieDomain(static_cast<std::size_t>(command - PrivacyCookieBase)); return;
    }
    const auto privacy_origins = PrivacyPermissionOrigins();
    if (command >= PrivacyPermissionBase && command < PrivacyPermissionBase + privacy_origins.size()) {
        ResetPrivacyPermissionOrigin(static_cast<std::size_t>(command - PrivacyPermissionBase)); return;
    }
    if (command >= SessionBase && command < SessionBase + named_sessions_.size()) {
        RestoreNamedSession(static_cast<std::size_t>(command - SessionBase)); return;
    }
    if (command >= DeleteSessionBase && command < DeleteSessionBase + named_sessions_.size()) {
        DeleteNamedSessionUi(static_cast<std::size_t>(command - DeleteSessionBase)); return;
    }
    if (command >= WorkspaceBase && command < WorkspaceBase + workspaces_.size()) {
        const auto index = static_cast<std::size_t>(command - WorkspaceBase);
        SwitchWorkspace(Wide(workspaces_[index].name)); return;
    }
    if (command >= ProfileBase && command < ProfileBase + state_.profiles.size()) {
        SwitchProfile(command - ProfileBase); return;
    }
    if (command >= SelectTabBase && command <= SelectTabBase + 8) {
        const auto visible=VisibleTabIndices();
        const auto index = static_cast<std::size_t>(command - SelectTabBase);
        if (index == 8 && !visible.empty()) Select(visible.back());
        else if (index < visible.size()) Select(visible[index]);
        return;
    }
    if (command >= DeleteBookmarkFolderBase && command < DeleteBookmarkFolderBase + 100) {
        const auto folders=webcom::BookmarkFolderPaths(bookmarks_,explicit_bookmark_folders_,true);
        const auto index=static_cast<std::size_t>(command-DeleteBookmarkFolderBase);
        if(index<folders.size()){DeleteBookmarkFolderTree(Wide(folders[index]));return;}
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
    if (command >= ExtensionToggleBase && command < ExtensionToggleBase + extensions_.size()) {
        ToggleExtension(static_cast<std::size_t>(command - ExtensionToggleBase)); return;
    }
    if (command >= ExtensionRemoveBase && command < ExtensionRemoveBase + extensions_.size()) {
        RemoveExtension(static_cast<std::size_t>(command - ExtensionRemoveBase)); return;
    }
    if (command >= ExtensionDetailsBase && command < ExtensionDetailsBase + extensions_.size()) {
        ShowExtensionDetails(static_cast<std::size_t>(command - ExtensionDetailsBase)); return;
    }
    if (command >= ExtensionOpenFolderBase && command < ExtensionOpenFolderBase + extension_sources_.size()) {
        OpenExtensionSourceFolder(static_cast<std::size_t>(command - ExtensionOpenFolderBase)); return;
    }
    if (command >= ExtensionReloadBase && command < ExtensionReloadBase + extension_sources_.size()) {
        const auto source_index=static_cast<std::size_t>(command-ExtensionReloadBase);
        std::optional<std::size_t> installed;
        for(std::size_t i=0;i<extensions_.size();++i)
            if(Utf8(extensions_[i].id)==extension_sources_[source_index].extension_id){installed=i;break;}
        ReloadExtensionSource(source_index,installed); return;
    }
    if (command >= ExtensionSourceDetailsBase && command < ExtensionSourceDetailsBase + extension_sources_.size()) {
        ShowExtensionSourceDetails(static_cast<std::size_t>(command - ExtensionSourceDetailsBase)); return;
    }
    if (command >= ExtensionForgetSourceBase && command < ExtensionForgetSourceBase + extension_sources_.size()) {
        ForgetExtensionSourceMapping(static_cast<std::size_t>(command - ExtensionForgetSourceBase)); return;
    }
    switch (command) {
    case AppearanceMenu: case ThemeMenu: case ProfileColorMenu: AppearanceOptions(command);break;
    case ToggleBookmarks:{auto next=appearance_;next.show_bookmarks=!show_bookmarks_;CommitAppearance(next);break;}
    case ToggleReducedMotion:{auto next=appearance_;next.reduce_motion=!appearance_.reduce_motion;CommitAppearance(next);break;}
    case ToggleHighContrast:{auto next=appearance_;next.high_contrast=!appearance_.high_contrast;CommitAppearance(next);break;}
    case ColorAutomatic:{auto next=appearance_;next.accent_rgb.reset();CommitAppearance(next);break;}
    case AppearanceReset:{auto reset=Appearance{};reset.display_name=appearance_.display_name;CommitAppearance(reset);break;}
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
    case CreateBookmarkFolderCmd: CreateBookmarkFolder(); break;
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
    case WorkspaceMenu: WorkspaceMenuPopup(); break;
    case StartupMenu: StartupMenuPopup(); break;
    case FocusCycle: CycleChromeFocus(false); break;
    case FocusCycleReverse: CycleChromeFocus(true); break;
    case SessionsManager: OpenInternalPage(L"webcom:sessions"); break;
    case SaveWorkspaceSession: SaveCurrentWorkspaceSession(); break;
    case PrivacyManager: OpenInternalPage(L"webcom:privacy"); RefreshPrivacyManager(); break;
    case PrivacyRefresh: RefreshPrivacyManager(); break;
    case PrivacyClearAllSiteData: ClearAllSiteDataFromPrivacy(); break;
    case ReaderMode: OpenReaderMode(); break;
    case ReaderBackToPage: ReturnFromReader(); break;
    case ReaderTextSmaller:
        if(tab && InternalPageFor(tab->url)==InternalPage::Reader){tab->reader_text_scale=std::max(-2,tab->reader_text_scale-1);PersistReaderDefaults(*tab);native_scroll_=0;QueueUI();} break;
    case ReaderTextLarger:
        if(tab && InternalPageFor(tab->url)==InternalPage::Reader){tab->reader_text_scale=std::min(4,tab->reader_text_scale+1);PersistReaderDefaults(*tab);native_scroll_=0;QueueUI();} break;
    case ReaderWidthCycle:
        if(tab && InternalPageFor(tab->url)==InternalPage::Reader){tab->reader_width_mode=(tab->reader_width_mode+1)%3;PersistReaderDefaults(*tab);native_scroll_=0;QueueUI();} break;
    case ReaderFontToggle:
        if(tab && InternalPageFor(tab->url)==InternalPage::Reader){tab->reader_serif=!tab->reader_serif;PersistReaderDefaults(*tab);QueueUI();} break;
    case ReaderCopyText: CopyReaderText(); break;
    case ReaderSaveText: SaveReaderText(); break;
    case ReaderExportMenu: ReaderExportMenuPopup(); break;
    case ReaderSaveHtml: SaveReaderHtml(); break;
    case ReaderSaveMarkdown: SaveReaderMarkdown(); break;
    case PrivacyResetAllPermissions:
        if (!site_permissions_.empty() && MessageBoxW(window_, L"Reset all saved site-permission exceptions for this profile?", L"Reset site permissions", MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES) {
            site_permissions_.clear(); SaveSitePermissionsNow(); native_scroll_=0; QueueUI(); Note(L"All saved site-permission exceptions were reset.");
        }
        break;
    case NextWorkspace: CycleWorkspace(1); break;
    case PreviousWorkspace: CycleWorkspace(-1); break;
    case AddWorkspace: CreateWorkspace(); break;
    case RenameWorkspaceCmd: RenameWorkspace(); break;
    case DeleteWorkspaceCmd: DeleteWorkspace(); break;
    case ToggleVerticalTabs: ToggleVerticalTabsMode(); break;
    case ToggleVerticalTabsCollapse: ToggleVerticalTabsCollapseMode(); break;
    case QuickCommands: ShowQuickCommands(); break;
    case CapturePage: CaptureMenuPopup(); break;
    case CaptureViewport: SavePageCapture(false); break;
    case CaptureSelection: SaveSelectionCapture(); break;
    case CaptureFullPage: SavePageCapture(true); break;
    case CaptureViewportClipboard: CopyVisibleCaptureToClipboard(); break;
    case CaptureSelectionClipboard: CopySelectionCaptureToClipboard(); break;
    case AnnotateViewport: AnnotateVisibleCapture(); break;
    case AnnotateSelection: AnnotateSelectionCapture(); break;
    case CloseSplitView: ClearSplitView(); break;
    case SplitView: {
        if(split_tab_id_) { ClearSplitView(); break; }
        if(tab && Active() && tab->id!=Active()->id && tab->workspace==active_workspace_ && IsWebAddress(tab->url)) {
            SetSplitTab(tab->id); break;
        }
        auto visible=VisibleTabIndices();
        if(visible.size()<2 || !Active() || !IsWebAddress(Active()->url)) { Note(L"Open at least two website tabs in this workspace to use split view."); break; }
        auto found=std::find(visible.begin(),visible.end(),active_);
        std::size_t start=found==visible.end()?0:static_cast<std::size_t>(found-visible.begin());
        Tab* secondary=nullptr;
        for(std::size_t offset=1;offset<visible.size();++offset){
            auto* candidate=tabs_[visible[(start+offset)%visible.size()]].get();
            if(candidate && IsWebAddress(candidate->url)){secondary=candidate;break;}
        }
        if(secondary)SetSplitTab(secondary->id);
        else Note(L"Open another website tab in this workspace to use split view.");
        break;
    }
    case SettingsMenu: OpenInternalPage(L"webcom:settings"); break;
    case ShortcutsPage: OpenInternalPage(L"webcom:shortcuts"); break;
    case DiagnosticsPage: OpenInternalPage(L"webcom:diagnostics"); break;
    case RecoveryPage: OpenInternalPage(L"webcom:recovery"); break;
    case UpdatesPage: OpenInternalPage(L"webcom:updates"); break;
    case ExtensionsPage: OpenInternalPage(L"webcom:extensions"); EnsureExtensionProfile(); break;
    case ExtensionsRefresh: RefreshExtensions(); break;
    case ExtensionsLoadUnpacked: LoadUnpackedExtension(); break;
    case WelcomePage: OpenInternalPage(L"webcom:welcome"); break;
    case OpenDefaultApps: OpenDefaultAppsSettings(); break;
    case WelcomeStartBrowsing:
        if (tab && InternalPageFor(tab->url)==InternalPage::Welcome) { tab->ephemeral=false; Navigate(L"webcom:newtab"); }
        else OpenInternalPage(L"webcom:newtab");
        break;
    case CopyDiagnostics: CopyDiagnosticsText(); break;
    case ExportDiagnosticsBundle: ExportDiagnosticsBundleNow(); break;
    case ExportRecoveryReport: ExportRecoveryReportNow(); break;
    case OpenDataFolder: OpenDataFolderNow(); break;
    case UpdateChannelMenu: UpdateChannelMenuPopup(); break;
    case UpdateChannelStable: SetUpdateChannel(ReleaseChannel::Stable); break;
    case UpdateChannelBeta: SetUpdateChannel(ReleaseChannel::Beta); break;
    case ToggleUpdateChecks: {
        UpdateSettings next=update_settings_; next.checks_enabled=!next.checks_enabled; std::string error;
        if(!SaveUpdateSettings(root_,next,&error)){Notice(L"Update-check preference could not be saved. "+Wide(error));break;}
        update_settings_=next;
        if(!update_settings_.checks_enabled){available_update_.reset();update_runtime_status_.clear();ResetUpdateRetry(update_retry_);SaveUpdateRetryState(root_,update_retry_,nullptr);}
        QueueUI();
        Note(update_settings_.checks_enabled?L"Update checks are enabled. WebCom still will not download or install automatically.":L"Update checks are disabled.");
        break;
    }
    case CheckForUpdates: StartUpdateCheck(); break;
    case StageAvailableUpdate: StageAvailableUpdateNow(); break;
    case InstallStagedUpdate: InstallStagedUpdateNow(); break;
    case RollbackPreviousVersion: RollbackPreviousVersionNow(); break;
    case ClearUpdateTransaction: {
        if(update_transaction_.phase==UpdateTransactionPhase::None)break;
        if(MessageBoxW(window_,L"Clear the staged update/rollback transaction state?\n\nThis does not uninstall WebCom or delete browser data.",L"Clear update state",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)break;
        const auto staged_dir=StagedUpdateDirectory(root_,update_transaction_.target_version);
        UpdateTransaction cleared; std::string error;
        if(!SaveUpdateTransaction(root_,cleared,&error)){Notice(L"Update state could not be cleared. "+Wide(error));break;}
        update_transaction_=cleared;available_update_.reset();update_runtime_status_.clear();
        if(!staged_dir.empty()){std::error_code remove_error;std::filesystem::remove_all(staged_dir,remove_error);}
        QueueUI(); Note(L"Staged update state cleared.");
        break;
    }
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
    case OpenFile: OpenLocalFile(); break;
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
    case RenameProfileDisplayNameCmd: RenameProfileDisplayName(); break;
    case ExportProfileSetupCmd: ExportProfileSetup(); break;
    case ImportProfileSetupCmd: ImportProfileSetup(); break;
    case CreateProfileDesktopShortcutCmd: CreateProfileDesktopShortcut(); break;
    case DeleteCurrentProfileCmd: DeleteCurrentProfile(); break;
    case ShowSiteCookiesCmd: SiteCookiesMenu(); break;
    case ClearSiteCookiesCmd: ClearSiteCookies(); break;
    case ShowCertificateDetailsCmd: ShowCertificateDetails(); break;
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
    case TogglePasswords: {
        auto* preferences = CurrentSavedProfile();
        if (!preferences) break;
        preferences->save_passwords = !preferences->save_passwords;
        if (state_.active_profile == profile_name_) state_.save_passwords = preferences->save_passwords;
        if(auto session=session_.lock())session->ApplyPreferences();
        Note(preferences->save_passwords ? L"Password-save prompts are on for this profile."
            : L"New password-save prompts are off for this profile. Existing saved passwords may still autofill.");
        Dirty(); UpdateUI(); break;
    }
    case ToggleAutofill: {
        auto* preferences = CurrentSavedProfile();
        if (!preferences) break;
        preferences->general_autofill = !preferences->general_autofill;
        if(auto session=session_.lock())session->ApplyPreferences();
        Note(preferences->general_autofill ? L"Address and form autofill is on for this profile."
            : L"Address and form autofill is off for this profile.");
        Dirty(); UpdateUI(); break;
    }
    case ClearSavedPasswords:
        ClearProfileDataKind(COREWEBVIEW2_BROWSING_DATA_KINDS_PASSWORD_AUTOSAVE,
            L"Clear saved passwords",
            L"Clear saved passwords for this WebCom profile?\n\nWebsites may ask you to sign in again. This does not delete downloaded files.",
            L"Saved passwords for this profile were cleared."); break;
    case ClearAutofillData:
        ClearProfileDataKind(COREWEBVIEW2_BROWSING_DATA_KINDS_GENERAL_AUTOFILL,
            L"Clear autofill form data",
            L"Clear saved names, addresses, phone numbers and other form autofill data for this WebCom profile?",
            L"Autofill form data for this profile was cleared."); break;
    case ClearData: ClearBrowsingData(); break;
    case OpenInDefault:
        if (tab && IsWebAddress(tab->url)) ShellExecuteW(window_, L"open", tab->url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        else Note(L"Open a website first.");
        break;
    case InstallRuntime:
        ShellExecuteW(window_, L"open", L"https://developer.microsoft.com/microsoft-edge/webview2/#download-section",
                      nullptr, nullptr, SW_SHOWNORMAL); break;
    case NextTab: case PreviousTab: {
        const auto visible=VisibleTabIndices();
        if(visible.empty())break;
        auto found=std::find(visible.begin(),visible.end(),active_);
        std::size_t position=found==visible.end()?0:static_cast<std::size_t>(found-visible.begin());
        if(command==NextTab)position=(position+1)%visible.size();
        else position=(position+visible.size()-1)%visible.size();
        Select(visible[position]);
        break;
    }
    case FullScreen:
        if (fullscreen_ && tab && tab->web)
            tab->web->ExecuteScript(L"if (document.fullscreenElement) document.exitFullscreen();", nullptr);
        ToggleFullscreen(); break;
    case EngineCrashed:
        AppendRuntimeLog(root_,RuntimeLogLevel::Error,"webview.browser_process_failed","WebView2 browser process exited",nullptr);
        if(auto session=session_.lock())session->InvalidateEnvironment(static_cast<std::uint64_t>(extra));
        break;
    case TabCrashed:
        AppendRuntimeLog(root_,RuntimeLogLevel::Warning,"webview.renderer_process_failed","A renderer process exited",nullptr);
        if (auto* crashed = Find(static_cast<std::uint64_t>(extra))) {
            Release(*crashed); crashed->failed = true;
            crashed->error = L"This page stopped unexpectedly. Press Reload to recover it.";
            UpdateUI();
        }
        break;
    case About:
        MessageBoxW(window_,
            L"WebCom for Windows v2 public beta\n\nNative C++20 / Win32 with Microsoft Edge WebView2.\n\n"
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
        if (key == 'O') return shift ? BookmarkManager : OpenFile;
        if (key == 'S' && shift) return CapturePage;
        if (key == 'P') return shift ? QuickCommands : PrintPage;
        if (key == VK_OEM_PLUS || key == VK_ADD) return ZoomIn;
        if (key == VK_OEM_MINUS || key == VK_SUBTRACT) return ZoomOut;
        if (key == '0' || key == VK_NUMPAD0) return ZoomReset;
        if (shift && key == VK_DELETE) return ClearData;
        if (key == VK_TAB) return shift ? PreviousTab : NextTab;
        if (key == VK_NEXT) return shift ? MoveTabRight : NextTab;
        if (key == VK_PRIOR) return shift ? MoveTabLeft : PreviousTab;
    }
    if (ctrl && alt) {
        if (key == VK_LEFT || key == VK_UP) return PreviousWorkspace;
        if (key == VK_RIGHT || key == VK_DOWN) return NextWorkspace;
        if (key >= '1' && key <= '9') return WorkspaceBase + key - '1';
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
        if (key == VK_F6) return shift ? FocusCycleReverse : FocusCycle;
        if (key == VK_F9) return ReaderMode;
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
            if(!app->appearance_.reduce_motion && app->omnibox_hover_start_)hover_amount=ChromeHoverAmount(static_cast<int>(i),app->omnibox_hover_from_,app->omnibox_hover_to_,
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
            app->omnibox_hover_from_=app->omnibox_hot_;app->omnibox_hover_to_=next;
            app->omnibox_hover_start_=app->appearance_.reduce_motion?0:GetTickCount64();
            app->omnibox_hot_=next;if(!app->appearance_.reduce_motion)SetTimer(app->window_,kAnimationFrame,16,nullptr);InvalidateRect(window,nullptr,FALSE);
        }
        TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);return 0;
    }
    if(message==WM_MOUSELEAVE){
        if(app->omnibox_hot_>=0){app->omnibox_hover_from_=app->omnibox_hot_;app->omnibox_hover_to_=-1;app->omnibox_hover_start_=app->appearance_.reduce_motion?0:GetTickCount64();if(!app->appearance_.reduce_motion)SetTimer(app->window_,kAnimationFrame,16,nullptr);}
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
        RECT close=app->vertical_tabs_?RECT{tab.right-app->D(32),tab.top+app->D(6),tab.right-app->D(8),tab.bottom-app->D(6)}:
            RECT{tab.right-app->D(28),app->D(10),tab.right-app->D(4),app->D(34)};
        return PtInRect(&close,point)!=FALSE;
    };
    if(message==WM_GETOBJECT && static_cast<LONG>(lparam)==OBJID_CLIENT && app->tab_accessible_)
        return LresultFromObject(IID_IAccessible,wparam,app->tab_accessible_.Get());
    if(message==WM_PAINT){app->PaintTabs();return 0;}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_NCHITTEST){
        POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};ScreenToClient(window,&point);
        if(app->vertical_tabs_ && point.y>=0 && point.y<app->D(44))return HTCLIENT;
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
        if(app->vertical_tabs_ && point.y>=0 && point.y<app->D(44) && !app->dragged_tab_id_ && !app->pressed_close_tab_id_){
            app->Dispatch(WorkspaceMenu);return 0;
        }
        const int hit=app->HitTab(point);
        const auto close_id=app->pressed_close_tab_id_;
        const auto dragged=app->dragged_tab_id_;
        const bool dragging=app->tab_dragging_;
        const int target=app->drag_target_index_;
        const int workspace_target=app->drag_workspace_target_;
        const bool should_close=close_id && hit>=0 && static_cast<std::size_t>(hit)<app->tabs_.size() &&
            app->tabs_[static_cast<std::size_t>(hit)]->id==close_id && close_hit(hit,point);
        POINT screen_point=point;ClientToScreen(window,&screen_point);
        std::string drop_profile;
        if(dragging)if(auto session=app->session_.lock())drop_profile=session->ProfileForWindow(WindowFromPoint(screen_point));
        app->CancelTabDrag();
        if(should_close)PostMessageW(app->window_,kCloseTab,0,static_cast<LPARAM>(close_id));
        else if(dragging && !drop_profile.empty() && drop_profile!=app->profile_name_){
            if(auto session=app->session_.lock())session->MoveTabToProfile(app,dragged,drop_profile);
        } else if(dragging && workspace_target>=0 && static_cast<std::size_t>(workspace_target)<app->workspaces_.size()){
            app->AssignTabWorkspace(dragged,Wide(app->workspaces_[static_cast<std::size_t>(workspace_target)].name));
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
                if(app->vertical_tabs_){
                    if(point.x>=-app->D(12) && point.x<=app->D(256)){
                        if(hit>=0){
                            target=hit;const auto bounds=app->DisplayTabRect(static_cast<std::size_t>(hit));
                            if(source<hit && point.y<(bounds.top+bounds.bottom)/2)--target;
                            if(source>hit && point.y>(bounds.top+bounds.bottom)/2)++target;
                        } else {
                            for(std::size_t i=0;i<app->tab_rects_.size();++i){
                                const auto bounds=app->DisplayTabRect(i);if(IsRectEmpty(&bounds))continue;
                                if(point.y<bounds.top){target=static_cast<int>(i);break;}
                                target=static_cast<int>(i);
                            }
                        }
                    }
                } else if(point.y>=-app->D(12) && point.y<=app->D(54)){
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
                RECT strip_bounds{};GetClientRect(window,&strip_bounds);
                int workspace_target=-1;
                for(const auto& drop:app->WorkspaceDropTargets(strip_bounds))
                    if(PtInRect(&drop.rect,point)){workspace_target=static_cast<int>(drop.workspace_index);break;}
                app->drag_workspace_target_=workspace_target;
                app->drag_target_index_=workspace_target>=0?-1:target;
                SetCursor(LoadCursorW(nullptr,IDC_SIZEALL));InvalidateRect(window,nullptr,FALSE);
                return 0;
            }
        }
        const bool close=close_hit(hit,point);
        if(hit!=app->hot_tab_ || close!=app->hot_close_){
            if(hit!=app->hot_tab_){
                app->hover_animation_from_=app->hot_tab_;
                app->hover_animation_to_=hit;
                app->hover_animation_start_=app->appearance_.reduce_motion?0:GetTickCount64();
                if(!app->appearance_.reduce_motion)SetTimer(app->window_,kAnimationFrame,16,nullptr);
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
            app->hover_animation_start_=app->appearance_.reduce_motion?0:GetTickCount64();
            if(!app->appearance_.reduce_motion)SetTimer(app->window_,kAnimationFrame,16,nullptr);
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
            if(!bookmark_dragging_ && (std::abs(point.x-bookmark_drag_start_.x)>D(5) || std::abs(point.y-bookmark_drag_start_.y)>D(5))) {
                bookmark_dragging_=true;bookmark_drag_folder_target_=-1;const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);
            }
            if(bookmark_dragging_){
                int folder_target=-1;
                for(auto it=native_actions_.rbegin();it!=native_actions_.rend();++it){
                    if(it->command>=BookmarkFolderDropBase && it->command<=BookmarkFolderDropBase+999 && PtInRect(&it->rect,point)){
                        folder_target=it->command==BookmarkFolderDropBase+999?-2:static_cast<int>(it->command-BookmarkFolderDropBase);break;
                    }
                }
                int target=bookmark_drag_target_;
                if(folder_target<0){
                    for(const auto& action:native_actions_)if(action.command>=BookmarkBase && action.command<BookmarkBase+bookmarks_.size()){
                        if(point.y<(action.rect.top+action.rect.bottom)/2){target=static_cast<int>(action.command-BookmarkBase);break;}
                        target=static_cast<int>(action.command-BookmarkBase);
                    }
                }else target=-1;
                if(target!=bookmark_drag_target_ || folder_target!=bookmark_drag_folder_target_){
                    bookmark_drag_target_=target;bookmark_drag_folder_target_=folder_target;const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);
                }
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
                    bookmark_drag_target_=bookmark_drag_source_;bookmark_drag_folder_target_=-1;bookmark_drag_start_=point;bookmark_dragging_=false;
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
            const int source=bookmark_drag_source_,target=bookmark_drag_target_,folder_target=bookmark_drag_folder_target_;const bool dragging=bookmark_dragging_;
            bookmark_drag_source_=-1;bookmark_drag_target_=-1;bookmark_drag_folder_target_=-1;bookmark_dragging_=false;
            if(GetCapture()==window_)ReleaseCapture();
            if(dragging && source>=0 && folder_target==-2)MoveBookmarkToFolder(static_cast<std::size_t>(source),L"");
            else if(dragging && source>=0 && folder_target>=0){
                const auto folders=webcom::BookmarkFolderPaths(bookmarks_,explicit_bookmark_folders_,true);
                if(static_cast<std::size_t>(folder_target)<folders.size())MoveBookmarkToFolder(static_cast<std::size_t>(source),Wide(folders[static_cast<std::size_t>(folder_target)]));
            }else if(dragging && source>=0 && target>=0)MoveBookmark(static_cast<std::size_t>(source),static_cast<std::size_t>(target));
            else {const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);}
            return 0;
        }
        if(HitNativeAction(point)) return 0;
        break;
    }
    case WM_CAPTURECHANGED:
        if(bookmark_drag_source_>=0){bookmark_drag_source_=-1;bookmark_drag_target_=-1;bookmark_drag_folder_target_=-1;bookmark_dragging_=false;const RECT content=ContentBounds();InvalidateRect(window_,&content,FALSE);}
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
            if (appearance_.reduce_motion) {
                tab_transition_until_=0;hover_animation_start_=0;hover_animation_from_=-1;hover_animation_to_=-1;
                tab_reflow_start_=0;tab_reflow_from_.clear();omnibox_hover_start_=0;omnibox_hover_from_=-1;omnibox_hover_to_=-1;
                KillTimer(window_,kAnimationFrame);if(tab_strip_)InvalidateRect(tab_strip_,nullptr,FALSE);
                if(suggestion_panel_ && IsWindowVisible(suggestion_panel_))InvalidateRect(suggestion_panel_,nullptr,FALSE);
                break;
            }
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
        if (wparam == kUpdateHealth) ConfirmUpdateHealth();
        if (wparam == kSave) { KillTimer(window_, kSave); Save(false); }
        if (wparam == kToast) { KillTimer(window_, kToast); note_until_ = 0; ShowWindow(toast_, SW_HIDE); }
        return 0;
    case kNotice: {
        queued_notice_ = false;
        const auto text = std::exchange(notice_text_, L"");
        if (!text.empty()) MessageBoxW(window_, text.c_str(), L"WebCom", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    case kUpdateCheckComplete: HandleUpdateCheckCompletion(reinterpret_cast<UpdateCheckCompletion*>(lparam)); return 0;
    case kUpdateStageComplete: HandleUpdateStageCompletion(reinterpret_cast<UpdateStageCompletion*>(lparam)); return 0;
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
        // A graceful close proves the updated browser reached its native event
        // loop and shutdown path; do not count that as a failed health start.
        if (update_health_owner_) ConfirmUpdateHealth();
        // Closing is immediate by user preference. Save normal metadata before
        // releasing controllers; a failed save preserves the previous atomic file.
        Save(false); SaveDownloads();
        closing_ = true;
        CancelTabDrag();
        KillTimer(window_, kTick); KillTimer(window_, kSave); KillTimer(window_, kUiFrame); KillTimer(window_, kToast);
        KillTimer(window_, kAnimationFrame); KillTimer(window_, kDownloadTick); KillTimer(window_, kUpdateHealth);
        for (auto& tab : tabs_) Release(*tab);
        environment_.Reset();
        DestroyWindow(window_);
        return 0;
    }
    case WM_QUERYENDSESSION: if(update_health_owner_)ConfirmUpdateHealth(); Save(false); return TRUE;
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
