#include "memory_snapshot_model.h"

#include <iomanip>
#include <sstream>

namespace webcom {
namespace {
double MiB(std::uint64_t bytes) { return static_cast<double>(bytes) / 1048576.0; }
}

std::wstring BuildMemorySnapshotText(const MemorySnapshot& s) {
    const auto total_working = s.host_working_set + s.webview_working_set;
    const auto total_private = s.host_private_commit + s.webview_private_commit;
    std::wostringstream out;
    out << std::fixed << std::setprecision(1)
        << L"WebCom memory snapshot\n\n"
        << L"Total working set: " << MiB(total_working) << L" MiB\n"
        << L"Total private commit: " << MiB(total_private) << L" MiB\n\n"
        << L"WebCom host: " << MiB(s.host_working_set) << L" MiB working set • "
        << MiB(s.host_private_commit) << L" MiB private commit\n"
        << L"WebView2 group: " << MiB(s.webview_working_set) << L" MiB working set • "
        << MiB(s.webview_private_commit) << L" MiB private commit\n"
        << L"Processes measured: " << s.measured_processes << L" of " << s.expected_processes
        << L" (" << s.webview_processes << L" WebView2)\n\n"
        << L"Tabs: " << s.tabs << L" total • " << s.web_tabs << L" web • "
        << s.sleeping_tabs << L" sleeping\n"
        << L"Workspaces: " << s.workspaces << L"\n"
        << L"Automatic sleep: " << (s.sleep_policy.empty() ? L"Unknown" : s.sleep_policy) << L"\n"
        << L"Never-sleep sites: " << s.never_sleep_sites << L"\n\n";
    if (!s.runtime_process_list_available)
        out << L"PARTIAL RESULT: WebView2 did not expose its process list, so host-only values must not be treated as the browser total.\n\n";
    if (s.measured_processes != s.expected_processes)
        out << L"Some processes exited or could not be queried. This snapshot is incomplete.\n\n";
    out << L"Working set is resident memory and can count shared pages more than once. Private commit is private allocated memory and is not the same as physical RAM in use.\n\n"
        << L"For release acceptance, compare the same sites and tab states using scripts\\Measure-WebComMemory.ps1. No percentage saving versus another browser is claimed without a matched Windows measurement.";
    return out.str();
}

} // namespace webcom
