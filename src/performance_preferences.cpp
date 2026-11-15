#include "performance_preferences.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <system_error>
#include <utility>

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
constexpr std::string_view kHeader = "WEBCOM_PERFORMANCE\t1\n";

std::filesystem::path File(const std::filesystem::path& root, std::string_view profile) {
    if (!IsValidProfileName(profile)) return {};
    std::string name(profile);
    for (char& c : name)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return root / "Performance" / (name + ".v1");
}

std::string Token(SleepTimeout timeout) {
    switch (timeout) {
    case SleepTimeout::OneMinute: return "1m";
    case SleepTimeout::FiveMinutes: return "5m";
    case SleepTimeout::FifteenMinutes: return "15m";
    case SleepTimeout::ThirtyMinutes: return "30m";
    case SleepTimeout::Never: return "never";
    }
    return "5m";
}

std::optional<SleepTimeout> ParseTimeout(std::string_view value) {
    if (value == "1m") return SleepTimeout::OneMinute;
    if (value == "5m") return SleepTimeout::FiveMinutes;
    if (value == "15m") return SleepTimeout::FifteenMinutes;
    if (value == "30m") return SleepTimeout::ThirtyMinutes;
    if (value == "never") return SleepTimeout::Never;
    return std::nullopt;
}

struct Data {
    std::optional<SleepTimeout> timeout;
    std::vector<std::string> never_sleep_hosts;
};

bool IsHostLabelValid(std::string_view label) {
    if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
    for (const char c : label)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

std::string NormalizeHostInternal(std::string_view value) {
    if (value.empty() || value.size() > 253) return {};
    std::string host(value);
    for (char& c : host) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return {};
    }
    if (host.starts_with("www.")) host.erase(0, 4);
    if (host.empty() || host.front() == '.' || host.back() == '.') return {};
    std::size_t start = 0;
    while (start < host.size()) {
        const auto end = host.find('.', start);
        const auto label = std::string_view(host).substr(start, end == std::string::npos ? host.size() - start : end - start);
        if (!IsHostLabelValid(label)) return {};
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return host;
}

Data ReadData(const std::filesystem::path& path) {
    Data data;
    std::ifstream in(path, std::ios::binary);
    if (!in) return data;
    std::string line;
    if (!std::getline(in, line) || line != "WEBCOM_PERFORMANCE\t1") return {};
    while (std::getline(in, line)) {
        if (line.starts_with("sleep\t")) {
            if (!data.timeout) data.timeout = ParseTimeout(std::string_view(line).substr(6));
            continue;
        }
        if (line.starts_with("never\t") && data.never_sleep_hosts.size() < kMaxNeverSleepHosts) {
            auto host = NormalizeHostInternal(std::string_view(line).substr(6));
            if (host.empty()) continue;
            if (std::find(data.never_sleep_hosts.begin(), data.never_sleep_hosts.end(), host) == data.never_sleep_hosts.end())
                data.never_sleep_hosts.push_back(std::move(host));
        }
    }
    return data;
}

std::string SerializeData(SleepTimeout timeout, const std::vector<std::string>& hosts) {
    std::string content = std::string(kHeader) + "sleep\t" + Token(timeout) + "\n";
    std::vector<std::string> emitted;
    emitted.reserve(std::min<std::size_t>(hosts.size(), kMaxNeverSleepHosts));
    for (const auto& raw : hosts) {
        if (emitted.size() >= kMaxNeverSleepHosts) break;
        const auto host = NormalizeHostInternal(raw);
        if (host.empty() || std::find(emitted.begin(), emitted.end(), host) != emitted.end()) continue;
        content += "never\t" + host + "\n";
        emitted.push_back(host);
    }
    return content;
}

bool Fail(std::string* error, std::string message) { if (error) *error = std::move(message); return false; }

bool AtomicWrite(const std::filesystem::path& destination, std::string_view content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) return Fail(error, "Cannot create the Performance preferences directory.");
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the Performance preferences temporary file.");
    DWORD written = 0;
    bool ok = content.size() <= MAXDWORD &&
        WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
        written == static_cast<DWORD>(content.size());
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) {
        DeleteFileW(temporary.c_str());
        return Fail(error, "Cannot replace the Performance preferences file.");
    }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the Performance preferences temporary file.");
    std::size_t offset = 0;
    bool ok = true;
    while (offset < content.size()) {
        const auto written = write(file, content.data() + offset, content.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(written);
    }
    if (ok) ok = fsync(file) == 0;
    if (close(file) != 0) ok = false;
    if (ok) ok = rename(temporary.c_str(), destination.c_str()) == 0;
    if (!ok) {
        unlink(temporary.c_str());
        return Fail(error, "Cannot replace the Performance preferences file.");
    }
#endif
    return true;
}
} // namespace

std::optional<SleepTimeout> LoadSleepTimeoutPreference(const std::filesystem::path& root,
                                                       std::string_view profile) {
    const auto path = File(root, profile);
    if (path.empty()) return std::nullopt;
    return ReadData(path).timeout;
}

bool SaveSleepTimeoutPreference(const std::filesystem::path& root, std::string_view profile,
                                SleepTimeout timeout, std::string* error) {
    const auto path = File(root, profile);
    if (path.empty()) return Fail(error, "Invalid profile name for Performance preferences.");
    const auto existing = ReadData(path);
    return AtomicWrite(path, SerializeData(timeout, existing.never_sleep_hosts), error);
}

std::string NormalizeNeverSleepHost(std::string_view host) {
    return NormalizeHostInternal(host);
}

std::vector<std::string> LoadNeverSleepHostsPreference(const std::filesystem::path& root,
                                                       std::string_view profile) {
    const auto path = File(root, profile);
    if (path.empty()) return {};
    return ReadData(path).never_sleep_hosts;
}

bool SaveNeverSleepHostsPreference(const std::filesystem::path& root, std::string_view profile,
                                   SleepTimeout timeout, const std::vector<std::string>& hosts,
                                   std::string* error) {
    const auto path = File(root, profile);
    if (path.empty()) return Fail(error, "Invalid profile name for Performance preferences.");
    return AtomicWrite(path, SerializeData(timeout, hosts), error);
}

bool ContainsNeverSleepHost(const std::vector<std::string>& hosts, std::string_view host) {
    const auto normalized = NormalizeHostInternal(host);
    if (normalized.empty()) return false;
    return std::find(hosts.begin(), hosts.end(), normalized) != hosts.end();
}

} // namespace webcom
