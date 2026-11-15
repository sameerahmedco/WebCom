#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct TabBatchItem {
    std::uint64_t id = 0;
    std::string url;
    std::string workspace;
    bool private_mode = false;
    bool active = false;
    bool pinned = false;
    bool has_download = false;
};

// Finds duplicate tabs within one workspace. The target tab is always kept.
// Pinned, active, and download-owning tabs are never returned for closure.
std::vector<std::uint64_t> DuplicateTabCloseCandidates(
    const std::vector<TabBatchItem>& items,
    std::uint64_t target_id,
    std::string_view workspace);

} // namespace webcom
