#include "history_model.h"
#include <algorithm>
#include <cwctype>

namespace webcom {
namespace {
std::wstring Fold(std::wstring_view value) {
    std::wstring out;
    out.reserve(value.size());
    for (wchar_t c : value) out.push_back(static_cast<wchar_t>(std::towlower(c)));
    return out;
}
}

bool HistoryMatches(const HistoryRecord& entry, std::wstring_view query) {
    if (query.empty()) return true;
    const auto needle = Fold(query);
    return Fold(entry.title).find(needle) != std::wstring::npos ||
           Fold(entry.url).find(needle) != std::wstring::npos;
}

std::vector<std::size_t> FilterHistory(const std::vector<HistoryRecord>& entries,
                                       std::wstring_view query) {
    std::vector<std::size_t> result;
    result.reserve(entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (HistoryMatches(entries[i], query)) result.push_back(i);
    return result;
}

std::size_t DeleteHistoryMatches(std::vector<HistoryRecord>& entries,
                                 std::wstring_view query) {
    if (query.empty()) return 0;
    const auto before = entries.size();
    std::erase_if(entries, [&](const HistoryRecord& item){ return HistoryMatches(item, query); });
    return before - entries.size();
}

std::size_t DeleteHistoryOlderThan(std::vector<HistoryRecord>& entries,
                                   std::uint64_t cutoff_ms) {
    const auto before = entries.size();
    std::erase_if(entries, [&](const HistoryRecord& item){ return item.visited_ms < cutoff_ms; });
    return before - entries.size();
}

std::size_t DeleteHistoryRange(std::vector<HistoryRecord>& entries,
                               std::uint64_t start_ms,
                               std::uint64_t end_ms) {
    if (end_ms < start_ms) std::swap(start_ms, end_ms);
    const auto before = entries.size();
    std::erase_if(entries, [&](const HistoryRecord& item){
        return item.visited_ms >= start_ms && item.visited_ms <= end_ms;
    });
    return before - entries.size();
}

} // namespace webcom
