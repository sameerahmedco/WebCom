#include "site_permissions.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace webcom {
namespace {
constexpr std::string_view Header = "WEBCOM_SITE_PERMISSIONS\t1\n";
constexpr std::size_t MaxBytes = 512 * 1024;
constexpr std::size_t MaxRules = 512;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

std::filesystem::path File(const std::filesystem::path& root, std::string_view profile) {
    std::string name(profile);
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return root / "Permissions" / (name + ".v1");
}

std::string LowerAscii(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool SafeField(std::string_view value) {
    if (value.empty() || value.size() > 2048) return false;
    for (unsigned char c : value) if (c < 0x20 || c == 0x7f || c == '\t') return false;
    return true;
}

std::string KindToken(SitePermissionKind kind) {
    switch (kind) {
    case SitePermissionKind::Camera: return "camera";
    case SitePermissionKind::Microphone: return "microphone";
    case SitePermissionKind::Geolocation: return "location";
    case SitePermissionKind::Notifications: return "notifications";
    }
    return "camera";
}

std::string DecisionToken(SitePermissionDecision decision) {
    switch (decision) {
    case SitePermissionDecision::Ask: return "ask";
    case SitePermissionDecision::Allow: return "allow";
    case SitePermissionDecision::Block: return "block";
    }
    return "ask";
}

bool ParseKind(std::string_view token, SitePermissionKind* kind) {
    if (token == "camera") *kind = SitePermissionKind::Camera;
    else if (token == "microphone") *kind = SitePermissionKind::Microphone;
    else if (token == "location") *kind = SitePermissionKind::Geolocation;
    else if (token == "notifications") *kind = SitePermissionKind::Notifications;
    else return false;
    return true;
}

bool ParseDecision(std::string_view token, SitePermissionDecision* decision) {
    if (token == "ask") *decision = SitePermissionDecision::Ask;
    else if (token == "allow") *decision = SitePermissionDecision::Allow;
    else if (token == "block") *decision = SitePermissionDecision::Block;
    else return false;
    return true;
}

bool AtomicWrite(const std::filesystem::path& destination, const std::string& content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                           std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the site-permission temporary file.");
    DWORD written = 0;
    bool ok = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) && written == content.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) { DeleteFileW(temporary.c_str()); return Fail(error, "Cannot replace the site-permission file."); }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the site-permission temporary file.");
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
    if (!ok) { unlink(temporary.c_str()); return Fail(error, "Cannot replace the site-permission file."); }
#endif
    return true;
}
} // namespace

std::string SiteOrigin(std::string_view url) {
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string_view::npos) return {};
    std::string scheme = LowerAscii(std::string(url.substr(0, scheme_end)));
    if (scheme != "http" && scheme != "https") return {};
    const auto authority_start = scheme_end + 3;
    auto authority_end = url.find_first_of("/?#", authority_start);
    if (authority_end == std::string_view::npos) authority_end = url.size();
    if (authority_end <= authority_start) return {};
    std::string authority(url.substr(authority_start, authority_end - authority_start));
    const auto at = authority.rfind('@');
    if (at != std::string::npos) authority.erase(0, at + 1);
    if (authority.empty() || authority.size() > 1024) return {};
    authority = LowerAscii(authority);
    if (authority.find_first_of("\t\r\n ") != std::string::npos) return {};
    if (scheme == "https" && authority.ends_with(":443")) authority.resize(authority.size() - 4);
    if (scheme == "http" && authority.ends_with(":80")) authority.resize(authority.size() - 3);
    if (authority.empty()) return {};
    return scheme + "://" + authority;
}

std::string SitePermissionKindName(SitePermissionKind kind) {
    switch (kind) {
    case SitePermissionKind::Camera: return "Camera";
    case SitePermissionKind::Microphone: return "Microphone";
    case SitePermissionKind::Geolocation: return "Location";
    case SitePermissionKind::Notifications: return "Notifications";
    }
    return "Permission";
}

std::string SitePermissionDecisionName(SitePermissionDecision decision) {
    switch (decision) {
    case SitePermissionDecision::Ask: return "Ask";
    case SitePermissionDecision::Allow: return "Allow";
    case SitePermissionDecision::Block: return "Block";
    }
    return "Ask";
}

SitePermissionDecision NextSitePermissionDecision(SitePermissionDecision decision) {
    switch (decision) {
    case SitePermissionDecision::Ask: return SitePermissionDecision::Allow;
    case SitePermissionDecision::Allow: return SitePermissionDecision::Block;
    case SitePermissionDecision::Block: return SitePermissionDecision::Ask;
    }
    return SitePermissionDecision::Ask;
}

SitePermissionDecision SitePermissionFor(const std::vector<SitePermissionRule>& rules,
                                         std::string_view origin,
                                         SitePermissionKind kind) {
    const auto normalized = SiteOrigin(origin);
    if (normalized.empty()) return SitePermissionDecision::Ask;
    const auto found = std::find_if(rules.rbegin(), rules.rend(), [&](const SitePermissionRule& rule) {
        return rule.origin == normalized && rule.kind == kind;
    });
    return found == rules.rend() ? SitePermissionDecision::Ask : found->decision;
}

void SetSitePermission(std::vector<SitePermissionRule>& rules,
                       std::string origin,
                       SitePermissionKind kind,
                       SitePermissionDecision decision) {
    origin = SiteOrigin(origin);
    if (origin.empty()) return;
    std::erase_if(rules, [&](const SitePermissionRule& rule) { return rule.origin == origin && rule.kind == kind; });
    if (decision != SitePermissionDecision::Ask) rules.push_back({std::move(origin), kind, decision});
    if (rules.size() > MaxRules) rules.erase(rules.begin(), rules.begin() + static_cast<std::ptrdiff_t>(rules.size() - MaxRules));
}

void ResetSitePermissions(std::vector<SitePermissionRule>& rules, std::string_view origin) {
    const auto normalized = SiteOrigin(origin);
    if (normalized.empty()) return;
    std::erase_if(rules, [&](const SitePermissionRule& rule) { return rule.origin == normalized; });
}

std::vector<SitePermissionRule> LoadSitePermissions(const std::filesystem::path& root,
                                                    std::string_view profile) {
    std::vector<SitePermissionRule> result;
    std::ifstream in(File(root, profile), std::ios::binary);
    if (!in) return result;
    std::ostringstream buffer; buffer << in.rdbuf();
    const std::string data = buffer.str();
    if (data.size() > MaxBytes || !data.starts_with(Header)) return {};
    std::istringstream lines(data.substr(Header.size()));
    std::string line;
    while (std::getline(lines, line) && result.size() < MaxRules) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto first = line.find('\t');
        const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
        if (first == std::string::npos || second == std::string::npos || line.find('\t', second + 1) != std::string::npos) continue;
        const std::string origin = line.substr(0, first);
        const auto normalized = SiteOrigin(origin);
        SitePermissionKind kind{}; SitePermissionDecision decision{};
        if (normalized.empty() || normalized != origin || !SafeField(origin) ||
            !ParseKind(std::string_view(line).substr(first + 1, second - first - 1), &kind) ||
            !ParseDecision(std::string_view(line).substr(second + 1), &decision) ||
            decision == SitePermissionDecision::Ask) continue;
        SetSitePermission(result, origin, kind, decision);
    }
    return result;
}

bool SaveSitePermissions(const std::filesystem::path& root,
                         std::string_view profile,
                         const std::vector<SitePermissionRule>& rules,
                         std::string* error) {
    std::error_code ec;
    const auto path = File(root, profile);
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return Fail(error, "Cannot create the site-permission folder.");
    std::ostringstream out; out << Header;
    std::size_t count = 0;
    for (const auto& rule : rules) {
        if (count++ >= MaxRules) break;
        const auto normalized = SiteOrigin(rule.origin);
        if (normalized.empty() || normalized != rule.origin || !SafeField(rule.origin) || rule.decision == SitePermissionDecision::Ask) continue;
        out << normalized << '\t' << KindToken(rule.kind) << '\t' << DecisionToken(rule.decision) << '\n';
    }
    return AtomicWrite(path, out.str(), error);
}

} // namespace webcom
