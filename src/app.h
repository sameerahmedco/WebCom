#pragma once

#include <windows.h>
#include <memory>
#include <string>
#ifdef WEBCOM_TEST_MODE
#include <filesystem>
#endif

namespace webcom {

// The implementation stays private to keep WebView2 headers out of main.cpp.
class BrowserApp {
public:
    explicit BrowserApp(HINSTANCE instance);
#ifdef WEBCOM_TEST_MODE
    BrowserApp(HINSTANCE instance, const std::filesystem::path& isolated_data_root);
#endif
    ~BrowserApp();
    BrowserApp(const BrowserApp&) = delete;
    BrowserApp& operator=(const BrowserApp&) = delete;
    bool Create(int show_command, const std::wstring& initial_address, bool start_private = false,
                const std::wstring& initial_profile = L"");
    bool HandleKeyMessage(const MSG& message);
    static constexpr const wchar_t* WindowClass = L"WebCom.NativeBrowser.Window";
    static constexpr ULONG_PTR OpenAddressMessage = 0x57454243;
    static constexpr ULONG_PTR OpenPrivateAddressMessage = 0x57454250;
    static constexpr ULONG_PTR OpenLaunchMessage = 0x57454233;
private:
    class Impl;
    class Session;
    std::shared_ptr<Session> session_;
};

} // namespace webcom
