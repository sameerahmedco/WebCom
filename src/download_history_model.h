#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace webcom {

enum class DownloadHistoryState { Active, Interrupted, Completed };

struct DownloadHistoryRecord {
    std::uint64_t started_ms = 0;
    DownloadHistoryState state = DownloadHistoryState::Interrupted;
    bool paused = false;
    bool has_source = false;
};

constexpr std::uint64_t kCompletedDownloadRetentionMs = 30ull * 24ull * 60ull * 60ull * 1000ull;

bool KeepDownloadHistoryRecord(const DownloadHistoryRecord& record, std::uint64_t now_ms,
                               std::uint64_t retention_ms = kCompletedDownloadRetentionMs);
std::vector<std::size_t> RetryFailedDownloadIndices(const std::vector<DownloadHistoryRecord>& records,
                                                    std::size_t maximum = 5);

} // namespace webcom
