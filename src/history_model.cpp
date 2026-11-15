#include "history_model.h"
#include <algorithm>
#include <cwctype>
#include <unordered_map>

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

std::wstring HistoryHost(std::wstring_view url) {
    std::size_t start = 0;
    const auto starts = [&](std::wstring_view prefix) {
        if (url.size() < prefix.size()) return false;
        for (std::size_t i = 0; i < prefix.size(); ++i)
            if (std::towlower(url[i]) != std::towlower(prefix[i])) return false;
        return true;
    };
    if (starts(L"https://")) start = 8;
    else if (starts(L"http://")) start = 7;
    else return {};
    auto end = url.find_first_of(L"/:?#", start);
    if (end == std::wstring_view::npos) end = url.size();
    if (end <= start) return {};
    std::wstring host(url.substr(start, end - start));
    const auto at = host.rfind(L'@');
    if (at != std::wstring::npos) host = host.substr(at + 1);
    if (host.size() > 4) {
        const std::wstring prefix = Fold(host.substr(0, 4));
        if (prefix == L"www.") host.erase(0, 4);
    }
    return Fold(host);
}

std::vector<HistoryHostSummary> BuildHistoryHostSummaries(
    const std::vector<HistoryRecord>& entries, std::size_t maximum) {
    maximum = std::min<std::size_t>(maximum, 16);
    std::unordered_map<std::wstring, HistoryHostSummary> by_host;
    for (const auto& entry : entries) {
        const auto host = HistoryHost(entry.url);
        if (host.empty()) continue;
        auto& item = by_host[host];
        item.host = host;
        ++item.visits;
        item.newest_ms = std::max(item.newest_ms, entry.visited_ms);
    }
    std::vector<HistoryHostSummary> result;
    result.reserve(by_host.size());
    for (auto& [_, item] : by_host) result.push_back(std::move(item));
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.visits != b.visits) return a.visits > b.visits;
        if (a.newest_ms != b.newest_ms) return a.newest_ms > b.newest_ms;
        return a.host < b.host;
    });
    if (result.size() > maximum) result.resize(maximum);
    return result;
}

std::size_t DeleteHistoryHost(std::vector<HistoryRecord>& entries, std::wstring_view host) {
    const auto target = Fold(host);
    if (target.empty()) return 0;
    const auto before = entries.size();
    std::erase_if(entries, [&](const HistoryRecord& item) { return HistoryHost(item.url) == target; });
    return before - entries.size();
}

} // namespace webcom
