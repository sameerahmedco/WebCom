#pragma once

#include <filesystem>
#include <string>
#include <cstddef>

namespace webcom {


struct OnboardingProgress {
    unsigned completed = 0;
    unsigned total = 4;
    bool default_browser = false;
    bool bookmarks_ready = false;
    bool workspaces_ready = false;
    bool updates_ready = false;
};

OnboardingProgress BuildOnboardingProgress(bool default_browser, std::size_t bookmark_count,
                                           std::size_t workspace_count, bool update_checks_enabled);

struct OnboardingState {
    bool welcome_seen = false;
    bool operator==(const OnboardingState&) const = default;
};

OnboardingState LoadOnboardingState(const std::filesystem::path& root);
bool SaveOnboardingState(const std::filesystem::path& root, const OnboardingState& state,
                         std::string* error = nullptr);

} // namespace webcom
