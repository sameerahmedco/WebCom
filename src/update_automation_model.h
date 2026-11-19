#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace webcom {

struct UpdateAutomationState {
    std::uint64_t last_background_check_unix_seconds = 0;
    bool operator==(const UpdateAutomationState&) const = default;
};

constexpr std::uint64_t kBackgroundUpdateCheckIntervalSeconds = 6ull * 60ull * 60ull;
constexpr std::uint64_t kBackgroundUpdateInitialDelaySeconds = 60ull;

bool BackgroundUpdateCheckDue(const UpdateAutomationState& state,
                              std::uint64_t now_unix_seconds);
bool BackgroundUpdateQuickRetryAllowed(unsigned consecutive_failures);
std::uint64_t BackgroundUpdateCheckDelaySeconds(const UpdateAutomationState& state,
                                                std::uint64_t now_unix_seconds,
                                                std::uint64_t retry_remaining_seconds,
                                                bool startup_delay);
void RecordBackgroundUpdateCheck(UpdateAutomationState& state,
                                 std::uint64_t now_unix_seconds);
UpdateAutomationState LoadUpdateAutomationState(const std::filesystem::path& root);
bool SaveUpdateAutomationState(const std::filesystem::path& root,
                               const UpdateAutomationState& state,
                               std::string* error = nullptr);

} // namespace webcom
