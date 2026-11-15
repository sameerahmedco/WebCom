#include "onboarding_model.h"

#include <fstream>
#include <system_error>
#include <utility>

namespace webcom {
namespace {
constexpr const char* kHeader = "WEBCOM_ONBOARDING\t1";

bool Fail(std::string* error, std::string value) {
    if (error) *error = std::move(value);
    return false;
}
}

OnboardingProgress BuildOnboardingProgress(bool default_browser, std::size_t bookmark_count,
                                           std::size_t workspace_count, bool update_checks_enabled) {
    OnboardingProgress progress;
    progress.default_browser = default_browser;
    progress.bookmarks_ready = bookmark_count > 0;
    progress.workspaces_ready = workspace_count > 1;
    progress.updates_ready = update_checks_enabled;
    progress.completed = static_cast<unsigned>(progress.default_browser) +
                         static_cast<unsigned>(progress.bookmarks_ready) +
                         static_cast<unsigned>(progress.workspaces_ready) +
                         static_cast<unsigned>(progress.updates_ready);
    return progress;
}

OnboardingState LoadOnboardingState(const std::filesystem::path& root) {
    std::ifstream input(root / "onboarding.v1", std::ios::binary);
    if (!input) return {};
    std::string header, line, extra;
    if (!std::getline(input, header) || header != kHeader) return {};
    if (!std::getline(input, line)) return {};
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "welcome_seen\t0" && line != "welcome_seen\t1") return {};
    if (std::getline(input, extra)) {
        if (!extra.empty() && extra.back() == '\r') extra.pop_back();
        if (!extra.empty()) return {};
    }
    return {line.back() == '1'};
}

bool SaveOnboardingState(const std::filesystem::path& root, const OnboardingState& state,
                         std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) return Fail(error, "Could not create the WebCom data directory.");
    const auto path = root / "onboarding.v1";
    const auto temp = root / "onboarding.v1.tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) return Fail(error, "Could not write onboarding state.");
        output << kHeader << "\nwelcome_seen\t" << (state.welcome_seen ? '1' : '0') << "\n";
        if (!output) return Fail(error, "Could not finish onboarding state.");
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temp, path, ec);
    }
    if (ec) {
        std::filesystem::remove(temp, ec);
        return Fail(error, "Could not replace onboarding state.");
    }
    return true;
}

} // namespace webcom
