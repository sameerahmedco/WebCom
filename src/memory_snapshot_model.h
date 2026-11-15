#pragma once

#include <cstdint>
#include <string>

namespace webcom {

struct MemorySnapshot {
    std::uint64_t host_working_set = 0;
    std::uint64_t host_private_commit = 0;
    std::uint64_t webview_working_set = 0;
    std::uint64_t webview_private_commit = 0;
    unsigned measured_processes = 0;
    unsigned expected_processes = 0;
    unsigned webview_processes = 0;
    unsigned tabs = 0;
    unsigned web_tabs = 0;
    unsigned sleeping_tabs = 0;
    unsigned workspaces = 0;
    std::wstring sleep_policy;
    unsigned never_sleep_sites = 0;
    bool runtime_process_list_available = true;
};

std::wstring BuildMemorySnapshotText(const MemorySnapshot& snapshot);

} // namespace webcom
