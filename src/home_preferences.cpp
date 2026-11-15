#include "home_preferences.h"

#include "core.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
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
constexpr std::string_view kHeader = "WEBCOM_HOME_PREFERENCES\t1\n";
constexpr std::size_t kMaxBytes = 128 * 1024;
constexpr std::size_t kMaxTitle = 80;
constexpr std::size_t kMaxUrl = 4096;
constexpr std::size_t kMaxHost = 255;

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::string LowerAscii(std::string value) {
    for (char& c : value)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}

bool SafeField(std::string_view value, std::size_t maximum, bool allow_space = true) {
    if (value.empty() || value.size() > maximum) return false;
    for (unsigned char c : value) {
        if (c == '\t' || c == '\r' || c == '\n' || c == 0 || c == 0x7f) return false;
        if (!allow_space && c == ' ') return false;
    }
    return true;
}

bool ValidShortcut(const HomeShortcut& shortcut) {
    if (!SafeField(shortcut.title, kMaxTitle) || !SafeField(shortcut.url, kMaxUrl)) return false;
    const auto resolved = ResolveAddress(shortcut.url);
    return resolved.allowed && resolved.url == shortcut.url &&
           (shortcut.url.starts_with("https://") || shortcut.url.starts_with("http://"));
}

bool ValidHost(std::string_view host) {
    if (!SafeField(host, kMaxHost, false)) return false;
    if (host.find('/') != std::string_view::npos || host.find(':') != std::string_view::npos ||
        host.find('?') != std::string_view::npos || host.find('#') != std::string_view::npos) return false;
    return std::all_of(host.begin(), host.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-';
    });
}

std::filesystem::path PreferenceFile(const std::filesystem::path& root, std::string_view profile) {
    std::string name = LowerAscii(std::string(profile));
    if (!SafeField(name, 80) || name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
        return {};
    return root / "Home" / (name + ".v1");
}

bool AtomicWrite(const std::filesystem::path& destination, const std::string& content,
                 std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) return Fail(error, "Cannot create the Home preferences directory.");
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                           std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the Home preferences temporary file.");
    DWORD written = 0;
    bool ok = content.size() <= MAXDWORD &&
              WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
              written == content.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) { DeleteFileW(temporary.c_str()); return Fail(error, "Cannot replace the Home preferences file."); }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the Home preferences temporary file.");
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
    if (!ok) { unlink(temporary.c_str()); return Fail(error, "Cannot replace the Home preferences file."); }
#endif
    return true;
}

} // namespace

HomePreferences LoadHomePreferences(const std::filesystem::path& root, std::string_view profile) {
    HomePreferences result;
    const auto path = PreferenceFile(root, profile);
    if (path.empty()) return result;
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > kMaxBytes) return result;
    std::ifstream in(path, std::ios::binary);
    if (!in) return result;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!content.starts_with(kHeader)) return result;
    std::size_t pos = kHeader.size();
    while (pos < content.size()) {
        const auto end = content.find('\n', pos);
        const auto line = content.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = end == std::string::npos ? content.size() : end + 1;
        if (line.empty()) continue;
        const auto first = line.find('\t');
        if (first == std::string::npos) continue;
        const auto type = line.substr(0, first);
        if (type == "S") {
            const auto second = line.find('\t', first + 1);
            if (second == std::string::npos || result.shortcuts.size() >= kMaxHomeShortcuts) continue;
            HomeShortcut shortcut{line.substr(first + 1, second - first - 1), line.substr(second + 1)};
            if (!ValidShortcut(shortcut)) continue;
            const bool duplicate = std::any_of(result.shortcuts.begin(), result.shortcuts.end(), [&](const auto& item) {
                return LowerAscii(item.url) == LowerAscii(shortcut.url);
            });
            if (!duplicate) result.shortcuts.push_back(std::move(shortcut));
        } else if (type == "H" && result.hidden_recent_hosts.size() < kMaxHiddenRecentHosts) {
            auto host = LowerAscii(line.substr(first + 1));
            if (!ValidHost(host)) continue;
            if (std::find(result.hidden_recent_hosts.begin(), result.hidden_recent_hosts.end(), host) == result.hidden_recent_hosts.end())
                result.hidden_recent_hosts.push_back(std::move(host));
        }
    }
    return result;
}

bool SaveHomePreferences(const std::filesystem::path& root, std::string_view profile,
                         const HomePreferences& preferences, std::string* error) {
    const auto path = PreferenceFile(root, profile);
    if (path.empty()) return Fail(error, "Invalid profile name for Home preferences.");
    if (preferences.shortcuts.size() > kMaxHomeShortcuts ||
        preferences.hidden_recent_hosts.size() > kMaxHiddenRecentHosts)
        return Fail(error, "Home preferences exceed the supported bound.");
    std::string content(kHeader);
    for (const auto& shortcut : preferences.shortcuts) {
        if (!ValidShortcut(shortcut)) return Fail(error, "A Home shortcut is invalid.");
        content += "S\t" + shortcut.title + "\t" + shortcut.url + "\n";
    }
    for (auto host : preferences.hidden_recent_hosts) {
        host = LowerAscii(std::move(host));
        if (!ValidHost(host)) return Fail(error, "A hidden recent-site host is invalid.");
        content += "H\t" + host + "\n";
    }
    if (content.size() > kMaxBytes) return Fail(error, "Home preferences are too large.");
    return AtomicWrite(path, content, error);
}

bool AddHomeShortcut(HomePreferences& preferences, HomeShortcut shortcut, std::string* error) {
    if (!ValidShortcut(shortcut)) return Fail(error, "Shortcut title or address is invalid.");
    if (preferences.shortcuts.size() >= kMaxHomeShortcuts) return Fail(error, "The New Tab shortcut limit is eight.");
    const auto url = LowerAscii(shortcut.url);
    if (std::any_of(preferences.shortcuts.begin(), preferences.shortcuts.end(), [&](const auto& item) {
        return LowerAscii(item.url) == url;
    })) return Fail(error, "That shortcut already exists.");
    preferences.shortcuts.push_back(std::move(shortcut));
    return true;
}

bool RemoveHomeShortcut(HomePreferences& preferences, std::size_t index) {
    if (index >= preferences.shortcuts.size()) return false;
    preferences.shortcuts.erase(preferences.shortcuts.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool HideHomeRecentHost(HomePreferences& preferences, std::string host) {
    host = LowerAscii(std::move(host));
    if (!ValidHost(host) || IsHomeRecentHostHidden(preferences, host) ||
        preferences.hidden_recent_hosts.size() >= kMaxHiddenRecentHosts) return false;
    preferences.hidden_recent_hosts.push_back(std::move(host));
    return true;
}

bool ShowHomeRecentHost(HomePreferences& preferences, std::string_view host) {
    const auto target = LowerAscii(std::string(host));
    const auto before = preferences.hidden_recent_hosts.size();
    std::erase(preferences.hidden_recent_hosts, target);
    return preferences.hidden_recent_hosts.size() != before;
}

bool IsHomeRecentHostHidden(const HomePreferences& preferences, std::string_view host) {
    const auto target = LowerAscii(std::string(host));
    return std::find(preferences.hidden_recent_hosts.begin(), preferences.hidden_recent_hosts.end(), target) !=
           preferences.hidden_recent_hosts.end();
}

} // namespace webcom
