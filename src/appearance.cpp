#include "appearance.h"
#include "core.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
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
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace webcom {
namespace {
constexpr std::string_view Header = "WEBCOM_APPEARANCE\t1\n";
constexpr std::size_t MaxBytes = 256;

bool Fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}

bool ValidTheme(BrowserTheme theme) {
    switch (theme) {
    case BrowserTheme::Graphite: case BrowserTheme::Charcoal:
    case BrowserTheme::Midnight: case BrowserTheme::Forest: return true;
    }
    return false;
}

std::string_view ThemeKey(BrowserTheme theme) {
    switch (theme) {
    case BrowserTheme::Graphite: return "graphite";
    case BrowserTheme::Charcoal: return "charcoal";
    case BrowserTheme::Midnight: return "midnight";
    case BrowserTheme::Forest: return "forest";
    }
    return "graphite";
}

std::optional<BrowserTheme> ParseTheme(std::string_view value) {
    for (const auto theme : {BrowserTheme::Graphite, BrowserTheme::Charcoal,
                             BrowserTheme::Midnight, BrowserTheme::Forest})
        if (ThemeKey(theme) == value) return theme;
    return std::nullopt;
}

std::filesystem::path File(const std::filesystem::path& root, std::string_view profile) {
    std::string name(profile);
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return root / "Appearance" / (name + ".v1");
}

double Luminance(std::uint32_t rgb) {
    auto linear = [](unsigned channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear((rgb >> 16) & 255) + 0.7152 * linear((rgb >> 8) & 255) +
           0.0722 * linear(rgb & 255);
}

double Contrast(std::uint32_t first, std::uint32_t second) {
    const double a = Luminance(first), b = Luminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

std::uint32_t Lighten(std::uint32_t rgb, unsigned amount) {
    auto channel = [amount](unsigned original) {
        return (original * (255 - amount) + 255 * amount + 127) / 255;
    };
    return (channel((rgb >> 16) & 255) << 16) | (channel((rgb >> 8) & 255) << 8) |
           channel(rgb & 255);
}

bool AtomicWrite(const std::filesystem::path& destination, const std::string& content,
                 std::string* error) {
    static std::atomic<unsigned long long> sequence{0};
#ifdef _WIN32
    const auto temporary = destination.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) +
        L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "Cannot create the appearance temporary file.");
    DWORD written = 0;
    bool ok = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
              written == content.size();
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    if (!CloseHandle(file)) ok = false;
    if (ok) ok = MoveFileExW(temporary.c_str(), destination.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) {
        DeleteFileW(temporary.c_str());
        return Fail(error, "Cannot write or replace the appearance file.");
    }
#else
    const auto temporary = destination.string() + ".tmp-" + std::to_string(getpid()) +
                           "-" + std::to_string(sequence.fetch_add(1));
    const int file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) return Fail(error, "Cannot create the appearance temporary file.");
    std::size_t offset = 0;
    bool ok = true;
    while (offset < content.size()) {
        const auto count = write(file, content.data() + offset, content.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        offset += static_cast<std::size_t>(count);
    }
    if (ok) ok = fsync(file) == 0;
    if (close(file) != 0) ok = false;
    if (ok) ok = rename(temporary.c_str(), destination.c_str()) == 0;
    if (!ok) {
        unlink(temporary.c_str());
        return Fail(error, "Cannot write or replace the appearance file.");
    }
#endif
    return true;
}
} // namespace

std::string_view ThemeName(BrowserTheme theme) {
    switch (theme) {
    case BrowserTheme::Graphite: return "Graphite";
    case BrowserTheme::Charcoal: return "Charcoal";
    case BrowserTheme::Midnight: return "Midnight";
    case BrowserTheme::Forest: return "Forest";
    }
    return "Graphite";
}

std::optional<std::uint32_t> ParseHexColor(std::string_view text) {
    if (text.size() == 7 && text.front() == '#') text.remove_prefix(1);
    if (text.size() != 6) return std::nullopt;
    std::uint32_t value = 0;
    for (const char c : text) {
        unsigned digit;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
        else return std::nullopt;
        value = (value << 4) | digit;
    }
    return value;
}

std::string FormatHexColor(std::uint32_t rgb) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result(7, '#');
    for (unsigned i = 0; i < 6; ++i) result[i + 1] = digits[(rgb >> (20 - i * 4)) & 15];
    return result;
}

std::uint32_t ProfileColor(const Appearance& appearance, std::string_view profile) {
    if (appearance.accent_rgb && *appearance.accent_rgb <= 0xffffff) return *appearance.accent_rgb;
    constexpr std::array<std::uint32_t, 6> colors{
        0xa8c7fa, 0xf4b6c2, 0x8bd4bf, 0xe8c074, 0xbdb7eb, 0x8bc5da};
    std::uint32_t hash = 2166136261u;
    constexpr std::string_view default_name = "default";
    bool is_default = profile.size() == default_name.size();
    for (std::size_t index = 0; index < profile.size(); ++index) {
        char c = profile[index];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        is_default = is_default && c == default_name[index];
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    }
    return is_default ? colors.front() : colors[1 + hash % (colors.size() - 1)];
}

std::uint32_t ContrastInk(std::uint32_t rgb) {
    return Contrast(rgb, 0xffffff) >= Contrast(rgb, 0) ? 0xffffff : 0;
}

Palette MakePalette(const Appearance& appearance, std::string_view profile) {
    Palette palette{0x18191c, 0x292a2d, 0x202124, 0x3c4043, 0xe8eaed,
                    0xaeb4bc, 0xa8c7fa, 0x303134, 0x414347};
    switch (appearance.theme) {
    case BrowserTheme::Charcoal:
        palette = {0x202020, 0x3a3a3a, 0x282828, 0x4b4d50, 0xe8eaed,
                   0xbdc1c6, 0xa8c7fa, 0x303134, 0x55585d};
        break;
    case BrowserTheme::Midnight:
        palette = {0x141b28, 0x222c3d, 0x192333, 0x33435d, 0xe8eef8,
                   0xafbed3, 0xa8c7fa, 0x2b384b, 0x40516a};
        break;
    case BrowserTheme::Forest:
        palette = {0x17211e, 0x26352f, 0x1e2a25, 0x384c43, 0xe7eee9,
                   0xb2c3b8, 0x8bd4bf, 0x2d3e35, 0x465a4e};
        break;
    case BrowserTheme::Graphite: break;
    }
    const auto selected = ProfileColor(appearance, profile);
    for (unsigned amount = 0; amount <= 255; ++amount) {
        const auto candidate = Lighten(selected, amount);
        bool readable = true;
        for (const auto background : {palette.caption, palette.frame, palette.surface,
                                       palette.hover, palette.search})
            readable = readable && Contrast(candidate, background) >= 3.0;
        if (readable) { palette.accent = candidate; break; }
    }
    return palette;
}

Appearance LoadAppearance(const std::filesystem::path& root, std::string_view profile) {
    if (!IsValidProfileName(profile) || root.empty() || !root.is_absolute()) return {};
    try {
        const auto path = File(root, profile);
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size > MaxBytes) return {};
        std::ifstream stream(path, std::ios::binary);
        std::string text(static_cast<std::size_t>(size), '\0');
        if (!stream || !stream.read(text.data(), static_cast<std::streamsize>(size)) ||
            stream.peek() != std::char_traits<char>::eof()) return {};
        if (!text.starts_with(Header) || !text.ends_with("end\n")) return {};
        std::string_view rows(text);
        rows.remove_prefix(Header.size()); rows.remove_suffix(4);
        auto take = [&rows](std::string_view key) -> std::optional<std::string_view> {
            const auto end = rows.find('\n');
            if (end == std::string_view::npos) return std::nullopt;
            const auto row = rows.substr(0, end);
            rows.remove_prefix(end + 1);
            if (!row.starts_with(key) || row.size() <= key.size() || row[key.size()] != '\t') return std::nullopt;
            return row.substr(key.size() + 1);
        };
        const auto theme_row = take("theme");
        const auto accent_row = take("accent");
        const auto bookmarks_row = take("bookmarks");
        if (!theme_row || !accent_row || !bookmarks_row || !rows.empty()) return {};
        const auto theme = ParseTheme(*theme_row);
        if (!theme || (*bookmarks_row != "1" && *bookmarks_row != "0")) return {};
        Appearance appearance;
        appearance.theme = *theme;
        appearance.show_bookmarks = *bookmarks_row == "1";
        if (*accent_row != "auto") {
            appearance.accent_rgb = ParseHexColor(*accent_row);
            if (!appearance.accent_rgb) return {};
        }
        return appearance;
    } catch (const std::exception&) { return {}; }
}

bool SaveAppearance(const std::filesystem::path& root, std::string_view profile,
                    const Appearance& appearance, std::string* error) {
    if (error) error->clear();
    try {
        if (!IsValidProfileName(profile)) return Fail(error, "Invalid appearance profile name.");
        if (root.empty() || !root.is_absolute()) return Fail(error, "The appearance root must be an absolute path.");
        if (!ValidTheme(appearance.theme)) return Fail(error, "Unknown browser theme.");
        if (appearance.accent_rgb && *appearance.accent_rgb > 0xffffff)
            return Fail(error, "Profile colors must contain exactly 24 RGB bits.");
        std::string content(Header);
        content += "theme\t"; content += ThemeKey(appearance.theme);
        content += "\naccent\t";
        content += appearance.accent_rgb ? FormatHexColor(*appearance.accent_rgb) : "auto";
        content += appearance.show_bookmarks ? "\nbookmarks\t1\nend\n" : "\nbookmarks\t0\nend\n";
        const auto destination = File(root, profile);
        std::error_code ec;
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec) return Fail(error, "Cannot create the appearance folder: " + ec.message());
        return AtomicWrite(destination, content, error);
    } catch (const std::exception& ex) {
        return Fail(error, std::string("Cannot save appearance: ") + ex.what());
    }
}
} // namespace webcom
