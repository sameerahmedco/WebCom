#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

struct ExtensionSourceRecord {
    std::string extension_id;
    std::filesystem::path folder;
    // A lightweight developer-change stamp captured after a successful load.
    // It is not a security hash and must never be used to establish trust.
    std::string source_stamp;
    bool operator==(const ExtensionSourceRecord&) const = default;
};

struct ExtensionSourceSnapshot {
    bool available = false;
    std::string stamp;
    std::uint64_t file_count = 0;
    std::uint64_t total_bytes = 0;
    std::string error;
};

std::vector<ExtensionSourceRecord> LoadExtensionSources(const std::filesystem::path& root,
                                                        std::string_view profile);
bool SaveExtensionSources(const std::filesystem::path& root, std::string_view profile,
                          const std::vector<ExtensionSourceRecord>& records,
                          std::string* error = nullptr);
bool RememberExtensionSource(std::vector<ExtensionSourceRecord>& records, std::string_view extension_id,
                             const std::filesystem::path& folder, std::string_view source_stamp = {});
bool ForgetExtensionSource(std::vector<ExtensionSourceRecord>& records, std::string_view extension_id);
const ExtensionSourceRecord* FindExtensionSource(const std::vector<ExtensionSourceRecord>& records,
                                                 std::string_view extension_id);
bool IsValidExtensionId(std::string_view extension_id);
bool IsValidExtensionSourceStamp(std::string_view source_stamp);
std::filesystem::path ExtensionSourcesPath(const std::filesystem::path& root, std::string_view profile);
ExtensionSourceSnapshot SnapshotExtensionSource(const std::filesystem::path& folder);
bool ExtensionSourceChanged(const ExtensionSourceRecord& record, const ExtensionSourceSnapshot& snapshot);

} // namespace webcom
