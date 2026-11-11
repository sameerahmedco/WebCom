#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace webcom {

struct ExtensionFolderInfo {
    bool valid = false;
    std::wstring folder;
    std::string name;
    std::string version;
    std::string description;
    std::string minimum_chrome_version;
    int manifest_version = 0;
    bool declares_toolbar_ui = false;
    bool has_background = false;
    bool has_content_scripts = false;
    bool has_options_page = false;
    bool has_side_panel = false;
    bool has_devtools_page = false;
    bool has_commands = false;
    bool has_omnibox = false;
    bool has_web_accessible_resources = false;
    std::string default_popup;
    std::string options_page;
    std::vector<std::string> permissions;
    std::vector<std::string> host_permissions;
    std::vector<std::string> optional_permissions;
    std::vector<std::string> optional_host_permissions;
    std::string error;
};

// Lightweight preflight for unpacked Chromium extensions. WebView2 remains the
// authority that validates/installs the manifest; this only gives the user a
// bounded preview and catches obvious folder mistakes before the COM call.
ExtensionFolderInfo InspectExtensionFolder(const std::filesystem::path& folder);
std::string ExtensionCompatibilitySummary(const ExtensionFolderInfo& info);

} // namespace webcom
