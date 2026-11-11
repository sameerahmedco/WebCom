#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace webcom {

struct UpdateRetryState {
    unsigned consecutive_failures = 0;
    std::uint64_t retry_after_unix_seconds = 0;
    std::string last_failure_category;
    bool operator==(const UpdateRetryState&) const = default;
};

std::uint64_t UpdateRetryDelaySeconds(unsigned consecutive_failures);
std::uint64_t UpdateRetryRemainingSeconds(const UpdateRetryState& state,
                                          std::uint64_t now_unix_seconds);
bool RegisterUpdateRetryFailure(UpdateRetryState& state, std::uint64_t now_unix_seconds,
                                std::string_view category, std::string* error = nullptr);
void ResetUpdateRetry(UpdateRetryState& state);
UpdateRetryState LoadUpdateRetryState(const std::filesystem::path& root);
bool SaveUpdateRetryState(const std::filesystem::path& root, const UpdateRetryState& state,
                          std::string* error = nullptr);

} // namespace webcom
