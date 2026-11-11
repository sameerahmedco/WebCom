#include "storage.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#else
// POSIX file operations allow storage regression tests on a development host.
// The WebCom application and DataDirectory() remain Windows-only.
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace webcom {
namespace {

constexpr std::uintmax_t kMaxStateBytes = 4 * 1024 * 1024;
constexpr std::size_t kMaxProfiles = 32;
constexpr std::size_t kMaxTabsPerProfile = 200;
constexpr std::size_t kMaxGroupsPerProfile = 64;
constexpr std::size_t kMaxWorkspacesPerProfile = 32;
constexpr std::size_t kMaxStartupPagesPerProfile = 16;
constexpr std::size_t kMaxUrlBytes = 16384;
constexpr std::size_t kMaxTitleBytes = 4096;
constexpr std::string_view kHeaderV1 = "WEBCOM_STATE\t1";
constexpr std::string_view kHeaderV2 = "WEBCOM_STATE\t2";
constexpr std::string_view kHeaderV3 = "WEBCOM_STATE\t3";
constexpr std::string_view kHeaderV4 = "WEBCOM_STATE\t4";
constexpr std::string_view kHeaderV5 = "WEBCOM_STATE\t5";
constexpr std::string_view kHeaderV6 = "WEBCOM_STATE\t6";
constexpr std::string_view kHeaderV7 = "WEBCOM_STATE\t7";
constexpr std::string_view kNewTabUrl = "webcom:newtab";

// Version 7 is UTF-8 text with LF line endings. All strings are byte-preserving
// percent-escaped with EscapeField. Records have a fixed field count:
// WEBCOM_STATE <TAB> 7 (loader also accepts versions 1 through 6)
// settings <TAB> balanced|saver <TAB> 0|1 <TAB> active-profile
// profile <TAB> profile-name <TAB> active-tab-index <TAB> save-passwords <TAB> general-autofill <TAB> active-workspace <TAB> 0|1(vertical-tabs) <TAB> 0|1(collapsed-rail) <TAB> continue|newtab|pages
// startuppage <TAB> complete-http(s)-url
// workspace <TAB> workspace-name <TAB> color-index(0..7)
// group <TAB> group-name <TAB> color-index(0..7) <TAB> 0|1(collapsed)
// tab <TAB> 0|1(pinned) <TAB> 0|1(keep-awake) <TAB> url <TAB> title <TAB> group-name <TAB> workspace-name
// endprofile
// ... more profiles ...
// end
// No passwords, form data, scrolling position, or navigation stacks are saved.

bool Fail(std::string* error, const std::string& reason) {
    if (error) *error = reason;
    return false;
}

std::string LowerAscii(std::string_view value) {
    std::string result(value);
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return result;
}

AppState DefaultState() {
    AppState state;
    state.profiles.push_back({"Default", {{std::string(kNewTabUrl), "New tab", false, false, ""}}, 0, {},
        true, true, {{"Main",0}}, "Main", false, false, StartupMode::Continue, {}});
    return state;
}

bool IsValidUtf8(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i++]);
        if (lead < 0x80) continue;
        unsigned continuation = 0;
        std::uint32_t codepoint = 0;
        std::uint32_t minimum = 0;
        if (lead >= 0xC2 && lead <= 0xDF) {
            continuation = 1; codepoint = lead & 0x1F; minimum = 0x80;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            continuation = 2; codepoint = lead & 0x0F; minimum = 0x800;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            continuation = 3; codepoint = lead & 0x07; minimum = 0x10000;
        } else {
            return false;
        }
        if (continuation > text.size() - i) return false;
        for (unsigned j = 0; j < continuation; ++j) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xC0) != 0x80) return false;
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return false;
    }
    return true;
}

bool IsValidTitle(std::string_view title) {
    if (title.size() > kMaxTitleBytes || !IsValidUtf8(title)) return false;
    for (unsigned char c : title) {
        if ((c < 0x20 && c != '\t' && c != '\r' && c != '\n') || c == 0x7F)
            return false;
    }
    return true;
}

bool IsRestorableUrl(std::string_view url) {
    constexpr std::array<const char*, 14> internal_pages{
        "webcom:newtab", "webcom:welcome", "webcom:history", "webcom:downloads",
        "webcom:settings", "webcom:shortcuts", "webcom:diagnostics", "webcom:recovery", "webcom:updates", "webcom:extensions",
        "webcom:bookmarks", "webcom:profiles", "webcom:sessions", "webcom:privacy"
    };
    for (const char* page : internal_pages) if (url == page) return true;
    if (url.empty() || url.size() > kMaxUrlBytes || !IsValidUtf8(url)) return false;
    for (unsigned char c : url) {
        if (c <= 0x20 || c == 0x7F || c == '\\') return false;
    }
    if (url.starts_with("file:///")) {
        const auto validated = ResolveAddress(url);
        return IsLocalFileAddress(url) && validated.allowed && validated.url == url;
    }
    // Guard before invoking the omnibox validator: a damaged entry may never
    // become a search query, an inferred hostname, or an operating-system URL.
    if (!url.starts_with("https://") && !url.starts_with("http://")) return false;
    const auto authorityStart = url.find("://") + 3;
    const auto authorityEnd = url.find_first_of("/?#", authorityStart);
    const auto authority = url.substr(authorityStart, authorityEnd - authorityStart);
    // URL user-info may contain an actual password. Do not persist it as plain
    // metadata, even though an explicitly typed HTTP URL could support it.
    if (authority.find('@') != std::string_view::npos) return false;
    const auto validated = ResolveAddress(url);
    return validated.allowed && validated.url == url;
}

bool NormalizeAndValidate(AppState& state, std::string* error) {
    if (state.memory_mode != MemoryMode::Balanced && state.memory_mode != MemoryMode::Saver)
        return Fail(error, "Unknown memory mode.");
    if (state.profiles.empty()) {
        if (LowerAscii(state.active_profile) != "default")
            return Fail(error, "The active profile does not exist.");
        state.profiles = DefaultState().profiles;
    }
    if (state.profiles.size() > kMaxProfiles)
        return Fail(error, "At most 32 profiles can be saved.");
    if (!IsValidProfileName(state.active_profile))
        return Fail(error, "The active profile name is invalid.");
    std::unordered_set<std::string> names;
    bool activeFound = false;
    const auto wanted = LowerAscii(state.active_profile);
    for (auto& profile : state.profiles) {
        if (!IsValidProfileName(profile.name))
            return Fail(error, "A profile name is invalid.");
        const auto normalizedName = LowerAscii(profile.name);
        if (!names.insert(normalizedName).second)
            return Fail(error, "Profile names must be unique ignoring letter case.");
        if (normalizedName == wanted) {
            state.active_profile = profile.name;
            activeFound = true;
        }
        if (profile.tabs.size() > kMaxTabsPerProfile)
            return Fail(error, "At most 200 tabs per profile can be saved.");
        if (profile.tabs.empty()) {
            if (profile.active_tab != 0)
                return Fail(error, "An empty profile has an invalid active tab.");
            profile.tabs.push_back({std::string(kNewTabUrl), "New tab", false, false, ""});
        }
        if (profile.active_tab >= profile.tabs.size())
            return Fail(error, "The active tab index is out of range.");
        std::unordered_set<std::string> workspace_names;
        if (profile.workspaces.empty()) profile.workspaces.push_back({"Main", 0});
        if (profile.workspaces.size() > kMaxWorkspacesPerProfile)
            return Fail(error, "At most 32 workspaces per profile can be saved.");
        for (const auto& workspace : profile.workspaces) {
            if (workspace.name.empty() || workspace.name.size() > 80 || !IsValidTitle(workspace.name))
                return Fail(error, "A workspace name is invalid or exceeds 80 UTF-8 bytes.");
            if (workspace.color > 7)
                return Fail(error, "A workspace color index is invalid.");
            if (!workspace_names.insert(LowerAscii(workspace.name)).second)
                return Fail(error, "Workspace names must be unique ignoring letter case.");
        }
        if (profile.active_workspace.empty()) profile.active_workspace = profile.workspaces.front().name;
        const auto active_workspace_key = LowerAscii(profile.active_workspace);
        const auto active_workspace = std::find_if(profile.workspaces.begin(), profile.workspaces.end(), [&](const SavedWorkspace& workspace) {
            return LowerAscii(workspace.name) == active_workspace_key;
        });
        if (active_workspace == profile.workspaces.end())
            return Fail(error, "The active workspace does not exist.");
        profile.active_workspace = active_workspace->name;
        if (profile.startup_mode != StartupMode::Continue && profile.startup_mode != StartupMode::NewTab &&
            profile.startup_mode != StartupMode::SpecificPages)
            return Fail(error, "Unknown startup mode.");
        if (profile.startup_pages.size() > kMaxStartupPagesPerProfile)
            return Fail(error, "At most 16 startup pages per profile can be saved.");
        std::unordered_set<std::string> startup_urls;
        for (const auto& url : profile.startup_pages) {
            if ((!url.starts_with("https://") && !url.starts_with("http://")) || !IsRestorableUrl(url))
                return Fail(error, "A startup page must be a complete HTTP(S) address without credentials.");
            if (!startup_urls.insert(LowerAscii(url)).second)
                return Fail(error, "Startup pages must be unique ignoring letter case.");
        }

        std::unordered_set<std::string> group_names;
        if (profile.groups.size() > kMaxGroupsPerProfile)
            return Fail(error, "At most 64 tab groups per profile can be saved.");
        for (const auto& group : profile.groups) {
            if (group.name.empty() || group.name.size() > 80 || !IsValidTitle(group.name))
                return Fail(error, "A tab group name is invalid or exceeds 80 UTF-8 bytes.");
            if (group.color > 7)
                return Fail(error, "A tab group color index is invalid.");
            const auto key = LowerAscii(group.name);
            if (!group_names.insert(key).second)
                return Fail(error, "Tab group names must be unique ignoring letter case.");
        }
        for (const auto& tab : profile.tabs) {
            if (!IsRestorableUrl(tab.url))
                return Fail(error, "A tab URL is invalid, too long, or contains credentials. Only complete HTTP(S) addresses and new tabs can be saved.");
            if (!IsValidTitle(tab.title))
                return Fail(error, "A tab title is invalid or exceeds 4096 UTF-8 bytes.");
            if (tab.group.size() > 80 || !IsValidTitle(tab.group))
                return Fail(error, "A tab group name is invalid or exceeds 80 UTF-8 bytes.");
            if (!tab.group.empty() && !group_names.empty() && group_names.find(LowerAscii(tab.group)) == group_names.end())
                return Fail(error, "A tab references a tab group that is not defined.");
            if (tab.workspace.empty() || tab.workspace.size() > 80 || !IsValidTitle(tab.workspace))
                return Fail(error, "A tab workspace name is invalid or exceeds 80 UTF-8 bytes.");
            const auto workspace = std::find_if(profile.workspaces.begin(), profile.workspaces.end(), [&](const SavedWorkspace& item) {
                return LowerAscii(item.name) == LowerAscii(tab.workspace);
            });
            if (workspace == profile.workspaces.end())
                return Fail(error, "A tab references a workspace that is not defined.");
        }
    }
    if (!activeFound) return Fail(error, "The active profile does not exist.");
    const auto active = std::find_if(state.profiles.begin(), state.profiles.end(),
        [&](const SavedProfile& profile) { return LowerAscii(profile.name) == LowerAscii(state.active_profile); });
    if (active != state.profiles.end()) state.save_passwords = active->save_passwords;
    return true;
}

std::vector<std::string_view> Fields(std::string_view line) {
    std::vector<std::string_view> fields;
    while (true) {
        const auto end = line.find('\t');
        fields.push_back(line.substr(0, end));
        // No record permits more than nine fields. Bound allocations even for a
        // corrupted file consisting mostly of tab separators.
        if (end == std::string_view::npos || fields.size() > 9) return fields;
        line.remove_prefix(end + 1);
    }
}

bool Decode(std::string_view encoded, std::size_t limit, std::string& result) {
    if (encoded.size() > limit * 3) return false;
    result = UnescapeField(encoded);
    // This also rejects incomplete escapes, raw structural bytes, and alternate
    // spellings; every saved field has a single canonical representation.
    return result.size() <= limit && EscapeField(result) == encoded && IsValidUtf8(result);
}

bool ParseIndex(std::string_view value, std::size_t& index) {
    if (value.empty() || value.size() > 5) return false;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), index);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool ParseFlag(std::string_view value, bool& flag) {
    if (value == "1") { flag = true; return true; }
    if (value == "0") { flag = false; return true; }
    return false;
}

bool ParseState(std::string_view text, AppState& result) {
    bool header = false; int version = 0;
    bool settings = false;
    bool ended = false;
    SavedProfile* current = nullptr;
    while (!text.empty()) {
        const auto next = text.find('\n');
        auto line = text.substr(0, next);
        if (next == std::string_view::npos) text = {};
        else text.remove_prefix(next + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (ended) return false;
        if (!header) {
            if (line == kHeaderV1) version = 1;
            else if (line == kHeaderV2) version = 2;
            else if (line == kHeaderV3) version = 3;
            else if (line == kHeaderV4) version = 4;
            else if (line == kHeaderV5) version = 5;
            else if (line == kHeaderV6) version = 6;
            else if (line == kHeaderV7) version = 7;
            else return false;
            header = true;
            continue;
        }
        const auto fields = Fields(line);
        if (!settings) {
            if (fields.size() != 4 || fields[0] != "settings") return false;
            if (fields[1] == "balanced") result.memory_mode = MemoryMode::Balanced;
            else if (fields[1] == "saver") result.memory_mode = MemoryMode::Saver;
            else return false;
            if (!ParseFlag(fields[2], result.save_passwords) ||
                !Decode(fields[3], 40, result.active_profile)) return false;
            settings = true;
        } else if (fields[0] == "profile") {
            const std::size_t expected = version >= 7 ? 9 : version >= 6 ? 8 : version >= 5 ? 7 : version >= 4 ? 5 : 3;
            if (current || fields.size() != expected || result.profiles.size() >= kMaxProfiles)
                return false;
            SavedProfile profile;
            if (!Decode(fields[1], 40, profile.name) ||
                !ParseIndex(fields[2], profile.active_tab)) return false;
            if (version >= 4) {
                if (!ParseFlag(fields[3], profile.save_passwords) ||
                    !ParseFlag(fields[4], profile.general_autofill)) return false;
            } else {
                profile.save_passwords = result.save_passwords;
                profile.general_autofill = true;
            }
            if (version >= 5) {
                if (!Decode(fields[5], 80, profile.active_workspace) ||
                    !ParseFlag(fields[6], profile.vertical_tabs)) return false;
                if (version >= 6 && !ParseFlag(fields[7], profile.vertical_tabs_collapsed)) return false;
                if (version >= 7) {
                    if (fields[8] == "continue") profile.startup_mode = StartupMode::Continue;
                    else if (fields[8] == "newtab") profile.startup_mode = StartupMode::NewTab;
                    else if (fields[8] == "pages") profile.startup_mode = StartupMode::SpecificPages;
                    else return false;
                } else profile.startup_mode = StartupMode::Continue;
                profile.workspaces.clear();
            } else {
                profile.active_workspace = "Main";
                profile.vertical_tabs = false;
                profile.vertical_tabs_collapsed = false;
            }
            result.profiles.push_back(std::move(profile));
            current = &result.profiles.back();
        } else if (fields[0] == "startuppage") {
            if (version < 7 || !current || fields.size() != 2 || current->startup_pages.size() >= kMaxStartupPagesPerProfile)
                return false;
            std::string url;
            if (!Decode(fields[1], kMaxUrlBytes, url)) return false;
            current->startup_pages.push_back(std::move(url));
        } else if (fields[0] == "workspace") {
            if (version < 5 || !current || fields.size() != 3 || current->workspaces.size() >= kMaxWorkspacesPerProfile)
                return false;
            SavedWorkspace workspace;
            std::size_t color = 0;
            if (!Decode(fields[1], 80, workspace.name) || workspace.name.empty() ||
                !ParseIndex(fields[2], color) || color > 7) return false;
            workspace.color = static_cast<std::uint8_t>(color);
            current->workspaces.push_back(std::move(workspace));
        } else if (fields[0] == "group") {
            if (version < 3 || !current || fields.size() != 4 || current->groups.size() >= kMaxGroupsPerProfile)
                return false;
            SavedTabGroup group;
            std::size_t color = 0;
            if (!Decode(fields[1], 80, group.name) || group.name.empty() ||
                !ParseIndex(fields[2], color) || color > 7 ||
                !ParseFlag(fields[3], group.collapsed)) return false;
            group.color = static_cast<std::uint8_t>(color);
            current->groups.push_back(std::move(group));
        } else if (fields[0] == "tab") {
            const std::size_t expected = version >= 5 ? 7 : version >= 2 ? 6 : 5;
            if (!current || fields.size() != expected || current->tabs.size() >= kMaxTabsPerProfile)
                return false;
            SavedTab tab;
            if (!ParseFlag(fields[1], tab.pinned) || !ParseFlag(fields[2], tab.keep_awake) ||
                !Decode(fields[3], kMaxUrlBytes, tab.url) ||
                !Decode(fields[4], kMaxTitleBytes, tab.title)) return false;
            if (version >= 2 && !Decode(fields[5], 80, tab.group)) return false;
            if (version >= 5) { if (!Decode(fields[6], 80, tab.workspace)) return false; }
            else tab.workspace = "Main";
            current->tabs.push_back(std::move(tab));
        } else if (fields[0] == "endprofile") {
            if (!current || fields.size() != 1) return false;
            current = nullptr;
        } else if (fields[0] == "end") {
            if (current || fields.size() != 1 || result.profiles.empty()) return false;
            ended = true;
        } else {
            return false;
        }
    }
    return header && settings && ended && NormalizeAndValidate(result, nullptr);
}

bool SerializeState(const AppState& state, std::string& output, std::string* error) {
    output = std::string(kHeaderV7) + "\nsettings\t" +
        (state.memory_mode == MemoryMode::Saver ? "saver" : "balanced") + "\t" +
        (state.save_passwords ? "1" : "0") + "\t" + EscapeField(state.active_profile) + "\n";
    for (const auto& profile : state.profiles) {
        output += "profile\t" + EscapeField(profile.name) + "\t" +
                  std::to_string(profile.active_tab) + "\t" +
                  (profile.save_passwords ? "1" : "0") + "\t" +
                  (profile.general_autofill ? "1" : "0") + "\t" +
                  EscapeField(profile.active_workspace) + "\t" +
                  (profile.vertical_tabs ? "1" : "0") + "\t" +
                  (profile.vertical_tabs_collapsed ? "1" : "0") + "\t" +
                  (profile.startup_mode == StartupMode::NewTab ? "newtab" :
                   profile.startup_mode == StartupMode::SpecificPages ? "pages" : "continue") + "\n";
        for (const auto& page : profile.startup_pages)
            output += "startuppage\t" + EscapeField(page) + "\n";
        for (const auto& workspace : profile.workspaces) {
            output += "workspace\t" + EscapeField(workspace.name) + "\t" +
                      std::to_string(static_cast<unsigned>(workspace.color)) + "\n";
        }
        for (const auto& group : profile.groups) {
            output += "group\t" + EscapeField(group.name) + "\t" +
                      std::to_string(static_cast<unsigned>(group.color)) + "\t" +
                      (group.collapsed ? "1" : "0") + "\n";
        }
        for (const auto& tab : profile.tabs) {
            output += std::string("tab\t") + (tab.pinned ? "1" : "0") + "\t" +
                      (tab.keep_awake ? "1" : "0") + "\t" + EscapeField(tab.url) +
                      "\t" + EscapeField(tab.title) + "\t" + EscapeField(tab.group) +
                      "\t" + EscapeField(tab.workspace) + "\n";
            if (output.size() > kMaxStateBytes)
                return Fail(error, "Session metadata exceeds the 4 MiB limit.");
        }
        output += "endprofile\n";
    }
    output += "end\n";
    return output.size() <= kMaxStateBytes ||
           Fail(error, "Session metadata exceeds the 4 MiB limit.");
}

std::string SystemErrorText(const char* operation, unsigned long code) {
    return std::string(operation) + " (system error " + std::to_string(code) + ").";
}

bool AtomicWriteNamed(const std::filesystem::path& root, std::string_view file_name,
                      std::string_view content, std::string* error) {
    const auto destination = root / std::string(file_name);
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" +
                        std::to_wstring(GetTickCount64()) + L"-" +
                        std::to_wstring(sequence.fetch_add(1));
    const auto temporary = root / (std::wstring(file_name.begin(), file_name.end()) + L".tmp-" + suffix);
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return Fail(error, SystemErrorText("Cannot create session temporary file", GetLastError()));
    DWORD written = 0;
    const BOOL didWrite = WriteFile(file, content.data(), static_cast<DWORD>(content.size()),
                                   &written, nullptr);
    DWORD writeError = didWrite ? ERROR_SUCCESS : GetLastError();
    if (didWrite && written != content.size()) writeError = ERROR_WRITE_FAULT;
    if (writeError == ERROR_SUCCESS && !FlushFileBuffers(file)) writeError = GetLastError();
    if (!CloseHandle(file) && writeError == ERROR_SUCCESS) writeError = GetLastError();
    if (writeError != ERROR_SUCCESS) {
        DeleteFileW(temporary.c_str());
        return Fail(error, SystemErrorText("Cannot write and flush session data", writeError));
    }
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto moveError = GetLastError();
        DeleteFileW(temporary.c_str());
        return Fail(error, SystemErrorText("Cannot replace session file", moveError));
    }
#else
    const auto temporary = root / (std::string(file_name) + ".tmp-" + std::to_string(getpid()) + "-" +
                                    std::to_string(sequence.fetch_add(1)));
    int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, SystemErrorText("Cannot create session temporary file", errno));
    std::size_t offset = 0;
    int writeError = 0;
    while (offset < content.size()) {
        const auto written = write(file, content.data() + offset, content.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { writeError = written == 0 ? EIO : errno; break; }
        offset += static_cast<std::size_t>(written);
    }
    if (writeError == 0 && fsync(file) != 0) writeError = errno;
    if (close(file) != 0 && writeError == 0) writeError = errno;
    if (writeError == 0 && rename(temporary.c_str(), destination.c_str()) != 0) writeError = errno;
    if (writeError != 0) {
        unlink(temporary.c_str());
        return Fail(error, SystemErrorText("Cannot replace session file", writeError));
    }
#endif
    return true;
}

bool ReadValidatedStateFile(const std::filesystem::path& path, AppState& state) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > kMaxStateBytes) return false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) ||
        stream.peek() != std::char_traits<char>::eof() ||
        bytes.find('\0') != std::string::npos) return false;
    AppState parsed;
    if (!ParseState(bytes, parsed)) return false;
    state = std::move(parsed);
    return true;
}

}  // namespace

std::filesystem::path DataDirectory() {
#ifdef _WIN32
    PWSTR folder = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &folder);
    if (FAILED(hr) || !folder || !*folder) {
        CoTaskMemFree(folder);
        throw std::runtime_error("Windows could not resolve the Local AppData folder.");
    }
    const std::filesystem::path root(folder);
    CoTaskMemFree(folder);
    return root / L"WebCom";
#else
    throw std::runtime_error("WebCom's application data directory is only available on Windows.");
#endif
}

AppState LoadState(const std::filesystem::path& root, bool* recovered_from_backup) {
    if (recovered_from_backup) *recovered_from_backup = false;
    try {
        AppState state;
        if (ReadValidatedStateFile(root / "state.v1", state)) return state;
        if (ReadValidatedStateFile(root / "state.v1.bak", state)) {
            if (recovered_from_backup) *recovered_from_backup = true;
            return state;
        }
        return DefaultState();
    } catch (const std::exception&) {
        return DefaultState();
    }
}

bool RemoveProfile(AppState& state, std::string_view profile, std::string* error) {
    if(error)error->clear();
    if(state.profiles.size()<=1)return Fail(error,"WebCom must keep at least one profile.");
    const auto key=LowerAscii(std::string(profile));
    const auto found=std::find_if(state.profiles.begin(),state.profiles.end(),[&](const SavedProfile& item){return LowerAscii(item.name)==key;});
    if(found==state.profiles.end())return Fail(error,"The profile does not exist.");
    const bool active=LowerAscii(state.active_profile)==key;
    state.profiles.erase(found);
    if(active)state.active_profile=state.profiles.front().name;
    return true;
}

void ResetRecoveredSessionTabs(AppState& state) {
    for (auto& profile : state.profiles) {
        if (profile.workspaces.empty()) profile.workspaces.push_back({"Main", 0});
        const auto active = std::find_if(profile.workspaces.begin(), profile.workspaces.end(),
            [&](const SavedWorkspace& workspace) {
                return LowerAscii(workspace.name) == LowerAscii(profile.active_workspace);
            });
        if (active == profile.workspaces.end()) profile.active_workspace = profile.workspaces.front().name;
        profile.tabs.clear();
        profile.groups.clear();
        profile.tabs.push_back({"webcom:newtab", "New tab", false, false, "", profile.active_workspace});
        profile.active_tab = 0;
    }
}

bool SaveState(const std::filesystem::path& root, const AppState& state,
               std::string* error) {
    if (error) error->clear();
    try {
        // Validate before touching the filesystem, including before creating the
        // parent directory. A rejected state never replaces the previous file.
        auto normalized = state;
        if (!NormalizeAndValidate(normalized, error)) return false;
        std::string serialized;
        if (!SerializeState(normalized, serialized, error)) return false;
        if (root.empty() || !root.is_absolute())
            return Fail(error, "The session directory must be an absolute path.");
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        if (ec) return Fail(error, "Cannot create the session directory: " + ec.message());

        // Preserve the previous valid session as a recovery generation before
        // replacing the primary file. A corrupt primary never poisons a good
        // backup because only a state that passes the full parser is copied.
        AppState previous;
        const auto primary = root / "state.v1";
        if (ReadValidatedStateFile(primary, previous)) {
            const auto size = std::filesystem::file_size(primary, ec);
            if (!ec && size > 0 && size <= kMaxStateBytes) {
                std::ifstream in(primary, std::ios::binary);
                std::string bytes(static_cast<std::size_t>(size), '\0');
                if (in && in.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) &&
                    in.peek() == std::char_traits<char>::eof()) {
                    std::string ignored;
                    AtomicWriteNamed(root, "state.v1.bak", bytes, &ignored);
                }
            }
            ec.clear();
        }

        if (!AtomicWriteNamed(root, "state.v1", serialized, error)) return false;

        // Seed the recovery file on the first successful save so even a fresh
        // profile has a known-good generation to fall back to.
        AppState backup;
        if (!ReadValidatedStateFile(root / "state.v1.bak", backup)) {
            std::string ignored;
            AtomicWriteNamed(root, "state.v1.bak", serialized, &ignored);
        }
        return true;
    } catch (const std::exception& ex) {
        return Fail(error, std::string("Cannot save session: ") + ex.what());
    }
}

bool SaveStateAfterProfileDeletion(const std::filesystem::path& root, const AppState& state,
                                   std::string* error) {
    if(!SaveState(root,state,error))return false;
    std::string backup_error;
    if(!SaveState(root,state,&backup_error)){
        std::error_code ec;std::filesystem::remove(root / "state.v1.bak",ec);
    }
    return true;
}

bool SaveStateAfterRecoveryReset(const std::filesystem::path& root, const AppState& state,
                                 std::string* error) {
    if (!SaveState(root, state, error)) return false;
    std::string backup_error;
    if (!SaveState(root, state, &backup_error)) {
        std::error_code ec;
        std::filesystem::remove(root / "state.v1.bak", ec);
    }
    return true;
}

}  // namespace webcom
