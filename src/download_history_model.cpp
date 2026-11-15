#include "download_history_model.h"

#include <algorithm>

namespace webcom {

bool KeepDownloadHistoryRecord(const DownloadHistoryRecord& record, std::uint64_t now_ms,
                               std::uint64_t retention_ms) {
    if (record.state != DownloadHistoryState::Completed) return true;
    if (record.started_ms > now_ms) return true;
    return now_ms - record.started_ms <= retention_ms;
}

std::vector<std::size_t> RetryFailedDownloadIndices(const std::vector<DownloadHistoryRecord>& records,
                                                    std::size_t maximum) {
    maximum = std::min<std::size_t>(maximum, 8);
    std::vector<std::size_t> result;
    result.reserve(std::min(maximum, records.size()));
    for (std::size_t i = 0; i < records.size() && result.size() < maximum; ++i) {
        const auto& record = records[i];
        if (record.state == DownloadHistoryState::Interrupted && !record.paused && record.has_source)
            result.push_back(i);
    }
    return result;
}

} // namespace webcom
