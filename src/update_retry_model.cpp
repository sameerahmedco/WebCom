#include "update_retry_model.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace webcom {
namespace {
constexpr std::string_view kHeader = "WEBCOM_UPDATE_RETRY\t1";

bool Fail(std::string* error, std::string value) {
    if (error) *error = std::move(value);
    return false;
}

bool ValidCategory(std::string_view value) {
    return !value.empty() && value.size() <= 48 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '.' || c == '_' || c == '-';
           });
}

bool ParseUnsigned(std::string_view value, std::uint64_t& output) {
    if (value.empty() || value.size() > 20) return false;
    std::uint64_t result = 0;
    for (unsigned char c : value) {
        if (!std::isdigit(c)) return false;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) return false;
        result = result * 10u + digit;
    }
    output = result;
    return true;
}
} // namespace

std::uint64_t UpdateRetryDelaySeconds(unsigned failures) {
    if (failures == 0) return 0;
    constexpr std::uint64_t delays[]{15, 30, 60, 120, 300};
    return delays[std::min<std::size_t>(failures - 1, std::size(delays) - 1)];
}

std::uint64_t UpdateRetryRemainingSeconds(const UpdateRetryState& state,
                                          std::uint64_t now_unix_seconds) {
    return state.retry_after_unix_seconds > now_unix_seconds
        ? state.retry_after_unix_seconds - now_unix_seconds : 0;
}

bool RegisterUpdateRetryFailure(UpdateRetryState& state, std::uint64_t now_unix_seconds,
                                std::string_view category, std::string* error) {
    if (!ValidCategory(category)) return Fail(error, "Update retry failure category is invalid.");
    if (state.consecutive_failures < 32u) ++state.consecutive_failures;
    const auto delay = UpdateRetryDelaySeconds(state.consecutive_failures);
    if (now_unix_seconds > std::numeric_limits<std::uint64_t>::max() - delay)
        return Fail(error, "Update retry timestamp overflowed.");
    state.retry_after_unix_seconds = now_unix_seconds + delay;
    state.last_failure_category = std::string(category);
    return true;
}

void ResetUpdateRetry(UpdateRetryState& state) { state = {}; }

UpdateRetryState LoadUpdateRetryState(const std::filesystem::path& root) {
    std::ifstream input(root / "update-retry.v1", std::ios::binary);
    if (!input) return {};
    std::string header, failures, retry_after, category, extra;
    if (!std::getline(input, header) || header != kHeader ||
        !std::getline(input, failures) || !std::getline(input, retry_after) ||
        !std::getline(input, category)) return {};
    for (auto* line : {&failures, &retry_after, &category})
        if (!line->empty() && line->back() == '\r') line->pop_back();
    if (!failures.starts_with("failures\t") || !retry_after.starts_with("retry_after\t") ||
        !category.starts_with("category\t")) return {};
    std::uint64_t failure_count = 0, retry_time = 0;
    if (!ParseUnsigned(std::string_view(failures).substr(9), failure_count) || failure_count > 32 ||
        !ParseUnsigned(std::string_view(retry_after).substr(12), retry_time)) return {};
    const auto category_value = std::string_view(category).substr(9);
    if ((failure_count == 0 && (!category_value.empty() || retry_time != 0)) ||
        (failure_count > 0 && !ValidCategory(category_value))) return {};
    if (std::getline(input, extra) && !extra.empty() && extra != "\r") return {};
    return {static_cast<unsigned>(failure_count), retry_time, std::string(category_value)};
}

bool SaveUpdateRetryState(const std::filesystem::path& root, const UpdateRetryState& state,
                          std::string* error) {
    if (state.consecutive_failures > 32u) return Fail(error, "Update retry failure count is invalid.");
    if (state.consecutive_failures == 0) {
        if (state.retry_after_unix_seconds != 0 || !state.last_failure_category.empty())
            return Fail(error, "Idle update retry state must not retain failure metadata.");
    } else if (!ValidCategory(state.last_failure_category) || state.retry_after_unix_seconds == 0) {
        return Fail(error, "Update retry state is incomplete.");
    }
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) return Fail(error, "Could not create WebCom data directory for update retry state.");
    const auto path = root / "update-retry.v1";
    auto temp = path; temp += ".tmp";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output) return Fail(error, "Could not write update retry state.");
        output << kHeader << "\nfailures\t" << state.consecutive_failures
               << "\nretry_after\t" << state.retry_after_unix_seconds
               << "\ncategory\t" << state.last_failure_category << "\n";
        if (!output) return Fail(error, "Could not finish update retry state.");
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec); ec.clear();
        std::filesystem::rename(temp, path, ec);
    }
    if (ec) { std::filesystem::remove(temp, ec); return Fail(error, "Could not replace update retry state."); }
    return true;
}

} // namespace webcom
