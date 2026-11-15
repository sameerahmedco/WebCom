#include "home_model.h"

#include <algorithm>
#include <cwctype>
#include <unordered_set>

namespace webcom {
namespace {
std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

bool StartsWithInsensitive(std::wstring_view value, std::wstring_view prefix) {
    if (value.size() < prefix.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
        if (std::towlower(value[i]) != std::towlower(prefix[i])) return false;
    return true;
}
} // namespace

std::wstring HomeDisplayHost(std::wstring_view url) {
    std::size_t start = 0;
    if (StartsWithInsensitive(url, L"https://")) start = 8;
    else if (StartsWithInsensitive(url, L"http://")) start = 7;
    else return {};

    auto end = url.find_first_of(L"/:?#", start);
    if (end == std::wstring_view::npos) end = url.size();
    if (end <= start) return {};
    std::wstring host(url.substr(start, end - start));
    const auto at = host.rfind(L'@');
    if (at != std::wstring::npos) host = host.substr(at + 1);
    if (host.size() > 4 && StartsWithInsensitive(host, L"www.")) host.erase(0, 4);
    return Lower(std::move(host));
}

std::vector<HomeRecentSite> BuildHomeRecentSites(const std::vector<HistoryRecord>& entries,
                                                  std::size_t maximum) {
    maximum = std::min<std::size_t>(maximum, 12);
    std::vector<HomeRecentSite> out;
    out.reserve(std::min(maximum, entries.size()));
    std::unordered_set<std::wstring> seen;
    for (std::size_t i = 0; i < entries.size() && out.size() < maximum; ++i) {
        const auto host = HomeDisplayHost(entries[i].url);
        if (host.empty() || !seen.insert(host).second) continue;
        HomeRecentSite site;
        site.source_index = i;
        site.url = entries[i].url;
        site.title = entries[i].title.empty() ? host : entries[i].title;
        site.host = host;
        out.push_back(std::move(site));
    }
    return out;
}

} // namespace webcom
