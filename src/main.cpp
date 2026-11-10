#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "app.h"
#include "launch_request.h"
#include <shellapi.h>
#include <shobjidl.h>
#include <exception>
#include <string>

namespace {
std::string Utf8Argument(std::wstring_view value) {
    if (value.empty()) return {};
    if (value.size() > 16384) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string encoded(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), encoded.data(), size, nullptr, nullptr);
    return encoded;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetCurrentProcessExplicitAppUserModelID(L"WebCom.Windows");

    int argument_count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    std::wstring address;
    std::wstring profile;
    std::wstring argument_error;
    bool start_private = false;
    bool show_help = false;
    bool address_supplied = false;
    bool profile_supplied = false;
    if (arguments) {
        for (int i = 1; i < argument_count; ++i) {
            if (wcscmp(arguments[i], L"--private") == 0) start_private = true;
            else if (wcscmp(arguments[i], L"--help") == 0) show_help = true;
            else if (wcscmp(arguments[i], L"--profile") == 0) {
                if (profile_supplied || i + 1 >= argument_count || wcsncmp(arguments[i + 1], L"--", 2) == 0) {
                    argument_error = L"Use --profile followed by one profile name.\nExample: WebCom --profile Work";
                    break;
                }
                profile = arguments[++i];
                profile_supplied = true;
            }
            else if (wcscmp(arguments[i], L"--url") == 0) {
                if (address_supplied || i + 1 >= argument_count ||
                    wcscmp(arguments[i + 1], L"--private") == 0 ||
                    wcscmp(arguments[i + 1], L"--help") == 0 ||
                    wcscmp(arguments[i + 1], L"--profile") == 0 ||
                    wcscmp(arguments[i + 1], L"--url") == 0) {
                    argument_error = L"Use --url followed by one address.\nExample: WebCom --private --url https://example.com";
                    break;
                }
                address = arguments[++i];
                address_supplied = true;
            } else if (wcsncmp(arguments[i], L"--", 2) == 0) {
                argument_error = L"Unknown option. Use --help to see the supported commands.";
                break;
            } else if (!address_supplied) {
                address = arguments[i];
                address_supplied = true;
            } else {
                argument_error = L"Pass one address or search phrase. Put a search phrase containing spaces in quotes.";
                break;
            }
        }
        LocalFree(arguments);
    }
    if (show_help) {
        MessageBoxW(nullptr,
            L"WebCom [URL]\nWebCom --private [URL]\nWebCom --profile Work [URL]\nWebCom --url https://example.com\n\n"
            L"--profile opens or focuses a separate window for that profile. New names create a local profile.\n"
            L"--private opens a private tab in the selected profile's window.\n"
            L"Search text uses Google. Quote phrases containing spaces.\n\n"
            L"Ctrl+T: new tab   Ctrl+L: address   Ctrl+W: close tab",
            L"WebCom", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (address.size() > 16384) argument_error = L"The address is too long. WebCom accepts up to 16,384 characters.";
    const std::string encoded_profile = Utf8Argument(profile);
    const std::string encoded_address = Utf8Argument(address);
    if (profile_supplied && !webcom::IsValidProfileName(encoded_profile))
        argument_error = L"Profile names use 1 to 40 letters, numbers, spaces, underscores or hyphens. Avoid leading or trailing spaces and Windows device names.";
    if (!address.empty() && encoded_address.empty()) argument_error = L"The address contains invalid text or is too long.";
    const auto launch_packet = webcom::EncodeLaunchRequest({encoded_profile, encoded_address, start_private});
    if (launch_packet.empty() && argument_error.empty()) argument_error = L"The launch request contains invalid text.";
    if (!argument_error.empty()) {
        MessageBoxW(nullptr, argument_error.c_str(), L"WebCom", MB_OK | MB_ICONERROR);
        return 2;
    }

    // One owner of the session file and user-data folder per Windows session.
    HANDLE instance_mutex = CreateMutexW(nullptr, FALSE, L"Local\\WebCom.Windows.v01");
    if (!instance_mutex) {
        MessageBoxW(nullptr, L"WebCom could not create its startup lock.", L"WebCom", MB_ICONERROR);
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        int result = 0;
        if (HWND existing = FindWindowW(webcom::BrowserApp::WindowClass, nullptr)) {
            // The receiver is merely an IPC entry point, not necessarily the
            // requested profile. Let Session select and activate the target.
            DWORD existing_process = 0;
            GetWindowThreadProcessId(existing, &existing_process);
            if (existing_process) AllowSetForegroundWindow(existing_process);
            COPYDATASTRUCT data{};
            data.dwData = webcom::BrowserApp::OpenLaunchMessage;
            data.cbData = static_cast<DWORD>(launch_packet.size());
            data.lpData = const_cast<std::uint8_t*>(launch_packet.data());
            DWORD_PTR accepted = 0;
            if (!SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
                                     SMTO_BLOCK | SMTO_ABORTIFHUNG, 5000, &accepted) || !accepted) {
                MessageBoxW(nullptr,
                    L"The running WebCom could not accept this request. If it is an older version, close every WebCom window and launch this version again.\n\n"
                    L"Otherwise, close any open dialog and retry; also check the profile and tab limits.",
                    L"WebCom", MB_OK | MB_ICONINFORMATION);
                result = 1;
            }
        } else {
            MessageBoxW(nullptr, L"WebCom is already starting. Please try again in a moment.",
                        L"WebCom", MB_OK | MB_ICONINFORMATION);
            result = 1;
        }
        CloseHandle(instance_mutex);
        return result;
    }

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) {
        CloseHandle(instance_mutex);
        MessageBoxW(nullptr, L"Windows could not initialize WebCom's browser thread.", L"WebCom", MB_ICONERROR);
        return 1;
    }
    int exit_code = 0;
    try {
        webcom::BrowserApp app(instance);
        if (!app.Create(show_command, address, start_private, profile)) exit_code = 1;
        else {
            MSG message{};
            BOOL result = 0;
            while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                if (!app.HandleKeyMessage(message)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            exit_code = result == -1 ? 1 : static_cast<int>(message.wParam);
        }
    } catch (const std::exception&) {
        MessageBoxW(nullptr,
            L"WebCom could not start. Check that your Windows user has a writable Local AppData folder.",
            L"WebCom", MB_OK | MB_ICONERROR);
        exit_code = 1;
    }
    CoUninitialize();
    CloseHandle(instance_mutex);
    return exit_code;
}
