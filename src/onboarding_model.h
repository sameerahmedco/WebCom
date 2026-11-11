#pragma once

#include <filesystem>
#include <string>

namespace webcom {

struct OnboardingState {
    bool welcome_seen = false;
    bool operator==(const OnboardingState&) const = default;
};

OnboardingState LoadOnboardingState(const std::filesystem::path& root);
bool SaveOnboardingState(const std::filesystem::path& root, const OnboardingState& state,
                         std::string* error = nullptr);

} // namespace webcom
