#pragma once

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "chrome_layout.h"

namespace webcom {

// Reused DIBs prevent per-paint GDI allocation and expose a complete native
// frame to DWM in a single copy. No compositing styles are applied to WebView2.
class NativePaintBuffer {
public:
    NativePaintBuffer() = default;
    NativePaintBuffer(const NativePaintBuffer&) = delete;
    NativePaintBuffer& operator=(const NativePaintBuffer&) = delete;
    ~NativePaintBuffer() { Reset(); }

    HDC Begin(HDC reference, int width, int height, bool grow_only = false) {
        if (width <= 0 || height <= 0) return nullptr;
        if (grow_only) { width = std::max(width, width_); height = std::max(height, height_); }
        if (dc_ && width == width_ && height == height_) {
            SetViewportOrgEx(dc_, 0, 0, nullptr);
            SelectClipRgn(dc_, nullptr);
            return dc_;
        }
        // Allocate the replacement before releasing the previous valid buffer.
        HDC next_dc = CreateCompatibleDC(reference);
        if (!next_dc) return nullptr;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP next_bitmap = CreateDIBSection(reference, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!next_bitmap) { DeleteDC(next_dc); return nullptr; }
        const auto previous = SelectObject(next_dc, next_bitmap);
        if (!previous || previous == HGDI_ERROR) {
            DeleteObject(next_bitmap); DeleteDC(next_dc); return nullptr;
        }
        Reset();
        dc_ = next_dc; bitmap_ = next_bitmap; previous_ = previous;
        width_ = width; height_ = height;
        return dc_;
    }

    void Present(HDC destination, RECT dirty) const {
        if (!dc_) return;
        const RECT bounds{0, 0, width_, height_};
        RECT clipped{};
        if (IntersectRect(&clipped, &dirty, &bounds))
            BitBlt(destination, clipped.left, clipped.top, clipped.right - clipped.left,
                clipped.bottom - clipped.top, dc_, clipped.left, clipped.top, SRCCOPY);
    }

    void Reset() {
        if (dc_ && previous_) SelectObject(dc_, previous_);
        if (bitmap_) DeleteObject(bitmap_);
        if (dc_) DeleteDC(dc_);
        dc_ = nullptr; bitmap_ = nullptr; previous_ = nullptr; width_ = height_ = 0;
    }
private:
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    int width_ = 0, height_ = 0;
};

struct NativePlacement {
    HWND window = nullptr;
    RECT before{}, after{};
    bool was_visible = false, visible = false;
    bool move = false, resize = false;
};

// The caller describes each child's final state once. Repeated layout passes
// with identical bounds/visibility perform no positioning or invalidation.
class NativeLayoutBatch {
public:
    NativeLayoutBatch(HWND parent, std::vector<NativePlacement>& changes)
        : parent_(parent), changes_(changes) { changes_.clear(); }

    void Place(HWND window, int x, int y, int width, int height, bool visible) {
        if (!window) return;
        const bool was_visible = (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
        if (!visible && !was_visible) return;
        RECT before{};
        if (!GetWindowRect(window, &before)) return;
        MapWindowPoints(nullptr, parent_, reinterpret_cast<POINT*>(&before), 2);
        const RECT after{x, y, x + std::max(1, width), y + std::max(1, height)};
        const bool move = before.left != after.left || before.top != after.top;
        const bool resize = before.right - before.left != after.right - after.left ||
                            before.bottom - before.top != after.bottom - after.top;
        if (!move && !resize && visible == was_visible) return;
        changes_.push_back({window, before, after, was_visible, visible, move, resize});
    }

    void Commit() {
        if (changes_.empty()) return;
        auto flags = [](const NativePlacement& value) {
            UINT result = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
                          SWP_NOREDRAW | SWP_DEFERERASE;
            if (!value.move) result |= SWP_NOMOVE;
            if (!value.resize) result |= SWP_NOSIZE;
            if (value.visible != value.was_visible) result |= value.visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
            return result;
        };
        HDWP batch = BeginDeferWindowPos(static_cast<int>(changes_.size()));
        for (const auto& value : changes_) {
            if (!batch) break;
            batch = DeferWindowPos(batch, value.window, nullptr, value.after.left, value.after.top,
                value.after.right - value.after.left, value.after.bottom - value.after.top, flags(value));
        }
        const bool completed = batch && EndDeferWindowPos(batch);
        if (!completed) {
            // A failed HDWP is abandoned as required by the API; apply the same
            // final states individually, still without synchronous repaints.
            for (const auto& value : changes_)
                SetWindowPos(value.window, nullptr, value.after.left, value.after.top,
                    value.after.right - value.after.left, value.after.bottom - value.after.top, flags(value));
        }
        for (const auto& value : changes_) {
            if (value.was_visible) InvalidateRect(parent_, &value.before, FALSE);
            if (value.visible) {
                InvalidateRect(parent_, &value.after, FALSE);
                InvalidateRect(value.window, nullptr, FALSE);
            }
        }
    }
private:
    HWND parent_;
    std::vector<NativePlacement>& changes_;
};

struct NativeChromeState {
    NativePaintBuffer frame, tabs, button, suggestions;
    std::vector<NativePlacement> placements;
    std::vector<int> bookmark_widths;
    RECT client{}, content{}, omnibox{}, home_search_rect{};
    std::vector<RECT> tab_close_rects;
    UINT dpi = 0;
    std::uint64_t native_tab = 0;
    int native_page = -1;
    bool layout_running = false, layout_again = false;
    bool address_focused = false;
    std::wstring tab_accessible_label;
};

} // namespace webcom
