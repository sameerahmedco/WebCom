// Native Windows regression tests. The app uses a temporary data directory and
// only native new-tab pages: no website, WebView2 engine or user profile is opened.
#include "app.h"
#include "appearance.h"
#include "chrome_layout.h"
#include "launch_request.h"
#include "storage.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
bool saw_quit = false;
unsigned unexpected_dialogs = 0;

void Check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}

// A regression in WM_CLOSE must fail, not leave unattended CI blocked in a
// confirmation dialog. Timer callbacks run inside MessageBox's nested loop.
struct DialogWatchdog {
    UINT_PTR timer = 0;
    DialogWatchdog() {
        timer = SetTimer(nullptr, 0, 100, [](HWND, UINT, UINT_PTR, DWORD) {
            EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM) -> BOOL {
                wchar_t type[64]{}; GetClassNameW(window, type, 64);
                if (wcscmp(type, L"#32770") == 0 && IsWindowVisible(window)) {
                    ++unexpected_dialogs;
                    EndDialog(window, IDCANCEL);
                }
                return TRUE;
            }, 0);
        });
        if (!timer) throw std::runtime_error("Cannot install the native dialog watchdog.");
    }
    ~DialogWatchdog() { if (timer) KillTimer(nullptr, timer); }
};

struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory() {
        const auto temp = std::filesystem::temp_directory_path();
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            const auto candidate = temp / (L"webcom-native-ui-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(stamp) + L"-" + std::to_wstring(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) { path = candidate; return; }
            if (error && error != std::errc::file_exists) break;
        }
        throw std::runtime_error("Cannot create isolated UI-test data directory.");
    }
    ~TemporaryDirectory() {
        if (!path.empty()) { std::error_code error; std::filesystem::remove_all(path, error); }
    }
};

std::wstring WindowText(HWND window) {
    const int size = GetWindowTextLengthW(window);
    std::wstring result(static_cast<std::size_t>(size) + 1, L'\0');
    GetWindowTextW(window, result.data(), size + 1); result.resize(size);
    return result;
}

std::vector<HWND> BrowserWindows() {
    std::vector<HWND> windows;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM context) -> BOOL {
        wchar_t type[128]{}; GetClassNameW(window, type, 128);
        if (wcscmp(type, webcom::BrowserApp::WindowClass) == 0)
            reinterpret_cast<std::vector<HWND>*>(context)->push_back(window);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

HWND ProfileWindow(const std::wstring& name) {
    for (HWND window : BrowserWindows())
        if (WindowText(window).find(L" - WebCom - " + name) != std::wstring::npos) return window;
    return nullptr;
}

void Pump(webcom::BrowserApp& app, DWORD duration_ms = 40) {
    const auto end = GetTickCount64() + duration_ms;
    do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { saw_quit = true; continue; }
            if (!app.HandleKeyMessage(message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        if (GetTickCount64() < end) Sleep(1);
    } while (GetTickCount64() < end);
}

bool Shortcut(webcom::BrowserApp& app, HWND window, UINT key, bool shift = false) {
    std::array<BYTE, 256> original{};
    if (!GetKeyboardState(original.data())) return false;
    auto state = original;
    state[VK_CONTROL] = 0x80; state[VK_LCONTROL] = 0x80;
    state[VK_SHIFT] = shift ? 0x80 : 0; state[VK_LSHIFT] = state[VK_SHIFT];
    state[VK_MENU] = 0; state[VK_LMENU] = 0; state[VK_RMENU] = 0;
    if (!SetKeyboardState(state.data())) return false;
    MSG message{}; message.hwnd = window; message.message = WM_KEYDOWN; message.wParam = key;
    const bool handled = app.HandleKeyMessage(message);
    SetKeyboardState(original.data());
    return handled;
}

bool Launch(HWND receiver, const webcom::LaunchRequest& request) {
    const auto bytes = webcom::EncodeLaunchRequest(request);
    COPYDATASTRUCT data{};
    data.dwData = webcom::BrowserApp::OpenLaunchMessage;
    data.cbData = static_cast<DWORD>(bytes.size());
    data.lpData = const_cast<std::uint8_t*>(bytes.data());
    return SendMessageW(receiver, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data)) != FALSE;
}

std::size_t SavedCount(const webcom::AppState& state, const std::string& name) {
    for (const auto& profile : state.profiles) if (profile.name == name) return profile.tabs.size();
    return 0;
}

HWND BookmarkButton(HWND owner) {
    struct Context { HWND result = nullptr; } context;
    EnumChildWindows(owner, [](HWND child, LPARAM data) -> BOOL {
        if (WindowText(child).starts_with(L"Bookmark this page")) {
            reinterpret_cast<Context*>(data)->result = child; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    return context.result;
}

HWND AddressEdit(HWND owner) {
    struct Context { HWND owner, result = nullptr; } context{owner};
    EnumChildWindows(owner, [](HWND child, LPARAM data) -> BOOL {
        auto& current = *reinterpret_cast<Context*>(data);
        wchar_t type[64]{}; GetClassNameW(child, type, 64);
        if (wcscmp(type, L"Edit") != 0) return TRUE;
        RECT bounds{}; GetWindowRect(child, &bounds);
        MapWindowPoints(nullptr, current.owner, reinterpret_cast<POINT*>(&bounds), 2);
        if (bounds.top < MulDiv(84, static_cast<int>(GetDpiForWindow(current.owner)), 96)) {
            current.result = child; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    return context.result;
}

HWND SuggestionPanel(HWND owner) {
    struct Context { HWND owner, result=nullptr; } context{owner};
    EnumChildWindows(owner, [](HWND child, LPARAM data)->BOOL {
        auto& current=*reinterpret_cast<Context*>(data);
        if(!IsWindowVisible(child) || !WindowText(child).empty()) return TRUE;
        wchar_t type[64]{};GetClassNameW(child,type,64);
        if(wcscmp(type,L"Static")!=0) return TRUE;
        RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,current.owner,reinterpret_cast<POINT*>(&bounds),2);
        const int dpi=static_cast<int>(GetDpiForWindow(current.owner));
        if(bounds.top>=MulDiv(80,dpi,96) && bounds.top<=MulDiv(100,dpi,96) && bounds.bottom-bounds.top>MulDiv(50,dpi,96)) {
            current.result=child;return FALSE;
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&context));
    return context.result;
}

HWND TabStrip(HWND owner) {
    HWND result = nullptr;
    EnumChildWindows(owner, [](HWND child, LPARAM data) -> BOOL {
        if (WindowText(child).starts_with(L"Tabs:")) {
            *reinterpret_cast<HWND*>(data) = child; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

bool HasVisibleBookmarkRow(HWND owner) {
    struct Context { HWND owner; bool visible = false; } context{owner};
    EnumChildWindows(owner, [](HWND child, LPARAM data) -> BOOL {
        auto& current = *reinterpret_cast<Context*>(data);
        if (!IsWindowVisible(child) || WindowText(child) != L"Google") return TRUE;
        RECT bounds{}; GetWindowRect(child, &bounds);
        MapWindowPoints(nullptr, current.owner, reinterpret_cast<POINT*>(&bounds), 2);
        const UINT dpi = GetDpiForWindow(current.owner);
        if (bounds.top >= MulDiv(84, static_cast<int>(dpi), 96) && bounds.bottom <= MulDiv(121, static_cast<int>(dpi), 96)) {
            current.visible = true; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    return context.visible;
}

COLORREF Rgb(std::uint32_t rgb) { return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }

bool AddressBackgroundMatches(HWND owner, const webcom::Appearance& appearance, std::string_view profile) {
    HWND edit = AddressEdit(owner);
    if (!edit) return false;
    SetFocus(owner);
    HDC dc = GetDC(edit);
    if (!dc) return false;
    SendMessageW(owner, WM_CTLCOLOREDIT, reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(edit));
    const COLORREF observed = GetBkColor(dc);
    ReleaseDC(edit, dc);
    return observed == Rgb(webcom::MakePalette(appearance, profile).surface);
}

LRESULT CALLBACK ObserveVisibility(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                    UINT_PTR, DWORD_PTR context) {
    if (message == WM_SHOWWINDOW && wparam) ++*reinterpret_cast<unsigned*>(context);
    return DefSubclassProc(window, message, wparam, lparam);
}

void Capture(webcom::BrowserApp& app, HWND window, const wchar_t* name) {
    const DWORD required = GetEnvironmentVariableW(L"WEBCOM_UI_CAPTURE_DIR", nullptr, 0);
    if (!required || !window) return;
    std::wstring directory(required, L'\0');
    if (!GetEnvironmentVariableW(L"WEBCOM_UI_CAPTURE_DIR", directory.data(), required)) return;
    directory.resize(wcslen(directory.c_str()));
    std::error_code error; std::filesystem::create_directories(directory, error);
    if (error) { std::cerr << "Screenshot directory unavailable.\n"; return; }
    ShowWindow(window, SW_SHOWMAXIMIZED); SetForegroundWindow(window); BringWindowToTop(window);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    Pump(app, 120); DwmFlush();
    if (GetForegroundWindow() != window) {
        std::cerr << "Screenshot skipped: no foreground interactive desktop.\n"; return;
    }
    RECT bounds{}; GetClientRect(window, &bounds);
    POINT origin{}; ClientToScreen(window, &origin);
    const int width = bounds.right, height = bounds.bottom;
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return;
    HDC screen = GetDC(nullptr), memory = screen ? CreateCompatibleDC(screen) : nullptr;
    if (!screen || !memory) { if (screen) ReleaseDC(nullptr, screen); return; }
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (bitmap) {
        const auto previous = SelectObject(memory, bitmap);
        if (BitBlt(memory, 0, 0, width, height, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT)) {
            GdiFlush();
            BITMAPFILEHEADER header{}; header.bfType = 0x4d42;
            header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
            header.bfSize = header.bfOffBits + static_cast<DWORD>(width * height * 4);
            std::ofstream output(std::filesystem::path(directory) / name, std::ios::binary);
            output.write(reinterpret_cast<const char*>(&header), sizeof(header));
            output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(BITMAPINFOHEADER));
            output.write(static_cast<const char*>(pixels), static_cast<std::streamsize>(width) * height * 4);
            if (!output) std::cerr << "Screenshot write failed.\n";
        }
        SelectObject(memory, previous); DeleteObject(bitmap);
    }
    DeleteDC(memory); ReleaseDC(nullptr, screen);
}
} // namespace

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) { std::cerr << "COM initialization failed.\n"; return 1; }
    try {
        TemporaryDirectory temporary;
        DialogWatchdog dialog_watchdog;
        webcom::AppState seed;
        seed.profiles = {
            {"Default", {{"webcom:newtab","Default one",false,false},{"webcom:newtab","Default two",false,false}},0,{},true,true,{{"Main",0}},"Main",false,false,webcom::StartupMode::Continue,{}},
            {"Work", {{"webcom:newtab","Work one",false,false},{"webcom:newtab","Work two",false,false},
                      {"webcom:newtab","Work three",false,false}},0,{},true,true,{{"Main",0}},"Main",false,false,webcom::StartupMode::Continue,{}}
        };
        Check(webcom::SaveState(temporary.path, seed), "seed isolated profile state");
        const webcom::Appearance default_appearance{webcom::BrowserTheme::Charcoal, 0xd8a573, false};
        const webcom::Appearance work_appearance{webcom::BrowserTheme::Midnight, 0x8bd4bf, true};
        Check(webcom::SaveAppearance(temporary.path, "Default", default_appearance), "seed Default theme and profile color");
        Check(webcom::SaveAppearance(temporary.path, "Work", work_appearance), "seed Work theme and profile color");
        webcom::BrowserApp app(GetModuleHandleW(nullptr), temporary.path);
        Check(app.Create(SW_SHOWNOACTIVATE, L"", false, L"Default"), "create first native profile window");
        Pump(app, 120);
        HWND first = ProfileWindow(L"Default");
        Check(first && IsWindow(first), "Default window exists");
        if (!first) throw std::runtime_error("Cannot continue without the first window.");
        Check(Launch(first, {"Work", "", false}), "open second profile through real IPC");
        Pump(app, 120);
        HWND work = ProfileWindow(L"Work");
        Check(work && work != first && IsWindow(first), "two profile windows coexist");
        Check(BrowserWindows().size() == 2, "one native window per profile");
        if (!work) throw std::runtime_error("Cannot continue without the Work window.");
        Check(Launch(first, {"work", "", false}), "case-insensitive profile launch accepted");
        Pump(app);
        Check(BrowserWindows().size() == 2 && ProfileWindow(L"Work") == work, "existing profile focused without duplicate window");
        Check(Shortcut(app, first, 'T'), "new tab in Default");
        Check(Shortcut(app, work, 'T'), "new tab in Work");
        Pump(app);
        SendMessageW(first, WM_QUERYENDSESSION, 0, 0);
        auto saved = webcom::LoadState(temporary.path);
        Check(SavedCount(saved, "Default") == 3, "save captures Default live tabs");
        // Launching the already-open Work profile adds one native tab, then Ctrl+T adds another.
        Check(SavedCount(saved, "Work") == 5, "save captures Work live tabs without overwrite");
        Check(saved.profiles.size() == 2, "case alias did not create a third profile");
        Check(AddressBackgroundMatches(first, default_appearance, "Default") &&
              AddressBackgroundMatches(work, work_appearance, "Work"), "each HWND applies its profile's persisted theme");
        Check(!HasVisibleBookmarkRow(first) && HasVisibleBookmarkRow(work), "bookmark-row preference is independent per profile");
        Check(Shortcut(app, first, 'B', true), "Ctrl+Shift+B handled in Default");
        Pump(app);
        Check(HasVisibleBookmarkRow(first) && HasVisibleBookmarkRow(work), "bookmark shortcut shows only the target profile's row");
        auto changed_appearance = webcom::LoadAppearance(temporary.path, "Default");
        Check(changed_appearance.show_bookmarks && changed_appearance.theme == default_appearance.theme &&
              changed_appearance.accent_rgb == default_appearance.accent_rgb,
              "bookmark toggle persists without changing theme or custom profile color");
        Check(webcom::LoadAppearance(temporary.path, "Work") == work_appearance, "appearance change leaves other profile untouched");
        Check(Shortcut(app, first, 'B', true), "Ctrl+Shift+B toggles the row off again");
        Pump(app);
        Check(!HasVisibleBookmarkRow(first) && webcom::LoadAppearance(temporary.path, "Default") == default_appearance,
              "hidden bookmark row and preferences return to saved state");

        HWND edit = AddressEdit(work);
        Check(edit != nullptr, "native omnibox is discoverable");
        if (edit) {
            constexpr wchar_t draft[] = L"first click selects this whole draft";
            SetWindowTextW(edit, draft); SetFocus(work);
            SendMessageW(edit, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 8));
            SendMessageW(edit, WM_LBUTTONUP, 0, MAKELPARAM(12, 8));
            Pump(app);
            DWORD first_selected = 0, last_selected = 0;
            SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&first_selected), reinterpret_cast<LPARAM>(&last_selected));
            Check(first_selected == 0 && last_selected == std::size(draft) - 1,
                  "first mouse click into an unfocused omnibox selects the full draft");
            SendMessageW(edit, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 8));
            SendMessageW(edit, WM_LBUTTONUP, 0, MAKELPARAM(12, 8));
            SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&first_selected), reinterpret_cast<LPARAM>(&last_selected));
            Check(first_selected == last_selected, "second omnibox click places a caret instead of selecting everything again");
            SetWindowTextW(edit, L"example"); SetFocus(edit); Pump(app);
            Check(SuggestionPanel(work)!=nullptr, "typing in the omnibox shows the native suggestion panel");
            SendMessageW(edit,WM_KEYDOWN,VK_ESCAPE,0); Pump(app);
            Check(SuggestionPanel(work)==nullptr, "Escape hides omnibox suggestions without a popup window");
            SetWindowTextW(edit, L""); SetFocus(work);
        }
        Check(Shortcut(app, work, 'O', true), "Ctrl+Shift+O opens bookmark manager");
        Pump(app);
        Check(WindowText(work).find(L"Bookmarks - WebCom - Work")!=std::wstring::npos,
              "bookmark manager is a first-class native tab");
        Check(Shortcut(app, work, 'W'), "close bookmark manager tab"); Pump(app);

        Check(Shortcut(app, first, '1'), "select first tab for pointer reorder");
        Pump(app);
        HWND strip = TabStrip(first);
        Check(strip != nullptr, "native tab strip is discoverable");
        if (strip) {
            RECT client{}; GetClientRect(first, &client);
            const int dpi = static_cast<int>(GetDpiForWindow(first));
            const auto geometry = webcom::MakeChromeStripLayout(MulDiv(client.right, 96, dpi), 3, 0, 0);
            // Cross the second tab's midpoint so the drop inserts after it.
            const int start = MulDiv(48, dpi, 96), finish = MulDiv(geometry.step * 7 / 4, dpi, 96), y = MulDiv(20, dpi, 96);
            SendMessageW(strip, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(start, y));
            SendMessageW(strip, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(finish, y));
            SendMessageW(strip, WM_LBUTTONUP, 0, MAKELPARAM(finish, y));
            Pump(app); SendMessageW(first, WM_QUERYENDSESSION, 0, 0);
            saved = webcom::LoadState(temporary.path);
            const auto profile = std::find_if(saved.profiles.begin(), saved.profiles.end(),
                [](const auto& item) { return item.name == "Default"; });
            Check(profile != saved.profiles.end() && profile->tabs.size() == 3 &&
                  profile->tabs[0].title == "Default two" && profile->tabs[1].title == "Default one",
                  "mouse drag reorders native tabs and persists the intended order");
        }
        Check(Launch(first, {"Work", "", true}), "private launch routes to Work window");
        Pump(app);
        SendMessageW(first, WM_QUERYENDSESSION, 0, 0);
        saved = webcom::LoadState(temporary.path);
        Check(SavedCount(saved, "Work") == 5, "private tab excluded from shared session save");
        Capture(app, work, L"03-work-incognito.bmp");
        Check(Shortcut(app, work, 'W'), "close native private tab without closing profile");
        Pump(app);
        Capture(app, first, L"01-default.bmp");
        Capture(app, work, L"02-work.bmp");
        HWND star = BookmarkButton(work);
        Check(star && !IsWindowVisible(star), "bookmark star hidden on native new tab");
        unsigned unexpected_shows = 0;
        if (star) SetWindowSubclass(star, ObserveVisibility, 77, reinterpret_cast<DWORD_PTR>(&unexpected_shows));
        // Warm fonts and all tab surfaces before measuring steady-state GDI ownership.
        for (unsigned i = 0; i < 10; ++i) { Shortcut(app, work, VK_TAB); Pump(app, 20); }
        const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        bool switches_handled = true;
        for (unsigned i = 0; i < 100; ++i) {
            switches_handled = Shortcut(app, work, VK_TAB) && switches_handled;
            Pump(app, 20);
        }
        const DWORD after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        Check(switches_handled, "100 tab switches handled through native shortcuts");
        Check(unexpected_shows == 0, "hidden star never transiently shown during tab switches");
        Check(before != 0 && after <= before + 8, "GDI handles remain bounded after repeated tab switches");
        if (star) RemoveWindowSubclass(star, ObserveVisibility, 77);
        Check(Launch(work, {"Default", "", true}), "prepare a private tab in a multi-tab window before closing");
        Pump(app);
        SendMessageW(first, WM_CLOSE, 0, 0); Pump(app);
        Check(!IsWindow(first) && IsWindow(work), "closing Default leaves Work running");
        Check(unexpected_dialogs == 0, "multi-tab/private window closes without a confirmation dialog");
        Check(!saw_quit, "closing one profile does not quit the application");
        Check(Shortcut(app, work, 'T'), "remaining profile still accepts new tabs");
        Pump(app); SendMessageW(work, WM_QUERYENDSESSION, 0, 0);
        saved = webcom::LoadState(temporary.path);
        Check(SavedCount(saved, "Default") == 3 && SavedCount(saved, "Work") == 6,
              "closed and open profiles both preserved by later saves");
        Check(Launch(work, {"Default", "", false}), "reopen closed profile alongside the remaining window");
        Pump(app);
        HWND reopened = ProfileWindow(L"Default");
        Check(reopened && IsWindow(work) && AddressBackgroundMatches(reopened, default_appearance, "Default") &&
              !HasVisibleBookmarkRow(reopened), "reopened profile retains theme color and bookmark visibility");
        if (reopened) SendMessageW(reopened, WM_CLOSE, 0, 0);
        Pump(app);
        // Closing is explicitly non-modal even when saving fails. Preserve a
        // backup of the last good test state, and block only this isolated path.
        Check(Shortcut(app, work, 'T'), "mark the final window dirty before a simulated save failure");
        const auto state_file = temporary.path / "state.v1";
        const auto state_backup = temporary.path / "last-good-state.test-backup";
        std::filesystem::rename(state_file, state_backup);
        std::filesystem::create_directory(state_file);
        SendMessageW(work, WM_CLOSE, 0, 0); Pump(app);
        Check(BrowserWindows().empty(), "last profile closes even when its best-effort save cannot replace the file");
        Check(unexpected_dialogs == 0, "all close paths finish without modal warnings");
        Check(saw_quit, "last profile posts WM_QUIT");
        Check(std::filesystem::is_regular_file(state_backup), "last good state remains preserved during failed save");
        std::filesystem::remove_all(state_file);
        std::filesystem::rename(state_backup, state_file);
        Check(!std::filesystem::exists(temporary.path / L"User Data"), "native-only tests never start the browser engine");
        std::cout << "GDI handles before/after switches: " << before << '/' << after << '\n';
    } catch (const std::exception& error) {
        ++failures; std::cerr << "UI test exception: " << error.what() << '\n';
    }
    CoUninitialize();
    std::cout << checks << " native UI checks; " << failures << " failures\n";
    return failures ? 1 : 0;
}
