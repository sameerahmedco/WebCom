#include "extension_sources.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <tuple>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace webcom {
namespace {
constexpr std::string_view kHeaderV1 = "WEBCOM_EXTENSION_SOURCES\t1";
constexpr std::string_view kHeaderV2 = "WEBCOM_EXTENSION_SOURCES\t2";
constexpr std::size_t kMaxBytes = 128u * 1024u;
constexpr std::size_t kMaxRecords = 64;
constexpr std::size_t kMaxSnapshotFiles = 4096;

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::string LowerProfile(std::string_view profile) {
    std::string value(profile);
    for (char& c : value) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}

bool Unreserved(unsigned char c) {
    return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

std::string PercentEncode(std::string_view input) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size() + 16);
    for (unsigned char c : input) {
        if (Unreserved(c)) result.push_back(static_cast<char>(c));
        else {
            result.push_back('%'); result.push_back(digits[c >> 4]); result.push_back(digits[c & 15]);
        }
    }
    return result;
}

int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool PercentDecode(std::string_view input, std::string& output) {
    output.clear(); output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        unsigned char value = static_cast<unsigned char>(input[i]);
        if (input[i] == '%') {
            if (i + 2 >= input.size()) return false;
            const int high = Hex(input[i + 1]), low = Hex(input[i + 2]);
            if (high < 0 || low < 0) return false;
            value = static_cast<unsigned char>((high << 4) | low); i += 2;
        }
        if (value == 0 || value < 0x20 || value == 0x7f) return false;
        output.push_back(static_cast<char>(value));
    }
    return true;
}

std::string PathUtf8(const std::filesystem::path& path) {
#if defined(__cpp_lib_char8_t)
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
#else
    return path.u8string();
#endif
}

std::filesystem::path PathFromUtf8(std::string_view value) {
#if defined(__cpp_lib_char8_t)
    std::u8string converted;
    converted.reserve(value.size());
    for (unsigned char c : value) converted.push_back(static_cast<char8_t>(c));
    return std::filesystem::path(converted);
#else
    return std::filesystem::u8path(value.begin(), value.end());
#endif
}

bool AtomicWrite(const std::filesystem::path& destination, const std::string& content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) +
        L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the extension-source temporary file.");
    DWORD written = 0;
    bool ok = content.size() <= static_cast<std::size_t>(MAXDWORD) &&
              WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
              written == content.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) { DeleteFileW(temporary.c_str()); return Fail(error, "Cannot replace the extension-source file."); }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the extension-source temporary file.");
    std::size_t offset = 0; bool ok = true;
    while (offset < content.size()) {
        const auto count = write(file, content.data() + offset, content.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(count);
    }
    if (ok) ok = fsync(file) == 0;
    if (close(file) != 0) ok = false;
    if (ok) ok = rename(temporary.c_str(), destination.c_str()) == 0;
    if (!ok) { unlink(temporary.c_str()); return Fail(error, "Cannot replace the extension-source file."); }
#endif
    return true;
}
} // namespace

bool IsValidExtensionId(std::string_view extension_id) {
    if (extension_id.empty() || extension_id.size() > 128) return false;
    return std::all_of(extension_id.begin(), extension_id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_';
    });
}

bool IsValidExtensionSourceStamp(std::string_view source_stamp) {
    if (source_stamp.empty()) return true;
    if (source_stamp.size() != 16) return false;
    return std::all_of(source_stamp.begin(), source_stamp.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

std::filesystem::path ExtensionSourcesPath(const std::filesystem::path& root, std::string_view profile) {
    return root / "ExtensionSources" / (LowerProfile(profile) + ".v1");
}

ExtensionSourceSnapshot SnapshotExtensionSource(const std::filesystem::path& folder) {
    ExtensionSourceSnapshot snapshot;
    std::error_code ec;
    if (folder.empty() || !std::filesystem::is_directory(folder, ec) || ec) {
        snapshot.error = "Source folder is missing or unavailable.";
        return snapshot;
    }

    struct EntryStamp {
        std::string relative;
        std::uint64_t bytes = 0;
        long long write_time = 0;
    };
    std::vector<EntryStamp> entries;
    entries.reserve(64);

    std::filesystem::recursive_directory_iterator it(folder, ec), end;
    if (ec) {
        snapshot.error = "Source folder could not be enumerated.";
        return snapshot;
    }
    while (it != end) {
        std::error_code type_error;
        if (it->is_regular_file(type_error)) {
            if (entries.size() >= kMaxSnapshotFiles) {
                snapshot.error = "Source contains more than WebCom's 4096-file change-detection limit.";
                return snapshot;
            }
            const auto relative_path = it->path().lexically_relative(folder);
            const auto relative = PathUtf8(relative_path);
            if (relative.empty() || relative.size() > 32768) {
                snapshot.error = "A source path is invalid or too long.";
                return snapshot;
            }
            const auto size = std::filesystem::file_size(it->path(), ec);
            if (ec) {
                snapshot.error = "A source file size could not be read.";
                return snapshot;
            }
            const auto write = std::filesystem::last_write_time(it->path(), ec);
            if (ec) {
                snapshot.error = "A source file timestamp could not be read.";
                return snapshot;
            }
            const auto ticks = static_cast<long long>(write.time_since_epoch().count());
            entries.push_back({relative, static_cast<std::uint64_t>(size), ticks});
            snapshot.total_bytes += static_cast<std::uint64_t>(size);
        } else if (type_error) {
            snapshot.error = "A source entry could not be inspected.";
            return snapshot;
        }
        it.increment(ec);
        if (ec) {
            snapshot.error = "Source folder enumeration failed.";
            return snapshot;
        }
    }

    std::sort(entries.begin(), entries.end(), [](const EntryStamp& a, const EntryStamp& b) {
        return std::tie(a.relative, a.bytes, a.write_time) < std::tie(b.relative, b.bytes, b.write_time);
    });

    std::uint64_t hash = 1469598103934665603ull;
    const auto feed = [&hash](std::string_view value) {
        for (unsigned char c : value) {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= 1099511628211ull;
        }
        hash ^= 0xffu;
        hash *= 1099511628211ull;
    };
    feed("webcom-extension-source-stamp-v1");
    for (const auto& entry : entries) {
        feed(entry.relative);
        feed(std::to_string(entry.bytes));
        feed(std::to_string(entry.write_time));
    }
    std::ostringstream encoded;
    encoded << std::hex << std::setfill('0') << std::setw(16) << hash;
    snapshot.available = true;
    snapshot.file_count = static_cast<std::uint64_t>(entries.size());
    snapshot.stamp = encoded.str();
    return snapshot;
}

bool ExtensionSourceChanged(const ExtensionSourceRecord& record, const ExtensionSourceSnapshot& snapshot) {
    return !record.source_stamp.empty() && snapshot.available && record.source_stamp != snapshot.stamp;
}

const ExtensionSourceRecord* FindExtensionSource(const std::vector<ExtensionSourceRecord>& records,
                                                 std::string_view extension_id) {
    const auto it = std::find_if(records.begin(), records.end(), [&](const ExtensionSourceRecord& record) {
        return record.extension_id == extension_id;
    });
    return it == records.end() ? nullptr : &*it;
}

bool RememberExtensionSource(std::vector<ExtensionSourceRecord>& records, std::string_view extension_id,
                             const std::filesystem::path& folder, std::string_view source_stamp) {
    if (!IsValidExtensionId(extension_id) || folder.empty() || !IsValidExtensionSourceStamp(source_stamp)) return false;
    std::erase_if(records, [&](const ExtensionSourceRecord& record) {
        return record.folder == folder && record.extension_id != extension_id;
    });
    if (auto* existing = const_cast<ExtensionSourceRecord*>(FindExtensionSource(records, extension_id))) {
        existing->folder = folder;
        existing->source_stamp = std::string(source_stamp);
        return true;
    }
    if (records.size() >= kMaxRecords) return false;
    records.push_back({std::string(extension_id), folder, std::string(source_stamp)});
    return true;
}

bool ForgetExtensionSource(std::vector<ExtensionSourceRecord>& records, std::string_view extension_id) {
    const auto old = records.size();
    std::erase_if(records, [&](const ExtensionSourceRecord& record) { return record.extension_id == extension_id; });
    return records.size() != old;
}

std::vector<ExtensionSourceRecord> LoadExtensionSources(const std::filesystem::path& root,
                                                        std::string_view profile) {
    std::vector<ExtensionSourceRecord> records;
    const auto path = ExtensionSourcesPath(root, profile);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) return records;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes == 0 || bytes > kMaxBytes) return records;
    std::ifstream input(path, std::ios::binary);
    if (!input) return records;
    std::string line;
    if (!std::getline(input, line)) return {};
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const bool schema_v1 = line == kHeaderV1;
    const bool schema_v2 = line == kHeaderV2;
    if (!schema_v1 && !schema_v2) return {};
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto first = line.find('\t');
        const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
        const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
        if (first == std::string::npos || second == std::string::npos || line.substr(0, first) != "source") return {};
        if ((schema_v1 && third != std::string::npos) || (schema_v2 && third == std::string::npos)) return {};
        std::string id, folder, stamp;
        const auto folder_field = schema_v2 ? std::string_view(line).substr(second + 1, third - second - 1)
                                            : std::string_view(line).substr(second + 1);
        if (!PercentDecode(std::string_view(line).substr(first + 1, second - first - 1), id) ||
            !PercentDecode(folder_field, folder) ||
            (schema_v2 && !PercentDecode(std::string_view(line).substr(third + 1), stamp)) ||
            !IsValidExtensionId(id) || !IsValidExtensionSourceStamp(stamp) || folder.size() > 32768) return {};
        if (!RememberExtensionSource(records, id, PathFromUtf8(folder), stamp)) return {};
    }
    return records;
}

bool SaveExtensionSources(const std::filesystem::path& root, std::string_view profile,
                          const std::vector<ExtensionSourceRecord>& records, std::string* error) {
    if (records.size() > kMaxRecords) return Fail(error, "Too many unpacked extension source records.");
    std::string content(kHeaderV2); content.push_back('\n');
    std::vector<std::string> ids;
    for (const auto& record : records) {
        if (!IsValidExtensionId(record.extension_id) || record.folder.empty() || !IsValidExtensionSourceStamp(record.source_stamp))
            return Fail(error, "Invalid extension source record.");
        if (std::find(ids.begin(), ids.end(), record.extension_id) != ids.end()) return Fail(error, "Duplicate extension source ID.");
        ids.push_back(record.extension_id);
        const auto folder = PathUtf8(record.folder);
        if (folder.empty() || folder.size() > 32768) return Fail(error, "Extension source path is invalid or too long.");
        content += "source\t" + PercentEncode(record.extension_id) + "\t" + PercentEncode(folder) + "\t" + PercentEncode(record.source_stamp) + "\n";
        if (content.size() > kMaxBytes) return Fail(error, "Extension source metadata exceeds the size limit.");
    }
    const auto path = ExtensionSourcesPath(root, profile);
    std::error_code ec; std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return Fail(error, "Cannot create the ExtensionSources directory.");
    return AtomicWrite(path, content, error);
}

} // namespace webcom
