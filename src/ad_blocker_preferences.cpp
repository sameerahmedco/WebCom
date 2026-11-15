#include "ad_blocker_preferences.h"
#include "core.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <fstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace webcom {
namespace {
constexpr std::string_view kHeader = "WEBCOM_AD_BLOCKER\t1\n";
constexpr std::string_view kRuleMarker = "\"excludedInitiatorDomains\": [\"webcom.invalid\"]";

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

std::filesystem::path PreferenceFile(const std::filesystem::path& root, std::string_view profile) {
    if (!IsValidProfileName(profile)) return {};
    std::string name(profile);
    for (char& c : name)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return root / "AdBlocker" / (name + ".v1");
}

bool ValidLabel(std::string_view label) {
    if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
    for (const char c : label)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

std::vector<std::string> NormalizeHosts(const std::vector<std::string>& hosts) {
    std::vector<std::string> normalized;
    normalized.reserve(std::min<std::size_t>(hosts.size(), kMaxAdBlockerSiteExceptions));
    for (const auto& raw : hosts) {
        if (normalized.size() >= kMaxAdBlockerSiteExceptions) break;
        auto host = NormalizeAdBlockerHost(raw);
        if (host.empty() || std::find(normalized.begin(), normalized.end(), host) != normalized.end()) continue;
        normalized.push_back(std::move(host));
    }
    return normalized;
}

std::string JsonArray(const std::vector<std::string>& hosts) {
    const auto normalized = NormalizeHosts(hosts);
    std::string result = "[";
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        if (i) result += ", ";
        // Normalization limits entries to lowercase DNS-style characters, so
        // no JSON escaping is required here.
        result += '"' + normalized[i] + '"';
    }
    result += "]";
    return result;
}

bool AtomicWrite(const std::filesystem::path& destination, std::string_view content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) return Fail(error, "Cannot create the Ad Blocker preferences directory.");
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the Ad Blocker preferences temporary file.");
    DWORD written = 0;
    bool ok = content.size() <= MAXDWORD &&
        WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
        written == static_cast<DWORD>(content.size());
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) {
        DeleteFileW(temporary.c_str());
        return Fail(error, "Cannot replace the Ad Blocker preferences file.");
    }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the Ad Blocker preferences temporary file.");
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
        return Fail(error, "Cannot replace the Ad Blocker preferences file.");
    }
#endif
    return true;
}
} // namespace

std::string NormalizeAdBlockerHost(std::string_view value) {
    if (value.empty() || value.size() > 253) return {};
    std::string host(value);
    for (char& c : host) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return {};
    }
    if (host.empty() || host.front() == '.' || host.back() == '.') return {};
    std::size_t start = 0;
    while (start < host.size()) {
        const auto end = host.find('.', start);
        const auto label = std::string_view(host).substr(start, end == std::string::npos ? host.size() - start : end - start);
        if (!ValidLabel(label)) return {};
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return host;
}

std::string AdBlockerHostFromUrl(std::string_view url) {
    std::size_t start = 0;
    if (url.starts_with("https://")) start = 8;
    else if (url.starts_with("http://")) start = 7;
    else return {};
    auto end = url.find_first_of("/?#", start);
    if (end == std::string_view::npos) end = url.size();
    if (end <= start) return {};
    auto authority = url.substr(start, end - start);
    const auto at = authority.rfind('@');
    if (at != std::string_view::npos) authority.remove_prefix(at + 1);
    if (authority.empty() || authority.front() == '[') return {}; // IPv6 is intentionally not persisted yet.
    const auto colon = authority.rfind(':');
    if (colon != std::string_view::npos) authority = authority.substr(0, colon);
    return NormalizeAdBlockerHost(authority);
}

std::vector<std::string> LoadAdBlockerSiteExceptions(const std::filesystem::path& root,
                                                      std::string_view profile) {
    const auto path = PreferenceFile(root, profile);
    if (path.empty()) return {};
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::string line;
    if (!std::getline(in, line) || line != "WEBCOM_AD_BLOCKER\t1") return {};
    std::vector<std::string> result;
    while (result.size() < kMaxAdBlockerSiteExceptions && std::getline(in, line)) {
        if (!line.starts_with("site\t")) continue;
        auto host = NormalizeAdBlockerHost(std::string_view(line).substr(5));
        if (host.empty() || std::find(result.begin(), result.end(), host) != result.end()) continue;
        result.push_back(std::move(host));
    }
    return result;
}

bool SaveAdBlockerSiteExceptions(const std::filesystem::path& root, std::string_view profile,
                                 const std::vector<std::string>& hosts, std::string* error) {
    const auto path = PreferenceFile(root, profile);
    if (path.empty()) return Fail(error, "Invalid profile name for Ad Blocker preferences.");
    std::string content(kHeader);
    for (const auto& host : NormalizeHosts(hosts)) content += "site\t" + host + "\n";
    return AtomicWrite(path, content, error);
}

bool ContainsAdBlockerSiteException(const std::vector<std::string>& hosts, std::string_view host) {
    const auto normalized = NormalizeAdBlockerHost(host);
    if (normalized.empty()) return false;
    return std::find(hosts.begin(), hosts.end(), normalized) != hosts.end();
}

std::optional<std::string> RenderAdBlockerNetworkRules(std::string_view template_json,
                                                        const std::vector<std::string>& hosts) {
    auto allowlist = JsonArray(hosts);
    if (allowlist == "[]") allowlist = "[\"webcom.invalid\"]";
    std::string rendered(template_json);
    std::size_t offset = 0;
    std::size_t replacements = 0;
    while ((offset = rendered.find(kRuleMarker, offset)) != std::string::npos) {
        const auto replacement = std::string("\"excludedInitiatorDomains\": ") + allowlist;
        rendered.replace(offset, kRuleMarker.size(), replacement);
        offset += replacement.size();
        ++replacements;
    }
    if (!replacements) return std::nullopt;
    return rendered;
}

std::string BuildAdBlockerSiteConfigScript(const std::vector<std::string>& hosts) {
    const auto allowlist = JsonArray(hosts);
    return "(() => {\n"
           "  'use strict';\n"
           "  const allowed = new Set(" + allowlist + ");\n"
           "  const host = location.hostname.toLowerCase();\n"
           "  const paused = allowed.has(host);\n"
           "  window.__webcomAdBlockerDisabled = paused;\n"
           "  if (!paused) return;\n"
           "  const mark = () => {\n"
           "    if (!document.documentElement) return false;\n"
           "    document.documentElement.setAttribute('data-webcom-adblock-off', '1');\n"
           "    return true;\n"
           "  };\n"
           "  if (!mark()) {\n"
           "    const observer = new MutationObserver(() => { if (mark()) observer.disconnect(); });\n"
           "    observer.observe(document, { childList: true, subtree: true });\n"
           "  }\n"
           "})();\n";
}

} // namespace webcom
