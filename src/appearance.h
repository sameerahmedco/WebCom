#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace webcom {

enum class BrowserTheme { Graphite, Charcoal, Midnight, Forest };

struct Appearance {
    BrowserTheme theme = BrowserTheme::Graphite;
    // nullopt selects the profile's deterministic automatic color. Values are
    // 0xRRGGBB, not Win32 COLORREF. Black is a valid explicit color.
    std::optional<std::uint32_t> accent_rgb;
    bool show_bookmarks = true;
    bool operator==(const Appearance&) const = default;
};

struct Palette {
    // All colors are 0xRRGGBB. Convert to COLORREF only in native UI code.
    std::uint32_t caption, frame, surface, hover, ink, muted, accent, search, border;
};

std::string_view ThemeName(BrowserTheme theme);
std::optional<std::uint32_t> ParseHexColor(std::string_view text);
std::string FormatHexColor(std::uint32_t rgb);
std::uint32_t ProfileColor(const Appearance& appearance, std::string_view profile);
// Choose readable black/white lettering for a custom profile swatch.
std::uint32_t ContrastInk(std::uint32_t rgb);

// The focus/selection accent is lightened when necessary to retain at least
// 3:1 contrast against every theme surface. ProfileColor retains the exact
// chosen swatch so a custom dark color can still be shown with an outline.
Palette MakePalette(const Appearance& appearance, std::string_view profile);

// Appearance is independent of state.v1. Missing, malformed or oversized files
// return defaults; loading never creates directories. Profile names use the
// same validation and case-insensitive identity as the rest of WebCom.
Appearance LoadAppearance(const std::filesystem::path& root, std::string_view profile);

// Validate completely before touching the destination. Saves use an atomic
// replacement; failure preserves an existing valid file. Root must be absolute.
bool SaveAppearance(const std::filesystem::path& root, std::string_view profile,
                    const Appearance& appearance, std::string* error = nullptr);

} // namespace webcom
