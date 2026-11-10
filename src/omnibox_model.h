#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

enum class OmniboxSuggestionKind { Search, OpenTab, Bookmark, History };

struct OmniboxCandidate {
    OmniboxSuggestionKind kind = OmniboxSuggestionKind::History;
    std::wstring title;
    std::wstring url;
    std::uint64_t recency = 0;
    std::uint64_t tab_id = 0;
    bool active = false;
};

struct OmniboxSuggestion {
    OmniboxSuggestionKind kind = OmniboxSuggestionKind::Search;
    std::wstring title;
    std::wstring detail;
    std::wstring url;
    std::uint64_t tab_id = 0;
    int score = 0;
};

std::vector<OmniboxSuggestion> BuildOmniboxSuggestions(
    std::wstring_view query,
    const std::vector<OmniboxCandidate>& candidates,
    std::size_t max_results = 8);

} // namespace webcom
