#include "update_automation_model.h"

#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace webcom {
namespace {
constexpr std::string_view kHeader = "WEBCOM_UPDATE_AUTOMATION\t1";

bool Fail(std::string* error, std::string value) {
    if (error) *error = std::move(value);
    return false;
}

bool ParseUnsigned(std::string_view value, std::uint64_t& output) {
    if (value.empty() || value.size() > 20) return false;
    std::uint64_t result = 0;
    for (unsigned char c : value) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) return false;
        result = result * 10u + digit;
    }
    output = result;
    return true;
}
} // namespace

bool BackgroundUpdateCheckDue(const UpdateAutomationState& state,
                              std::uint64_t now_unix_seconds) {
    if (state.last_background_check_unix_seconds == 0) return true;
    if (now_unix_seconds < state.last_background_check_unix_seconds) return true;
    return now_unix_seconds - state.last_background_check_unix_seconds >=
           kBackgroundUpdateCheckIntervalSeconds;
}

bool BackgroundUpdateQuickRetryAllowed(unsigned consecutive_failures) {
    return consecutive_failures > 0 && consecutive_failures < 3;
}

std::uint64_t BackgroundUpdateCheckDelaySeconds(const UpdateAutomationState& state,
                                                std::uint64_t now_unix_seconds,
                                                std::uint64_t retry_remaining_seconds,
                                                bool startup_delay) {
    if (retry_remaining_seconds) {
        if (retry_remaining_seconds < 15) return 15;
        if (retry_remaining_seconds > 300) return 300;
        return retry_remaining_seconds;
    }
    if (BackgroundUpdateCheckDue(state, now_unix_seconds))
        return startup_delay ? kBackgroundUpdateInitialDelaySeconds : 15ull;
    return kBackgroundUpdateCheckIntervalSeconds -
           (now_unix_seconds - state.last_background_check_unix_seconds);
}

void RecordBackgroundUpdateCheck(UpdateAutomationState& state,
                                 std::uint64_t now_unix_seconds) {
    state.last_background_check_unix_seconds = now_unix_seconds;
}

UpdateAutomationState LoadUpdateAutomationState(const std::filesystem::path& root) {
    std::ifstream input(root / "update-automation.v1", std::ios::binary);
    if (!input) return {};
    std::string header, last_check, extra;
    if (!std::getline(input, header) || !std::getline(input, last_check)) return {};
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (!last_check.empty() && last_check.back() == '\r') last_check.pop_back();
    if (header != kHeader || !last_check.starts_with("last_background_check\t")) return {};
    std::uint64_t timestamp = 0;
    if (!ParseUnsigned(std::string_view(last_check).substr(22), timestamp)) return {};
    if (std::getline(input, extra) && !extra.empty() && extra != "\r") return {};
    return {timestamp};
}

bool SaveUpdateAutomationState(const std::filesystem::path& root,
                               const UpdateAutomationState& state,
                               std::string* error) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) return Fail(error, "Could not create WebCom data directory for update automation state.");
    const auto path = root / "update-automation.v1";
    auto temp = path; temp += ".tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) return Fail(error, "Could not write update automation state.");
        output << kHeader << "\nlast_background_check\t"
               << state.last_background_check_unix_seconds << "\n";
        if (!output) return Fail(error, "Could not finish update automation state.");
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec); ec.clear();
        std::filesystem::rename(temp, path, ec);
    }
    if (ec) {
        std::filesystem::remove(temp, ec);
        return Fail(error, "Could not replace update automation state.");
    }
    return true;
}

} // namespace webcom
