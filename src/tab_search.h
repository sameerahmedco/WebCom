#pragma once

#include <windows.h>
#include "tab_search_model.h"

namespace webcom {

struct Palette;

struct SearchPickerOptions {
    const wchar_t* title = L"Search tabs";
    const wchar_t* placeholder = L"Search title or address";
    const wchar_t* empty_text = L"No matching tabs.\nTry a page title or address.";
    const wchar_t* noun_singular = L"tab";
    const wchar_t* noun_plural = L"tabs";
    const wchar_t* footer = L"↑ ↓ navigate   Enter switch   Esc close";
    bool show_tab_state = true;
};

// Opens a native, keyboard-accessible search popup at the requested top-left
// screen point, clamped to the monitor work area. The caller supplies a snapshot
// and must resolve the returned stable tab ID again after this modal loop ends.
// At most 200 items are copied. No web views or tab thumbnails are created.
// Returns zero on cancellation or failure; preserves an observed WM_QUIT.
std::uint64_t ShowTabSearch(HWND owner, POINT anchor_screen,
                             const std::vector<TabSearchItem>& items, UINT dpi = 96,
                             const Palette* palette = nullptr,
                             const SearchPickerOptions* options = nullptr);

} // namespace webcom
