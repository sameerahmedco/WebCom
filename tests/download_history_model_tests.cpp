#include "download_history_model.h"

#include <iostream>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
}

int main() {
    using namespace webcom;
    constexpr std::uint64_t day = 24ull * 60ull * 60ull * 1000ull;
    const std::uint64_t now = 60ull * day;
    Check(KeepDownloadHistoryRecord({now - 29 * day, DownloadHistoryState::Completed, false, true}, now), "recent completed kept");
    Check(KeepDownloadHistoryRecord({now - 30 * day, DownloadHistoryState::Completed, false, true}, now), "retention boundary kept");
    Check(!KeepDownloadHistoryRecord({now - 31 * day, DownloadHistoryState::Completed, false, true}, now), "old completed pruned");
    Check(KeepDownloadHistoryRecord({now - 90 * day, DownloadHistoryState::Interrupted, false, true}, now), "old failed kept");
    Check(KeepDownloadHistoryRecord({now + day, DownloadHistoryState::Completed, false, true}, now), "future timestamp kept fail-safe");

    std::vector<DownloadHistoryRecord> records{
        {0, DownloadHistoryState::Interrupted, false, true},
        {0, DownloadHistoryState::Interrupted, true, true},
        {0, DownloadHistoryState::Completed, false, true},
        {0, DownloadHistoryState::Interrupted, false, false},
        {0, DownloadHistoryState::Interrupted, false, true}
    };
    const auto retry = RetryFailedDownloadIndices(records, 5);
    Check(retry.size() == 2, "retry candidates bounded to failed sources");
    Check(retry[0] == 0 && retry[1] == 4, "retry candidate order stable");
    Check(RetryFailedDownloadIndices(records, 1).size() == 1, "retry maximum honored");
    Check(RetryFailedDownloadIndices(std::vector<DownloadHistoryRecord>(20, {0, DownloadHistoryState::Interrupted, false, true}), 99).size() == 8,
          "retry hard cap enforced");

    std::cout << (checks - failures) << " download history checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
