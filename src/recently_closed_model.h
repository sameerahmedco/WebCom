#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

constexpr std::size_t kMaxRecentlyClosedEntries = 20;

struct RecentlyClosedEntry {
    std::string url;
    std::string title;
    std::string workspace;
    std::string group;
    bool pinned = false;
    bool keep_awake = false;
    std::uint64_t closed_at_ms = 0;
};

std::vector<RecentlyClosedEntry> LoadRecentlyClosed(const std::filesystem::path& root,
                                                     std::string_view profile);
bool SaveRecentlyClosed(const std::filesystem::path& root, std::string_view profile,
                        const std::vector<RecentlyClosedEntry>& entries,
                        std::string* error = nullptr);

bool AddRecentlyClosed(std::vector<RecentlyClosedEntry>& entries, RecentlyClosedEntry entry);
bool RemoveRecentlyClosed(std::vector<RecentlyClosedEntry>& entries, std::size_t index,
                          RecentlyClosedEntry* removed = nullptr);

} // namespace webcom
