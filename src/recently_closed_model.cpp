#include "recently_closed_model.h"

#include "core.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <fstream>
#include <system_error>

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
constexpr std::string_view kHeader = "WEBCOM_RECENTLY_CLOSED\t1\n";
constexpr std::size_t kMaxBytes = 192 * 1024;
constexpr std::size_t kMaxTitle = 4096;
constexpr std::size_t kMaxUrl = 16384;
constexpr std::size_t kMaxWorkspace = 80;
constexpr std::size_t kMaxGroup = 80;

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::filesystem::path File(const std::filesystem::path& root, std::string_view profile) {
    if (!IsValidProfileName(profile)) return {};
    std::string name(profile);
    for (char& c : name) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return root / "Recovery" / (name + ".recently-closed.v1");
}

bool SafeText(std::string_view value, std::size_t limit) {
    if (value.size() > limit) return false;
    for (unsigned char c : value) if (c == 0 || c == '\r' || c == '\n') return false;
    return true;
}

bool ValidUrl(std::string_view value) {
    if (value.empty() || value.size() > kMaxUrl) return false;
    const auto resolved = ResolveAddress(value);
    return resolved.allowed && resolved.url == value;
}

bool ValidEntry(const RecentlyClosedEntry& entry) {
    return ValidUrl(entry.url) && SafeText(entry.title, kMaxTitle) &&
           SafeText(entry.workspace, kMaxWorkspace) && SafeText(entry.group, kMaxGroup);
}

bool ParseU64(std::string_view value, std::uint64_t& out) {
    if (value.empty() || value.size() > 20) return false;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), out);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool ParseFlag(std::string_view value, bool& out) {
    if (value == "1") { out = true; return true; }
    if (value == "0") { out = false; return true; }
    return false;
}

bool AtomicWrite(const std::filesystem::path& destination, std::string_view content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) return Fail(error, "Cannot create the recovery directory.");
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the recently-closed temporary file.");
    DWORD written = 0;
    bool ok = content.size() <= MAXDWORD &&
        WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
        written == static_cast<DWORD>(content.size());
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) { DeleteFileW(temporary.c_str()); return Fail(error, "Cannot replace the recently-closed file."); }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the recently-closed temporary file.");
    std::size_t offset = 0; bool ok = true;
    while (offset < content.size()) {
        const auto n = write(file, content.data() + offset, content.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(n);
    }
    if (ok) ok = fsync(file) == 0;
    if (close(file) != 0) ok = false;
    if (ok) ok = rename(temporary.c_str(), destination.c_str()) == 0;
    if (!ok) { unlink(temporary.c_str()); return Fail(error, "Cannot replace the recently-closed file."); }
#endif
    return true;
}

std::vector<std::string_view> Fields(std::string_view line) {
    std::vector<std::string_view> fields;
    while (true) {
        const auto end = line.find('\t');
        fields.push_back(line.substr(0, end));
        if (end == std::string_view::npos) break;
        line.remove_prefix(end + 1);
        if (fields.size() > 8) break;
    }
    return fields;
}

bool DecodeField(std::string_view encoded, std::size_t limit, std::string& out) {
    if (encoded.size() > limit * 3) return false;
    out = UnescapeField(encoded);
    return out.size() <= limit && EscapeField(out) == encoded && SafeText(out, limit);
}
} // namespace

std::vector<RecentlyClosedEntry> LoadRecentlyClosed(const std::filesystem::path& root,
                                                     std::string_view profile) {
    std::vector<RecentlyClosedEntry> result;
    const auto path = File(root, profile);
    if (path.empty()) return result;
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxBytes) return result;
    std::ifstream in(path, std::ios::binary);
    if (!in) return result;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!content.starts_with(kHeader)) return result;
    std::size_t pos = kHeader.size();
    while (pos < content.size() && result.size() < kMaxRecentlyClosedEntries) {
        const auto end = content.find('\n', pos);
        const auto line = std::string_view(content).substr(pos, end == std::string::npos ? content.size() - pos : end - pos);
        pos = end == std::string::npos ? content.size() : end + 1;
        if (line.empty()) continue;
        const auto f = Fields(line);
        if (f.size() != 8 || f[0] != "E") continue;
        RecentlyClosedEntry entry;
        if (!ParseU64(f[1], entry.closed_at_ms) || !ParseFlag(f[2], entry.pinned) || !ParseFlag(f[3], entry.keep_awake) ||
            !DecodeField(f[4], kMaxUrl, entry.url) || !DecodeField(f[5], kMaxTitle, entry.title) ||
            !DecodeField(f[6], kMaxWorkspace, entry.workspace) || !DecodeField(f[7], kMaxGroup, entry.group) ||
            !ValidEntry(entry)) continue;
        result.push_back(std::move(entry));
    }
    return result;
}

bool SaveRecentlyClosed(const std::filesystem::path& root, std::string_view profile,
                        const std::vector<RecentlyClosedEntry>& entries, std::string* error) {
    const auto path = File(root, profile);
    if (path.empty()) return Fail(error, "Invalid profile name for recently-closed recovery.");
    if (entries.size() > kMaxRecentlyClosedEntries) return Fail(error, "Recently-closed recovery exceeds the supported bound.");
    std::string content(kHeader);
    for (const auto& entry : entries) {
        if (!ValidEntry(entry)) return Fail(error, "A recently-closed entry is invalid.");
        content += "E\t" + std::to_string(entry.closed_at_ms) + "\t" + (entry.pinned ? "1" : "0") + "\t" +
            (entry.keep_awake ? "1" : "0") + "\t" + EscapeField(entry.url) + "\t" + EscapeField(entry.title) + "\t" +
            EscapeField(entry.workspace) + "\t" + EscapeField(entry.group) + "\n";
    }
    if (content.size() > kMaxBytes) return Fail(error, "Recently-closed recovery is too large.");
    return AtomicWrite(path, content, error);
}

bool AddRecentlyClosed(std::vector<RecentlyClosedEntry>& entries, RecentlyClosedEntry entry) {
    if (!ValidEntry(entry)) return false;
    entries.insert(entries.begin(), std::move(entry));
    if (entries.size() > kMaxRecentlyClosedEntries) entries.resize(kMaxRecentlyClosedEntries);
    return true;
}

bool RemoveRecentlyClosed(std::vector<RecentlyClosedEntry>& entries, std::size_t index,
                          RecentlyClosedEntry* removed) {
    if (index >= entries.size()) return false;
    if (removed) *removed = entries[index];
    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

} // namespace webcom
