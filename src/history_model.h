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

} // namespace webcom
