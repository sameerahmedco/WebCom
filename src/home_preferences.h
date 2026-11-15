#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct HomeShortcut {
    std::string title;
    std::string url;
};

struct HomePreferences {
    std::vector<HomeShortcut> shortcuts;
    std::vector<std::string> hidden_recent_hosts;
};

constexpr std::size_t kMaxHomeShortcuts = 8;
constexpr std::size_t kMaxHiddenRecentHosts = 64;

HomePreferences LoadHomePreferences(const std::filesystem::path& root,
                                    std::string_view profile);
bool SaveHomePreferences(const std::filesystem::path& root, std::string_view profile,
                         const HomePreferences& preferences,
                         std::string* error = nullptr);

bool AddHomeShortcut(HomePreferences& preferences, HomeShortcut shortcut,
                     std::string* error = nullptr);
bool RemoveHomeShortcut(HomePreferences& preferences, std::size_t index);
bool HideHomeRecentHost(HomePreferences& preferences, std::string host);
bool ShowHomeRecentHost(HomePreferences& preferences, std::string_view host);
bool IsHomeRecentHostHidden(const HomePreferences& preferences, std::string_view host);

} // namespace webcom
