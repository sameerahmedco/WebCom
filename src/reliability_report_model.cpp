#include "reliability_report_model.h"

#include <sstream>

namespace webcom {

std::string BuildReliabilityReportText(const ReliabilityReportSnapshot& s) {
    std::ostringstream out;
    out << "WebCom recovery and reliability report\r\n"
        << "Version: " << s.version << "\r\n"
        << "Previous session unclean: " << (s.previous_session_unclean ? "yes" : "no") << "\r\n"
        << "Update transaction: " << (s.update_phase.empty() ? "none" : s.update_phase) << "\r\n";
    if (!s.update_target.empty()) out << "Update target: " << s.update_target << "\r\n";
    if (!s.update_previous.empty()) out << "Previous version: " << s.update_previous << "\r\n";
    out << "Failed health starts: " << s.failed_health_starts << "\r\n"
        << "Rollback recommended: " << (s.rollback_recommended ? "yes" : "no") << "\r\n"
        << "Recovery installer: " << (s.recovery_version.empty() ? "not available" : s.recovery_version) << "\r\n"
        << "Update retry failures: " << s.update_retry_failures << "\r\n"
        << "Update retry remaining seconds: " << s.update_retry_remaining_seconds << "\r\n";
    if (!s.update_retry_category.empty()) out << "Last update failure category: " << s.update_retry_category << "\r\n";
    return out.str();
}

std::string ReliabilityReportPrivacyText() {
    return "This recovery report is created only when you explicitly export it.\r\n"
           "It is not uploaded automatically. The runtime log is bounded and redacts HTTP, HTTPS and file URLs.\r\n"
           "The report intentionally excludes cookie values, saved passwords, form contents and authentication data.\r\n"
           "Review every file before sharing it with support.\r\n";
}

} // namespace webcom
