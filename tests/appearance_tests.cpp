#include "appearance.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
std::string Bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
void Write(const std::filesystem::path& path, const std::string& value) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc); stream << value;
}
struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
};
double RelativeLight(std::uint32_t rgb) {
    double result = 0;
    constexpr std::array<double, 3> weights{0.2126, 0.7152, 0.0722};
    for (unsigned component = 0; component < 3; ++component) {
        const auto byte = (rgb >> (16 - component * 8)) & 0xff;
        const double normalized = byte / 255.0;
        result += weights[component] * (normalized <= 0.04045 ? normalized / 12.92 :
                                       std::pow((normalized + 0.055) / 1.055, 2.4));
    }
    return result;
}
double Ratio(std::uint32_t a, std::uint32_t b) {
    const double first = RelativeLight(a) + 0.05, second = RelativeLight(b) + 0.05;
    return std::max(first / second, second / first);
}
} // namespace

int main() {
    using namespace webcom;
    Check(ParseHexColor("#a8C7fa") == 0xa8c7fau, "mixed-case prefixed RGB parsed");
    Check(ParseHexColor("1A2B3C") == 0x1a2b3cu, "unprefixed RGB parsed");
    Check(ParseHexColor("#000000") == 0u, "explicit black parsed");
    Check(ParseHexColor("#FFFFFF") == 0xffffffu, "explicit white parsed");
    Check(FormatHexColor(0x1a2b3c) == "#1A2B3C", "canonical uppercase formatter");
    Check(FormatHexColor(0) == "#000000", "formatter preserves leading zeros");
    for (const auto& bad : std::vector<std::string>{"", "#123", "#12345", "#1234567", "#GGGGGG",
         "0x123456", " #123456", "#123456 ", "#123456\n", std::string("12\0ABC", 6),
         "＃112233", std::string(1000, 'a')})
        Check(!ParseHexColor(bad), "invalid RGB input rejected");

    const Appearance defaults;
    Check(defaults.theme == BrowserTheme::Graphite && !defaults.accent_rgb && defaults.show_bookmarks,
          "default appearance is graphite with automatic accent and visible bookmarks");
    Check(ProfileColor(defaults, "Default") == 0xa8c7fa, "Default profile starts with neutral blue accent");
    Check(ProfileColor(defaults, "WORK") == ProfileColor(defaults, "work"), "automatic color case-insensitive");
    Check(ProfileColor(defaults, "Work") == 0x8bc5da, "automatic Work color remains stable across versions");
    Check(ProfileColor(Appearance{BrowserTheme::Graphite, 0, true}, "Work") == 0,
          "explicit black remains exact profile swatch");
    Check(ProfileColor(Appearance{BrowserTheme::Graphite, 0xffffffff, true}, "Work") == ProfileColor(defaults, "Work"),
          "out-of-range UI input falls back to automatic profile color");
    Check(ContrastInk(0) == 0xffffff && ContrastInk(0xffffff) == 0,
          "profile label ink adapts to black and white backgrounds");

    constexpr std::array<std::uint32_t, 10> colors{0x000000, 0xffffff, 0xff0000, 0x00ff00, 0x0000ff,
        0x808080, 0x123456, 0xabcdef, 0xa8c7fa, 0x4b4d50};
    for (const auto theme : {BrowserTheme::Graphite, BrowserTheme::Charcoal, BrowserTheme::Midnight, BrowserTheme::Forest}) {
        Check(!ThemeName(theme).empty(), "known theme has display name");
        const auto base = MakePalette(Appearance{theme, {}, true}, "Default");
        Check(RelativeLight(base.caption) < 0.05 && RelativeLight(base.frame) < 0.1,
              "all theme presets keep dark surfaces");
        Check(Ratio(base.ink, base.frame) >= 4.5 && Ratio(base.muted, base.frame) >= 4.5,
              "normal and secondary text remain readable on frame");
        for (const auto color : colors) {
            const auto palette = MakePalette(Appearance{theme, color, true}, "Work");
            bool readable = true;
            for (const auto surface : {palette.caption, palette.frame, palette.surface, palette.hover, palette.search})
                readable = readable && Ratio(palette.accent, surface) >= 3.0;
            Check(readable && Ratio(ContrastInk(color), color) >= 4.5,
                  "custom color retains readable focus accent and swatch label");
        }
    }
    const auto invalid_theme = MakePalette(Appearance{static_cast<BrowserTheme>(99), {}, true}, "Default");
    Check(invalid_theme.frame == MakePalette(defaults, "Default").frame, "invalid runtime theme uses safe graphite palette");

    const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = std::filesystem::temp_directory_path() / ("webcom-appearance-tests-" + stamp);
    if (!std::filesystem::create_directory(root)) { std::cerr << "Cannot create test directory\n"; return 1; }
    Cleanup cleanup{root};
    const auto path = root / "Appearance" / "default.v1";
    std::string error;
    Check(LoadAppearance(root, "Default") == defaults, "missing appearance uses defaults");
    Check(!std::filesystem::exists(root / "Appearance"), "load does not create directories");
    const Appearance saved{BrowserTheme::Forest, 0x257fc2, false};
    Check(SaveAppearance(root, "Default", saved, &error), "save per-profile appearance");
    Check(error.empty(), "successful save clears error");
    Check(LoadAppearance(root, "DEFAULT") == saved, "all fields roundtrip with case-insensitive profile identity");
    Check(LoadAppearance(root, "Work") == defaults, "another profile keeps its own preferences");
    const auto previous = Bytes(path);
    for (const std::string name : {"../escape", "CON", "LPT1", "..", " name", "name "}) {
        Check(!SaveAppearance(root, name, saved, &error) && !error.empty(), "unsafe profile name rejected on save");
        Check(LoadAppearance(root, name) == defaults, "unsafe profile name rejected on load");
    }
    Check(!SaveAppearance("relative", "Default", saved, &error), "relative save root rejected");
    Check(!SaveAppearance({}, "Default", saved, &error), "empty save root rejected");
    Check(LoadAppearance("relative", "Default") == defaults, "relative load root ignored");
    Check(!SaveAppearance(root, "Default", Appearance{static_cast<BrowserTheme>(99), {}, true}, &error),
          "invalid theme cannot replace saved preferences");
    Check(!SaveAppearance(root, "Default", Appearance{BrowserTheme::Graphite, 0x1000000, true}, &error),
          "out-of-range RGB cannot replace saved preferences");
    Check(Bytes(path) == previous, "rejected saves preserve existing bytes");
    Check(SaveAppearance(root, "Default", Appearance{BrowserTheme::Charcoal, 0, true}, &error) &&
          LoadAppearance(root, "Default").accent_rgb == 0u, "explicit black persists independently of automatic color");
    Check(SaveAppearance(root, "Default", defaults, &error) && LoadAppearance(root, "Default") == defaults,
          "reset-to-default appearance roundtrips");
    for (const auto theme : {BrowserTheme::Graphite, BrowserTheme::Charcoal, BrowserTheme::Midnight, BrowserTheme::Forest}) {
        const Appearance value{theme, 0xe8c074, false};
        Check(SaveAppearance(root, "Default", value, &error) && LoadAppearance(root, "Default") == value,
              "every known theme roundtrips");
    }

    constexpr std::string_view prefix = "WEBCOM_APPEARANCE\t1\n";
    for (const auto& corrupt : std::vector<std::string>{
        "", std::string(prefix), "WEBCOM_APPEARANCE\t99\ntheme\tforest\naccent\tauto\nbookmarks\t1\nend\n",
        std::string(prefix) + "theme\tunknown\naccent\tauto\nbookmarks\t1\nend\n",
        std::string(prefix) + "theme\tforest\naccent\t#bad\nbookmarks\t1\nend\n",
        std::string(prefix) + "theme\tforest\naccent\tauto\nbookmarks\t2\nend\n",
        std::string(prefix) + "theme\tforest\naccent\tauto\nend\n",
        std::string(prefix) + "theme\tforest\ntheme\tgraphite\naccent\tauto\nbookmarks\t1\nend\n",
        previous + "garbage\n", previous.substr(0, previous.size() - 1), std::string(257, 'x')}) {
        Write(path, corrupt);
        Check(LoadAppearance(root, "Default") == defaults, "corrupt or oversized preferences fail closed to defaults");
    }
    Check(SaveAppearance(root, "Default", saved, &error), "restore valid appearance after corrupt file");
    const auto blocked = root / "blocked";
    Write(blocked, "not a folder");
    Check(!SaveAppearance(blocked, "Default", saved, &error) && !error.empty(), "folder creation failure reported");
    const auto occupied = root / "Appearance" / "occupied.v1";
    std::filesystem::create_directory(occupied);
    Write(occupied / "keep.txt", "preserve me");
    Check(!SaveAppearance(root, "Occupied", saved, &error) && !error.empty(), "failed atomic replacement reported");
    Check(Bytes(occupied / "keep.txt") == "preserve me", "failed replacement preserves existing destination");
    Check(LoadAppearance(root, "Default") == saved, "save failure does not damage another profile");
    bool temporary_left = false;
    for (const auto& entry : std::filesystem::directory_iterator(root / "Appearance"))
        if (entry.path().filename().string().find(".tmp-") != std::string::npos) temporary_left = true;
    Check(!temporary_left, "temporary files cleaned after success and replacement failure");
    std::cout << checks << " appearance checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
