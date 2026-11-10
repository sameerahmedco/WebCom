#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct TabSearchItem {
    std::uint64_t id = 0;
    std::wstring title;
    std::wstring url;
    bool private_mode = false;
    bool audible = false;
    bool muted = false;
    bool sleeping = false;
    bool active = false;
};

// Returns source indices in their original order. Every whitespace-separated
// query word must occur in the title or URL, ignoring case with invariant
// Windows Unicode mapping (the user's character locale on portable builds).
// No global locale is changed. Zero IDs are not selectable. Work is bounded to the
// first 200 input items and the first 512 query characters.
std::vector<std::size_t> FilterTabSearchItems(std::span<const TabSearchItem> items,
                                             std::wstring_view query);

} // namespace webcom
