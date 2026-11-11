#pragma once

#include <string>

namespace webcom {

struct ReliabilityReportSnapshot {
    std::string version;
    bool previous_session_unclean = false;
    std::string update_phase;
    std::string update_target;
    std::string update_previous;
    unsigned failed_health_starts = 0;
    bool rollback_recommended = false;
    std::string recovery_version;
    unsigned update_retry_failures = 0;
    unsigned update_retry_remaining_seconds = 0;
    std::string update_retry_category;
};

std::string BuildReliabilityReportText(const ReliabilityReportSnapshot& snapshot);
std::string ReliabilityReportPrivacyText();

} // namespace webcom
