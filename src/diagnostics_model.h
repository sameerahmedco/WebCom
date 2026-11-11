#pragma once
#include <string>

namespace webcom {
struct DiagnosticsSnapshot {
    std::wstring version;
    std::wstring runtime_version;
    std::wstring profile_key;
    std::wstring profile_label;
    std::wstring update_channel;
    std::wstring active_workspace;
    std::wstring data_path;
    unsigned tabs = 0;
    unsigned web_tabs = 0;
    unsigned sleeping_tabs = 0;
    unsigned workspaces = 0;
    bool vertical_tabs = false;
    bool reduced_motion = false;
    bool high_contrast = false;
};
std::wstring BuildDiagnosticsText(const DiagnosticsSnapshot& snapshot);
} // namespace webcom
