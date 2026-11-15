#pragma once

#include "history_model.h"

#include <cstddef>
#include <string>
#include <vector>

namespace webcom {

struct HomeRecentSite {
    std::size_t source_index = 0;
    std::wstring url;
    std::wstring title;
    std::wstring host;
};

// Builds a stable, bounded list of recent HTTP(S) sites for the native New Tab
// page. Multiple visits to the same host collapse to the newest entry.
std::vector<HomeRecentSite> BuildHomeRecentSites(
    const std::vector<HistoryRecord>& entries,
    std::size_t maximum = 6);

std::wstring HomeDisplayHost(std::wstring_view url);

} // namespace webcom
