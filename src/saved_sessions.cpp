#include "saved_sessions.h"
#include "core.h"

#include <algorithm>
#include <atomic>
#include <cctype>
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
#include <fcntl.h>
#include <unistd.h>
#endif

namespace webcom {
namespace {
constexpr std::string_view Header = "WEBCOM_NAMED_SESSIONS\t1\n";
constexpr std::size_t MaxSessions = 64;
constexpr std::size_t MaxTabs = 200;
constexpr std::size_t MaxBytes = 4 * 1024 * 1024;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

std::string Lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::filesystem::path File(const std::filesystem::path& root, std::string_view profile) {
    std::string name(profile);
    name = Lower(std::move(name));
    return root / "Sessions" / (name + ".v1");
}

bool SafeText(std::string_view value, std::size_t maximum, bool allow_empty = true) {
    if ((!allow_empty && value.empty()) || value.size() > maximum) return false;
    for (unsigned char c : value) if (c < 0x20 || c == 0x7f) return false;
    return true;
}

std::string Escape(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == ' ') out.push_back(static_cast<char>(c));
        else { out.push_back('%'); out.push_back(hex[(c >> 4) & 15]); out.push_back(hex[c & 15]); }
    }
    return out;
}

int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool Unescape(std::string_view value, std::size_t maximum, std::string& out) {
    out.clear();
    if (value.size() > maximum * 3 + 3) return false;
    for (std::size_t i = 0; i < value.size();) {
        if (value[i] == '%') {
            if (i + 2 >= value.size()) return false;
            const int a = Hex(value[i + 1]), b = Hex(value[i + 2]);
            if (a < 0 || b < 0) return false;
            out.push_back(static_cast<char>((a << 4) | b));
            i += 3;
        } else out.push_back(value[i++]);
        if (out.size() > maximum) return false;
    }
    return SafeText(out, maximum);
}

std::vector<std::string_view> Fields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t at = 0;
    while (true) {
        const auto end = line.find('\t', at);
        fields.push_back(line.substr(at, end == std::string_view::npos ? line.size() - at : end - at));
        if (end == std::string_view::npos || fields.size() > 6) break;
        at = end + 1;
    }
    return fields;
}

bool ValidTab(const NamedSessionTab& tab) {
    if (!SafeText(tab.url, 16384, false) || !SafeText(tab.title, 1024) || !SafeText(tab.workspace, 80, false)) return false;
    const auto resolved = ResolveAddress(tab.url);
    return resolved.allowed && resolved.url == tab.url;
}

bool ValidSession(const NamedSession& session) {
    if (!SafeText(session.name, 80, false) || session.tabs.empty() || session.tabs.size() > MaxTabs) return false;
    return std::all_of(session.tabs.begin(), session.tabs.end(), ValidTab);
}

bool AtomicWrite(const std::filesystem::path& destination, const std::string& content, std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                           std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the named-session temporary file.");
    DWORD written = 0;
    bool ok = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) && written == content.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) { DeleteFileW(temporary.c_str()); return Fail(error, "Cannot replace the named-session file."); }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) + "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the named-session temporary file.");
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
    if (!ok) { unlink(temporary.c_str()); return Fail(error, "Cannot replace the named-session file."); }
#endif
    return true;
}
}

std::vector<NamedSession> LoadNamedSessions(const std::filesystem::path& root, std::string_view profile) {
    std::vector<NamedSession> result;
    std::ifstream in(File(root, profile), std::ios::binary);
    if (!in) return result;
    std::ostringstream buffer; buffer << in.rdbuf();
    const std::string data = buffer.str();
    if (data.size() > MaxBytes || !data.starts_with(Header)) return {};
    std::istringstream lines(data.substr(Header.size()));
    std::string line;
    NamedSession* current = nullptr;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto fields = Fields(line);
        if (fields.empty()) return {};
        if (fields[0] == "session") {
            if (current || fields.size() != 3 || result.size() >= MaxSessions) return {};
            NamedSession session;
            if (!Unescape(fields[1], 80, session.name)) return {};
            try { session.created_ms = std::stoull(std::string(fields[2])); } catch (...) { return {}; }
            result.push_back(std::move(session));
            current = &result.back();
        } else if (fields[0] == "tab") {
            if (!current || fields.size() != 5 || current->tabs.size() >= MaxTabs) return {};
            NamedSessionTab tab;
            if ((fields[1] != "0" && fields[1] != "1") ||
                !Unescape(fields[2], 16384, tab.url) || !Unescape(fields[3], 1024, tab.title) ||
                !Unescape(fields[4], 80, tab.workspace)) return {};
            tab.pinned = fields[1] == "1";
            if (!ValidTab(tab)) return {};
            current->tabs.push_back(std::move(tab));
        } else if (fields[0] == "endsession") {
            if (!current || fields.size() != 1 || !ValidSession(*current)) return {};
            current = nullptr;
        } else if (fields[0] == "end") {
            if (current || fields.size() != 1) return {};
            return result;
        } else return {};
    }
    return {};
}

bool SaveNamedSessions(const std::filesystem::path& root, std::string_view profile,
                       const std::vector<NamedSession>& sessions, std::string* error) {
    if (error) error->clear();
    if (sessions.size() > MaxSessions) return Fail(error, "At most 64 named sessions can be saved per profile.");
    std::vector<std::string> names;
    std::ostringstream out; out << Header;
    for (const auto& session : sessions) {
        if (!ValidSession(session)) return Fail(error, "A named session contains invalid data.");
        const auto key = Lower(session.name);
        if (std::find(names.begin(), names.end(), key) != names.end()) return Fail(error, "Named-session names must be unique.");
        names.push_back(key);
        out << "session\t" << Escape(session.name) << '\t' << session.created_ms << '\n';
        for (const auto& tab : session.tabs)
            out << "tab\t" << (tab.pinned ? "1" : "0") << '\t' << Escape(tab.url) << '\t' << Escape(tab.title) << '\t' << Escape(tab.workspace) << '\n';
        out << "endsession\n";
    }
    out << "end\n";
    const auto content = out.str();
    if (content.size() > MaxBytes) return Fail(error, "The named-session file is too large.");
    std::error_code ec; std::filesystem::create_directories(File(root, profile).parent_path(), ec);
    if (ec) return Fail(error, "Cannot create the named-session folder.");
    return AtomicWrite(File(root, profile), content, error);
}

bool UpsertNamedSession(std::vector<NamedSession>& sessions, NamedSession session, std::string* error) {
    if (error) error->clear();
    if (!ValidSession(session)) return Fail(error, "The named session is invalid.");
    const auto key = Lower(session.name);
    auto found = std::find_if(sessions.begin(), sessions.end(), [&](const NamedSession& item){ return Lower(item.name) == key; });
    if (found != sessions.end()) { *found = std::move(session); return true; }
    if (sessions.size() >= MaxSessions) return Fail(error, "At most 64 named sessions can be saved per profile.");
    sessions.insert(sessions.begin(), std::move(session));
    return true;
}

bool DeleteNamedSession(std::vector<NamedSession>& sessions, std::string_view name) {
    const auto key = Lower(std::string(name));
    const auto before = sessions.size();
    std::erase_if(sessions, [&](const NamedSession& item){ return Lower(item.name) == key; });
    return before != sessions.size();
}

} // namespace webcom
