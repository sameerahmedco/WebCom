#include "diagnostics_model.h"
#include <sstream>
namespace webcom {
std::wstring BuildDiagnosticsText(const DiagnosticsSnapshot& s) {
    std::wostringstream out;
    out << L"WebCom support diagnostics\r\n"
        << L"Version: " << s.version << L"\r\n"
        << L"WebView2 Runtime: " << (s.runtime_version.empty()?L"Unavailable":s.runtime_version) << L"\r\n"
        << L"Profile: " << (s.profile_label.empty()?s.profile_key:s.profile_label) << L"\r\n"
        << L"Profile key: " << s.profile_key << L"\r\n"
        << L"Update channel: " << s.update_channel << L"\r\n"
        << L"Workspace: " << s.active_workspace << L"\r\n"
        << L"Tabs: " << s.tabs << L" total, " << s.web_tabs << L" web, " << s.sleeping_tabs << L" sleeping\r\n"
        << L"Workspaces: " << s.workspaces << L"\r\n"
        << L"Vertical tabs: " << (s.vertical_tabs?L"On":L"Off") << L"\r\n"
        << L"Reduce motion: " << (s.reduced_motion?L"On":L"Off") << L"\r\n"
        << L"High contrast: " << (s.high_contrast?L"On":L"Off") << L"\r\n"
        << L"Data path: " << s.data_path << L"\r\n";
    return out.str();
}
} // namespace webcom
