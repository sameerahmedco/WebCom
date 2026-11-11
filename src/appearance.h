#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace webcom {

enum class BrowserTheme { Graphite, Charcoal, Midnight, Forest };

struct Appearance {
    BrowserTheme theme = BrowserTheme::Graphite;
    // nullopt selects the profile's deterministic automatic color. Values are
    // 0xRRGGBB, not Win32 COLORREF. Black is a valid explicit color.
    std::optional<std::uint32_t> accent_rgb;
    bool show_bookmarks = true;
    bool reduce_motion = false;
    bool high_contrast = false;
    // Reader Mode defaults are profile-local and bounded by the native UI.
    int reader_text_scale = 0;   // -2..4
    int reader_width_mode = 1;   // 0 narrow, 1 comfortable, 2 wide
    bool reader_serif = false;
    // Optional user-facing label. The immutable WebView2 profile key remains
    // the SavedProfile::name used for storage and browser data isolation.
    std::string display_name;

    Appearance() = default;
    Appearance(BrowserTheme selected_theme, std::optional<std::uint32_t> selected_accent = {},
               bool bookmarks = true, bool reduced_motion = false, std::string label = {}, bool contrast = false)
        : theme(selected_theme), accent_rgb(selected_accent), show_bookmarks(bookmarks),
          reduce_motion(reduced_motion), high_contrast(contrast), display_name(std::move(label)) {}
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
