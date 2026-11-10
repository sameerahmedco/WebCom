#pragma once

#include <windows.h>
#include "tab_search_model.h"

namespace webcom {

struct Palette;

// Opens a native, keyboard-accessible search popup at the requested top-left
// screen point, clamped to the monitor work area. The caller supplies a snapshot
// and must resolve the returned stable tab ID again after this modal loop ends.
// At most 200 items are copied. No web views or tab thumbnails are created.
// Returns zero on cancellation or failure; preserves an observed WM_QUIT.
std::uint64_t ShowTabSearch(HWND owner, POINT anchor_screen,
                             const std::vector<TabSearchItem>& items, UINT dpi = 96,
                             const Palette* palette = nullptr);

} // namespace webcom
