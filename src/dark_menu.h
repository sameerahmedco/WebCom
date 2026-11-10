#pragma once

#include <windows.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace webcom {

struct Palette;

// A caller supplies real actions only. An empty, separator-marked item draws
// a section divider; id == 0 is informational and cannot be activated.
struct MenuItem {
    UINT id = 0;
    std::wstring text;
    std::wstring shortcut;
    std::wstring icon;
    bool enabled = true;
    bool checked = false;
    bool separator = false;
    bool emphasized = false;
    // Optional compound Zoom row. id resets zoom; shortcut shows the percentage.
    UINT decrease_id = 0;
    UINT increase_id = 0;
    UINT extra_id = 0;
    // Disables reset/minus/plus independently of the fullscreen (extra) action.
    bool zoom_controls_enabled = true;
    // Optional native color chip (0xRRGGBB); distinct from the readable focus
    // accent so custom dark profile colors remain recognizable.
    std::optional<std::uint32_t> swatch_rgb = std::nullopt;
};

// anchor_screen is the popup's desired top edge and its right (or left) edge.
// All dimensions are DIPs at the supplied DPI. The menu is clamped to the
// monitor work area. It returns the selected caller id, or zero on dismissal.
// Standard icon strings are Segoe MDL2 Assets glyphs. L"incognito" draws a
// small vector hat-and-glasses icon matching the reference design.
UINT ShowDarkMenu(HWND owner, POINT anchor_screen, const std::vector<MenuItem>& items,
                  UINT dpi = 96, int width_dip = 334, bool align_right = true,
                  const Palette* palette = nullptr);

} // namespace webcom
