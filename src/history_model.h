#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct HistoryRecord {
    std::wstring url;
    std::wstring title;
    std::uint64_t visited_ms = 0;
};

struct HistoryHostSummary {
    std::wstring host;
    std::size_t visits = 0;
    std::uint64_t newest_ms = 0;
};

enum class HistoryDeleteMode {
    VisibleMatches,
    OlderThan,
    TimeRange
};

bool HistoryMatches(const HistoryRecord& entry, std::wstring_view query);
std::vector<std::size_t> FilterHistory(const std::vector<HistoryRecord>& entries,
                                       std::wstring_view query);
std::size_t DeleteHistoryMatches(std::vector<HistoryRecord>& entries,
                                 std::wstring_view query);
std::size_t DeleteHistoryOlderThan(std::vector<HistoryRecord>& entries,
                                   std::uint64_t cutoff_ms);
std::size_t DeleteHistoryRange(std::vector<HistoryRecord>& entries,
                               std::uint64_t start_ms,
                               std::uint64_t end_ms);
std::wstring HistoryHost(std::wstring_view url);
std::vector<HistoryHostSummary> BuildHistoryHostSummaries(
    const std::vector<HistoryRecord>& entries, std::size_t maximum = 8);
std::size_t DeleteHistoryHost(std::vector<HistoryRecord>& entries, std::wstring_view host);

} // namespace webcom
