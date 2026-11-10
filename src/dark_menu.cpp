#include "dark_menu.h"
#include "appearance.h"
#include "render_helpers.h"
#include "win32_com_compat.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <algorithm>
#include <cwctype>
#include <limits>
#include <string_view>
#include <utility>

namespace webcom {
namespace {

constexpr wchar_t kMenuClass[] = L"WebCom.DarkPopupMenu.v1";
constexpr wchar_t kActiveMenuProperty[] = L"WebCom.ActiveDarkPopupMenu";

COLORREF NativeColor(std::uint32_t rgb) {
    return RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

COLORREF Blend(COLORREF foreground, COLORREF background, unsigned amount) {
    const auto channel = [amount](unsigned a, unsigned b) { return (a * amount + b * (100 - amount)) / 100; };
    return RGB(channel(GetRValue(foreground), GetRValue(background)),
               channel(GetGValue(foreground), GetGValue(background)),
               channel(GetBValue(foreground), GetBValue(background)));
}

std::wstring BoundedLabel(std::wstring_view value, std::size_t limit) {
    std::wstring result(value.substr(0, limit));
    for (wchar_t& c : result) if (c < 0x20 || c == 0x7f) c = L' ';
    if (!result.empty() && result.back() >= 0xd800 && result.back() <= 0xdbff) result.pop_back();
    return result;
}

void SolidRect(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void RoundedPath(Gdiplus::GraphicsPath& path, RECT rect, int diameter, float inset) {
    using Gdiplus::REAL;
    const REAL x=rect.left+inset,y=rect.top+inset;
    const REAL w=std::max(1.0f,rect.right-rect.left-2*inset);
    const REAL h=std::max(1.0f,rect.bottom-rect.top-2*inset);
    const REAL d=std::max(1.0f,std::min({static_cast<REAL>(diameter),w,h}));
    path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);
    path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();
}

void RoundBox(HDC dc, const RECT& rect, COLORREF color, int radius) {
    using namespace Gdiplus;
    Graphics graphics(dc);graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    GraphicsPath path;RoundedPath(path,rect,radius,0.5f);
    SolidBrush brush(Color(255,GetRValue(color),GetGValue(color),GetBValue(color)));
    graphics.FillPath(&brush,&path);
}

void Outline(HDC dc, RECT rect, COLORREF color, int radius, int thickness = 1) {
    using namespace Gdiplus;
    Graphics graphics(dc);graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    GraphicsPath path;RoundedPath(path,rect,radius,static_cast<REAL>(thickness)/2);
    Pen pen(Color(255,GetRValue(color),GetGValue(color),GetBValue(color)),static_cast<REAL>(thickness));
    graphics.DrawPath(&pen,&path);
}

void DrawLabel(HDC dc, std::wstring_view label, RECT rect, HFONT font,
               COLORREF color, UINT alignment = DT_LEFT) {
    const auto old_font = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, label.data(), static_cast<int>(label.size()), &rect,
              alignment | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, old_font);
}

struct MenuState {
    HWND owner = nullptr;
    HWND window = nullptr;
    HWND previous_focus = nullptr;
    HINSTANCE instance = nullptr;
    UINT dpi = 96;
    int width = 0;
    int height = 0;
    int natural_height = 0;
    int scroll_offset = 0;
    int selected = -1;
    int pressed = -1;
    UINT hovered_command = 0;
    UINT pressed_command = 0;
    bool done = false;
    bool keyboard_selection = false;
    UINT result = 0;
    HFONT font = nullptr;
    HFONT heading_font = nullptr;
    HFONT small_font = nullptr;
    HFONT icon_font = nullptr;
    COLORREF kBackground = 0, kText = 0, kMuted = 0, kDisabled = 0, kHover = 0;
    COLORREF kEmphasis = 0, kDivider = 0, kFocus = 0, kField = 0;
    NativePaintBuffer chrome_buffer, row_buffer;
    std::vector<NativePlacement> placements;
    std::vector<MenuItem> items;
    std::vector<RECT> rows;
    std::vector<HWND> buttons;
    struct ZoomButton { HWND window = nullptr; int row = 0; int part = 0; UINT command = 0; };
    std::vector<ZoomButton> zoom_buttons;

    int D(int value) const { return MulDiv(value, static_cast<int>(dpi), 96); }
    void ApplyPalette(const Palette* requested) {
        const auto palette = requested ? *requested : MakePalette(Appearance{}, "Default");
        kBackground = NativeColor(palette.frame); kField = NativeColor(palette.surface);
        kText = NativeColor(palette.ink); kMuted = NativeColor(palette.muted);
        kHover = NativeColor(palette.hover); kDivider = NativeColor(palette.border);
        kFocus = NativeColor(palette.accent);
        kEmphasis = Blend(kFocus, kBackground, 9);
        kDisabled = Blend(kMuted, kBackground, 66);
    }
    bool IsZoom(int index) const { return items[index].decrease_id || items[index].increase_id || items[index].extra_id; }
    UINT PartCommand(int index, int part) const {
        if (index < 0 || static_cast<std::size_t>(index) >= items.size()) return 0;
        const auto& item = items[index];
        if (!item.enabled || item.separator) return 0;
        if (part == 3) return item.extra_id;
        if (IsZoom(index) && !item.zoom_controls_enabled) return 0;
        return part == 0 ? item.id : part == 1 ? item.decrease_id : item.increase_id;
    }
    UINT DefaultCommand(int index) const {
        for (int part = 0; part <= 3; ++part)
            if (const UINT command = PartCommand(index, part)) return command;
        return 0;
    }
    bool Actionable(int index) const {
        return DefaultCommand(index) != 0;
    }
    RECT ZoomRect(int index, int part) const {
        RECT row = DisplayRow(index);
        if (part == 0) return {width - D(139), row.top + D(8), width - D(99), row.bottom - D(8)};
        if (part == 1) return {width - D(172), row.top + D(8), width - D(144), row.bottom - D(8)};
        if (part == 2) return {width - D(92), row.top + D(8), width - D(64), row.bottom - D(8)};
        return {width - D(40), row.top + D(8), width - D(12), row.bottom - D(8)};
    }
    UINT CommandAt(int index, POINT point) const {
        if (!Actionable(index)) return 0;
        if (IsZoom(index)) {
            for (int part = 0; part <= 3; ++part) {
                const RECT rect = ZoomRect(index, part);
                if (PtInRect(&rect, point)) return PartCommand(index, part);
            }
            return 0; // The label and spaces are not a hidden reset-zoom button.
        }
        return PartCommand(index, 0);
    }
    int MaximumScroll() const { return std::max(0, natural_height - height); }
    RECT DisplayRow(int index) const {
        RECT rect = rows[index];
        OffsetRect(&rect, 0, -scroll_offset);
        return rect;
    }
    bool FullyVisible(const RECT& rect) const {
        return rect.top >= D(8) && rect.bottom <= height - D(8);
    }
    void End(UINT command = 0) {
        if (done) return;
        done = true;
        result = command;
        const HWND closing_window = window;
        if (GetCapture() == closing_window) ReleaseCapture();
        if (closing_window && IsWindow(closing_window)) DestroyWindow(closing_window);
    }
    void Invalidate() const {
        if (!window) return;
        InvalidateRect(window, nullptr, FALSE);
        for (HWND button : buttons) if (button && IsWindowVisible(button)) InvalidateRect(button, nullptr, FALSE);
        for (const auto& button : zoom_buttons) if (button.window && IsWindowVisible(button.window)) InvalidateRect(button.window, nullptr, FALSE);
    }
    void LayoutButtons() {
        NativeLayoutBatch batch(window, placements);
        for (std::size_t i = 0; i < buttons.size(); ++i) {
            if (!buttons[i]) continue;
            const RECT row = DisplayRow(static_cast<int>(i));
            // Fully visible native buttons supply accessible names/actions.
            // Partial rows are drawn by the clipped parent until scrolled in.
            const bool visible = FullyVisible(row);
            const RECT button_rect = IsZoom(static_cast<int>(i)) ? ZoomRect(static_cast<int>(i), 0) : row;
            batch.Place(buttons[i], button_rect.left, button_rect.top,
                        button_rect.right - button_rect.left, button_rect.bottom - button_rect.top, visible);
        }
        for (const auto& button : zoom_buttons) {
            const bool visible = FullyVisible(DisplayRow(button.row));
            const RECT rect = ZoomRect(button.row, button.part);
            batch.Place(button.window, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, visible);
        }
        batch.Commit();
        Invalidate();
    }
    void EnsureVisible(int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= rows.size()) return;
        RECT display = DisplayRow(index);
        const int padding = D(12);
        if (display.top < padding) scroll_offset = rows[index].top - padding;
        else if (display.bottom > height - padding) scroll_offset = rows[index].bottom - height + padding;
        scroll_offset = std::clamp(scroll_offset, 0, MaximumScroll());
        LayoutButtons();
    }
    void Select(int index, bool keyboard) {
        if (index >= 0 && !Actionable(index)) index = -1;
        if (selected == index && keyboard_selection == keyboard) return;
        selected = index;
        keyboard_selection = keyboard;
        hovered_command = DefaultCommand(index);
        if (keyboard && selected >= 0) {
            EnsureVisible(selected);
            HWND focus = window;
            if (buttons[selected] && IsWindowVisible(buttons[selected]) && IsWindowEnabled(buttons[selected]))
                focus = buttons[selected];
            else for (const auto& button : zoom_buttons)
                if (button.row == selected && button.command == hovered_command &&
                    IsWindowVisible(button.window) && IsWindowEnabled(button.window)) { focus = button.window; break; }
            SetFocus(focus);
        }
        Invalidate();
    }
    void Hover(POINT point) {
        const int index = Hit(point);
        const UINT command = CommandAt(index, point);
        if (selected == index && hovered_command == command && !keyboard_selection) return;
        selected = index;
        hovered_command = command;
        keyboard_selection = false;
        Invalidate();
    }
    int First() const {
        for (int i = 0; static_cast<std::size_t>(i) < items.size(); ++i) if (Actionable(i)) return i;
        return -1;
    }
    int Last() const {
        for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i) if (Actionable(i)) return i;
        return -1;
    }
    int Next(int direction) const {
        if (items.empty()) return -1;
        if (selected < 0) return direction > 0 ? First() : Last();
        const int count = static_cast<int>(items.size());
        int candidate = selected;
        for (int i = 0; i < count; ++i) {
            candidate = (candidate + direction + count) % count;
            if (Actionable(candidate)) return candidate;
        }
        return -1;
    }
    int Hit(POINT point) const {
        if (point.x < 0 || point.x >= width || point.y < D(8) || point.y >= height - D(8)) return -1;
        point.y += scroll_offset;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (PtInRect(&rows[i], point)) return Actionable(static_cast<int>(i)) ? static_cast<int>(i) : -1;
        return -1;
    }
    bool Inside(POINT point) const {
        return point.x >= 0 && point.y >= 0 && point.x < width && point.y < height;
    }
    void Scroll(int change) {
        if (!MaximumScroll()) return;
        scroll_offset = std::clamp(scroll_offset + change, 0, MaximumScroll());
        selected = -1;
        pressed = -1;
        hovered_command = 0;
        pressed_command = 0;
        keyboard_selection = false;
        LayoutButtons();
    }
    void IncognitoIcon(HDC dc, RECT rect, COLORREF color) const {
        const int center_x = (rect.left + rect.right) / 2;
        const int center_y = (rect.top + rect.bottom) / 2;
        const int saved = SaveDC(dc);
        HPEN pen = CreatePen(PS_SOLID, std::max(1, D(1)), color);
        HBRUSH brush = CreateSolidBrush(color);
        SelectObject(dc, pen);
        SelectObject(dc, brush);
        POINT hat[] = {{center_x - D(5), center_y - D(2)}, {center_x - D(3), center_y - D(8)},
                       {center_x, center_y - D(7)}, {center_x + D(3), center_y - D(8)},
                       {center_x + D(5), center_y - D(2)}};
        Polygon(dc, hat, static_cast<int>(std::size(hat)));
        MoveToEx(dc, center_x - D(8), center_y, nullptr);
        LineTo(dc, center_x + D(8), center_y);
        SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Ellipse(dc, center_x - D(7), center_y + D(2), center_x - D(1), center_y + D(8));
        Ellipse(dc, center_x + D(1), center_y + D(2), center_x + D(7), center_y + D(8));
        MoveToEx(dc, center_x - D(1), center_y + D(4), nullptr);
        LineTo(dc, center_x + D(2), center_y + D(4));
        RestoreDC(dc, saved);
        DeleteObject(pen);
        DeleteObject(brush);
    }
    void DrawRow(HDC dc, int index, RECT rect) const {
        const MenuItem& item = items[index];
        SolidRect(dc, rect, kBackground);
        // Child buttons cover entire rows, so retain the popup's side border
        // inside their own buffered paint as well as in the parent frame.
        SolidRect(dc, {rect.left, rect.top, rect.left + 1, rect.bottom}, kDivider);
        SolidRect(dc, {rect.right - 1, rect.top, rect.right, rect.bottom}, kDivider);
        if (item.separator) {
            const int middle = (rect.top + rect.bottom) / 2;
            RECT line{D(16), middle, width - D(16), middle + 1};
            SolidRect(dc, line, kDivider);
            return;
        }
        if (IsZoom(index)) {
            const bool zoom_enabled = item.enabled && item.zoom_controls_enabled;
            const COLORREF color = zoom_enabled ? kText : kDisabled;
            RECT icon_rect{D(17), rect.top, D(39), rect.bottom};
            if (!item.icon.empty()) DrawLabel(dc, item.icon, icon_rect, icon_font, zoom_enabled ? kMuted : kDisabled, DT_CENTER);
            RECT label_rect{D(49), rect.top, width - D(181), rect.bottom};
            DrawLabel(dc, item.text, label_rect, font, color);
            for (int part = 0; part <= 3; ++part) {
                RECT button = ZoomRect(index, part);
                OffsetRect(&button, 0, rect.top - DisplayRow(index).top);
                DrawZoomButton(dc, index, part, button);
            }
            RECT divider{width - D(53), rect.top + D(9), width - D(53) + std::max(1, D(1)), rect.bottom - D(9)};
            SolidRect(dc, divider, kDivider);
            return;
        }
        const bool highlighted = index == selected;
        if (item.emphasized || highlighted) {
            RECT highlight = rect;
            highlight.left += D(8);
            highlight.right -= D(8);
            if (item.emphasized) { highlight.top += D(4); highlight.bottom -= D(4); }
            else { highlight.top += D(1); highlight.bottom -= D(1); }
            const COLORREF fill = highlighted ? kHover : kEmphasis;
            RoundBox(dc, highlight, fill, D(12));
        }
        const COLORREF color = item.enabled ? kText : kDisabled;
        RECT icon_rect{D(17), rect.top, D(39), rect.bottom};
        if (item.swatch_rgb) {
            const int center_y = (rect.top + rect.bottom) / 2;
            RECT chip{D(19), center_y - D(8), D(35), center_y + D(8)};
            RoundBox(dc, chip, NativeColor(*item.swatch_rgb), D(16));
            Outline(dc, chip, Blend(kText, kBackground, 40), D(16));
        }
        else if (item.icon == L"incognito") IncognitoIcon(dc, icon_rect, item.enabled ? kMuted : kDisabled);
        else if (!item.icon.empty()) DrawLabel(dc, item.icon, icon_rect, icon_font, item.enabled ? kMuted : kDisabled, DT_CENTER);
        else if (item.checked) DrawLabel(dc, L"\uE73E", icon_rect, icon_font, color, DT_CENTER);

        const bool trailing_check = item.checked && (!item.icon.empty() || item.swatch_rgb);
        const int check_width = trailing_check ? D(25) : 0;
        if (trailing_check) {
            RECT check_rect{width - D(39), rect.top, width - D(17), rect.bottom};
            DrawLabel(dc, L"\uE73E", check_rect, icon_font, item.enabled ? kFocus : kDisabled, DT_CENTER);
        }
        int shortcut_width = 0;
        if (!item.shortcut.empty()) {
            SIZE extent{};
            const auto old_font = SelectObject(dc, small_font);
            GetTextExtentPoint32W(dc, item.shortcut.c_str(), static_cast<int>(item.shortcut.size()), &extent);
            SelectObject(dc, old_font);
            shortcut_width = std::min(static_cast<int>(extent.cx), width / 3);
            RECT shortcut_rect{width - D(18) - check_width - shortcut_width, rect.top,
                               width - D(18) - check_width, rect.bottom};
            DrawLabel(dc, item.shortcut, shortcut_rect, small_font, item.enabled ? kMuted : kDisabled, DT_RIGHT);
        }
        RECT label_rect{D(49), rect.top, width - D(18) - check_width - shortcut_width - (shortcut_width ? D(12) : 0), rect.bottom};
        DrawLabel(dc, item.text, label_rect, item.emphasized ? heading_font : font, color);
        if (highlighted && keyboard_selection) {
            RECT focus = rect;
            focus.left += D(9); focus.right -= D(9);
            focus.top += item.emphasized ? D(5) : D(2);
            focus.bottom -= item.emphasized ? D(5) : D(2);
            Outline(dc, focus, kFocus, D(12), std::max(1,D(1)));
        }
    }
    void DrawZoomButton(HDC dc, int index, int part, RECT rect) const {
        const auto& item = items[index];
        const UINT command = PartCommand(index, part);
        const bool enabled = command != 0;
        const bool hover = enabled && index == selected && hovered_command == command;
        SolidRect(dc, rect, kBackground);
        if (part != 0 || hover) RoundBox(dc, rect, hover ? kHover : kField, part == 0 ? D(8) : D(28));
        const auto color = enabled ? kText : kDisabled;
        if (part == 0) DrawLabel(dc, item.shortcut, rect, font, color, DT_CENTER);
        else if (part == 1) DrawLabel(dc, L"\uE738", rect, icon_font, color, DT_CENTER);
        else if (part == 2) DrawLabel(dc, L"\uE710", rect, icon_font, color, DT_CENTER);
        else DrawLabel(dc, L"\uE740", rect, icon_font, color, DT_CENTER);
        if (hover && keyboard_selection && enabled) {
            RECT focus = rect; InflateRect(&focus, -D(1), -D(1));
            DrawFocusRect(dc, &focus);
        }
    }
    void DrawControl(const DRAWITEMSTRUCT& draw, int row, int part = -1) {
        const int w = static_cast<int>(draw.rcItem.right - draw.rcItem.left);
        const int h = static_cast<int>(draw.rcItem.bottom - draw.rcItem.top);
        HDC buffered = row_buffer.Begin(draw.hDC, w, h, true);
        HDC dc = buffered ? buffered : draw.hDC;
        const RECT bounds = buffered ? RECT{0, 0, w, h} : draw.rcItem;
        if (part >= 0) DrawZoomButton(dc, row, part, bounds);
        else DrawRow(dc, row, bounds);
        if (buffered) BitBlt(draw.hDC, draw.rcItem.left, draw.rcItem.top, w, h, dc, 0, 0, SRCCOPY);
    }
    void Paint() {
        PAINTSTRUCT paint{};
        HDC target = BeginPaint(window, &paint);
        HDC buffered = chrome_buffer.Begin(target, width, height);
        HDC dc = buffered ? buffered : target;
        const RECT client{0, 0, width, height};
        SolidRect(dc, client, kBackground);
        Outline(dc, client, kDivider, D(24));
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, 0, D(8), width, height - D(8));
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const RECT row = DisplayRow(static_cast<int>(i));
            if (row.bottom > D(8) && row.top < height - D(8)) DrawRow(dc, static_cast<int>(i), row);
        }
        RestoreDC(dc, saved);
        if (MaximumScroll()) {
            if (scroll_offset > 0) {
                RECT indicator{0, 0, width, D(9)};
                SolidRect(dc, indicator, kBackground);
                DrawLabel(dc, L"\uE70E", indicator, icon_font, kMuted, DT_CENTER);
            }
            if (scroll_offset < MaximumScroll()) {
                RECT indicator{0, height - D(9), width, height};
                SolidRect(dc, indicator, kBackground);
                DrawLabel(dc, L"\uE70D", indicator, icon_font, kMuted, DT_CENTER);
            }
        }
        if (buffered) chrome_buffer.Present(target, paint.rcPaint);
        EndPaint(window, &paint);
    }
    bool Key(UINT key) {
        switch (key) {
        case VK_ESCAPE: case VK_MENU: case VK_F10: End(); return true;
        case VK_LEFT:
            if (Actionable(selected) && IsZoom(selected)) {
                if (const UINT command = PartCommand(selected, 1)) End(command);
            } else End();
            return true;
        case VK_RIGHT:
            if (Actionable(selected) && IsZoom(selected))
                if (const UINT command = PartCommand(selected, 2)) End(command);
            if (Actionable(selected) && !IsZoom(selected) && !items[selected].shortcut.empty() &&
                items[selected].shortcut.back() == L'\u203A')
                End(DefaultCommand(selected));
            return true;
        case VK_DOWN: Select(Next(1), true); return true;
        case VK_UP: Select(Next(-1), true); return true;
        case VK_HOME: Select(First(), true); return true;
        case VK_END: Select(Last(), true); return true;
        case VK_TAB: Select(Next((GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1), true); return true;
        case VK_NEXT: Scroll(height - D(40)); Select(FirstVisible(), true); return true;
        case VK_PRIOR: Scroll(-height + D(40)); Select(FirstVisible(), true); return true;
        case VK_RETURN: case VK_SPACE: {
            const int item = selected >= 0 ? selected : First();
            if (selected >= 0 && hovered_command) End(hovered_command);
            else if (const UINT command = DefaultCommand(item)) End(command);
            return true;
        }
        default: return false;
        }
    }
    int FirstVisible() const {
        for (std::size_t i = 0; i < items.size(); ++i)
            if (Actionable(static_cast<int>(i)) && FullyVisible(DisplayRow(static_cast<int>(i)))) return static_cast<int>(i);
        return First();
    }
    void TypeAhead(wchar_t key) {
        if (key < L' ' || (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000)) return;
        const int count = static_cast<int>(items.size());
        if (!count) return;
        const wint_t target = std::towlower(key);
        int index = selected < 0 ? count - 1 : selected;
        for (int i = 0; i < count; ++i) {
            index = (index + 1) % count;
            if (Actionable(index) && !items[index].text.empty() && std::towlower(items[index].text.front()) == target) {
                Select(index, true); return;
            }
        }
    }
};

LRESULT CALLBACK MenuWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<MenuState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<MenuState*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window, message, wparam, lparam);
    try {
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: state->Paint(); return 0;
    case WM_DRAWITEM: {
        const auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
        const auto found = std::find(state->buttons.begin(), state->buttons.end(), draw->hwndItem);
        if (found != state->buttons.end()) {
            const int index = static_cast<int>(found - state->buttons.begin());
            state->DrawControl(*draw, index, state->IsZoom(index) ? 0 : -1);
            return TRUE;
        }
        for (const auto& button : state->zoom_buttons) if (button.window == draw->hwndItem) {
            state->DrawControl(*draw, button.row, button.part);
            return TRUE;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        state->Hover(point);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (!state->Inside(point)) { state->End(); return 0; }
        state->pressed = state->Hit(point);
        state->pressed_command = state->CommandAt(state->pressed, point);
        state->Hover(point);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        const int index = state->Hit(point);
        const UINT command = state->CommandAt(index, point);
        if (index == state->pressed && command && command == state->pressed_command) state->End(command);
        state->pressed = -1;
        state->pressed_command = 0;
        return 0;
    }
    case WM_MOUSEWHEEL:
        state->Scroll(-GET_WHEEL_DELTA_WPARAM(wparam) * state->D(84) / WHEEL_DELTA);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wparam) == BN_SETFOCUS) {
            const HWND button = reinterpret_cast<HWND>(lparam);
            const auto found = std::find(state->buttons.begin(), state->buttons.end(), button);
            if (found != state->buttons.end()) {
                state->selected = static_cast<int>(found - state->buttons.begin());
                state->hovered_command = state->PartCommand(state->selected, 0);
            } else for (const auto& zoom : state->zoom_buttons) if (zoom.window == button) {
                state->selected = zoom.row; state->hovered_command = zoom.command; break;
            }
            state->keyboard_selection = true;
            state->Invalidate();
            return 0;
        }
        if (HIWORD(wparam) == BN_CLICKED) {
            const HWND button = reinterpret_cast<HWND>(lparam);
            if (!button) return 0;
            const auto found = std::find(state->buttons.begin(), state->buttons.end(), button);
            if (found != state->buttons.end()) {
                const int index = static_cast<int>(found - state->buttons.begin());
                if (const UINT command = state->PartCommand(index, 0)) state->End(command);
            }
            for (const auto& zoom_button : state->zoom_buttons) if (zoom_button.window == button && state->Actionable(zoom_button.row)) {
                if (const UINT command = state->PartCommand(zoom_button.row, zoom_button.part)) state->End(command);
                break;
            }
        }
        return 0;
    case WM_KEYDOWN: if (state->Key(static_cast<UINT>(wparam))) return 0; break;
    case WM_CHAR: state->TypeAhead(static_cast<wchar_t>(wparam)); return 0;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE && !state->done) state->End();
        return 0;
    case WM_CAPTURECHANGED:
        if (!state->done && reinterpret_cast<HWND>(lparam) != window &&
            (!lparam || !IsChild(window, reinterpret_cast<HWND>(lparam)))) state->End();
        return 0;
    case WM_CANCELMODE: case WM_CLOSE: case WM_DISPLAYCHANGE: case WM_DPICHANGED:
        state->End(); return 0;
    case WM_DESTROY: state->done = true; return 0;
    case WM_NCDESTROY:
        state->window = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    default: break;
    }
    } catch (...) {
        if (message == WM_NCCREATE) return FALSE;
        state->End();
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

struct ResourceCleanup {
    MenuState& state;
    ~ResourceCleanup() {
        if (state.window && IsWindow(state.window)) DestroyWindow(state.window);
        if (state.font) DeleteObject(state.font);
        if (state.heading_font) DeleteObject(state.heading_font);
        if (state.small_font) DeleteObject(state.small_font);
        if (state.icon_font) DeleteObject(state.icon_font);
    }
};

} // namespace

UINT ShowDarkMenu(HWND owner, POINT anchor_screen, const std::vector<MenuItem>& items,
                  UINT dpi, int width_dip, bool align_right, const Palette* palette) {
    if (!owner || !IsWindow(owner) || items.empty() || items.size() > 512) return 0;
    const HWND existing = static_cast<HWND>(GetPropW(owner, kActiveMenuProperty));
    if (existing && IsWindow(existing)) SendMessageW(existing, WM_CANCELMODE, 0, 0);
    if (!IsWindow(owner)) return 0;

    MenuState state;
    ResourceCleanup cleanup{state};
    state.owner = owner;
    state.instance = GetModuleHandleW(nullptr);
    state.previous_focus = GetFocus();
    state.dpi = std::clamp(dpi, 72u, 768u);
    state.ApplyPalette(palette);
    state.items = items;
    for (auto& item : state.items) {
        item.text = BoundedLabel(item.text, 512);
        item.shortcut = BoundedLabel(item.shortcut, 80);
        item.icon = BoundedLabel(item.icon, 20);
    }
    state.buttons.resize(items.size(), nullptr);
    state.width = state.D(std::clamp(width_dip, 220, 600));
    state.font = CreateFontW(-state.D(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    state.heading_font = CreateFontW(-state.D(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    state.small_font = CreateFontW(-state.D(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    state.icon_font = CreateFontW(-state.D(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    if (!state.font || !state.heading_font || !state.small_font || !state.icon_font) return 0;
    int y = state.D(8);
    state.rows.reserve(items.size());
    for (const auto& item : state.items) {
        const bool zoom = item.decrease_id || item.increase_id || item.extra_id;
        const int row_height = state.D(item.separator ? 10 : item.emphasized ? 44 : zoom ? 44 : 32);
        state.rows.push_back({0, y, state.width, y + row_height});
        y += row_height;
    }
    state.natural_height = y + state.D(8);

    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromPoint(anchor_screen, MONITOR_DEFAULTTONEAREST), &monitor)) return 0;
    const RECT work = monitor.rcWork;
    const int margin = std::max(1, state.D(4));
    if (work.right - work.left - margin * 2 < state.D(220) ||
        work.bottom - work.top - margin * 2 < state.D(80)) return 0;
    state.width = std::min(state.width, std::max(1, static_cast<int>(work.right - work.left) - margin * 2));
    state.height = std::min(state.natural_height, std::max(1, static_cast<int>(work.bottom - work.top) - margin * 2));
    for (auto& row : state.rows) row.right = state.width;
    int left = anchor_screen.x - (align_right ? state.width : 0);
    int top = anchor_screen.y;
    left = std::clamp(left, static_cast<int>(work.left) + margin,
                       std::max(static_cast<int>(work.left) + margin, static_cast<int>(work.right) - state.width - margin));
    top = std::clamp(top, static_cast<int>(work.top) + margin,
                      std::max(static_cast<int>(work.top) + margin, static_cast<int>(work.bottom) - state.height - margin));

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = MenuWindowProc;
    window_class.hInstance = state.instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kMenuClass;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 0;
    HWND popup = CreateWindowExW(WS_EX_TOOLWINDOW, kMenuClass, L"WebCom menu",
                                WS_POPUP | WS_CLIPCHILDREN,
                                left, top, state.width, state.height,
                                owner, nullptr, state.instance, &state);
    if (!popup) return 0;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(popup, 20, &dark, sizeof(dark));
    HRGN region = CreateRoundRectRgn(0, 0, state.width + 1, state.height + 1, state.D(24), state.D(24));
    if (region && !SetWindowRgn(popup, region, FALSE)) DeleteObject(region);

    for (std::size_t i = 0; i < state.items.size(); ++i) {
        const auto& item = state.items[i];
        if (item.separator) continue;
        std::wstring accessible_name = item.text;
        if (state.IsZoom(static_cast<int>(i))) accessible_name = L"Reset zoom, " + item.shortcut;
        if (item.checked) accessible_name += L" (selected)";
        if (!state.IsZoom(static_cast<int>(i)) && !item.shortcut.empty()) accessible_name += L", " + item.shortcut;
        const RECT rect = state.IsZoom(static_cast<int>(i)) ? state.ZoomRect(static_cast<int>(i), 0) : state.rows[i];
        if (item.id) {
            HWND button = CreateWindowExW(0, L"BUTTON", accessible_name.c_str(),
                WS_CHILD | WS_TABSTOP | BS_OWNERDRAW | BS_NOTIFY,
                rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                popup, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(0x4000 + i)), state.instance, nullptr);
            state.buttons[i] = button;
            if (button) {
                SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(state.font), FALSE);
                EnableWindow(button, state.PartCommand(static_cast<int>(i), 0) ? TRUE : FALSE);
            }
        }
        if (state.IsZoom(static_cast<int>(i))) {
            for (int part = 1; part <= 3; ++part) {
                const UINT command = part == 1 ? item.decrease_id : part == 2 ? item.increase_id : item.extra_id;
                if (!command) continue;
                const RECT bounds = state.ZoomRect(static_cast<int>(i), part);
                const wchar_t* label = part == 1 ? L"Zoom out" : part == 2 ? L"Zoom in" : L"Full screen";
                HWND button = CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW | BS_NOTIFY,
                    bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                    popup, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(0x5000 + i * 4 + part)), state.instance, nullptr);
                if (button) {
                    EnableWindow(button, state.PartCommand(static_cast<int>(i), part) ? TRUE : FALSE);
                    state.zoom_buttons.push_back({button, static_cast<int>(i), part, command});
                }
            }
        }
    }
    SetPropW(owner, kActiveMenuProperty, popup);
    state.LayoutButtons();
    BOOL animate = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0);
    if (!animate || GetSystemMetrics(SM_REMOTESESSION) || !AnimateWindow(popup, 85, AW_BLEND | AW_ACTIVATE))
        ShowWindow(popup, SW_SHOW);
    UpdateWindow(popup);
    SetForegroundWindow(popup);
    SetFocus(popup);
    SetCapture(popup);
    NotifyWinEvent(EVENT_SYSTEM_MENUPOPUPSTART, popup, OBJID_CLIENT, CHILDID_SELF);

    bool repost_quit = false;
    int quit_code = 0;
    while (!state.done && state.window && IsWindow(owner)) {
        MSG message{};
        const BOOL status = GetMessageW(&message, nullptr, 0, 0);
        if (status <= 0) {
            repost_quit = status == 0;
            quit_code = static_cast<int>(message.wParam);
            state.End();
            break;
        }
        const bool belongs = message.hwnd == popup || IsChild(popup, message.hwnd);
        if (belongs && (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) &&
            state.Key(static_cast<UINT>(message.wParam))) continue;
        if (belongs && message.message == WM_CHAR) {
            state.TypeAhead(static_cast<wchar_t>(message.wParam)); continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    state.End();
    if (IsWindow(owner)) {
        if (GetPropW(owner, kActiveMenuProperty) == popup) RemovePropW(owner, kActiveMenuProperty);
        // Dismissing a menu to another application must not steal focus back.
        if (GetForegroundWindow() == owner && state.previous_focus && IsWindow(state.previous_focus) &&
            (state.previous_focus == owner || IsChild(owner, state.previous_focus))) SetFocus(state.previous_focus);
    }
    NotifyWinEvent(EVENT_SYSTEM_MENUPOPUPEND, owner, OBJID_CLIENT, CHILDID_SELF);
    if (repost_quit) PostQuitMessage(quit_code);
    return state.result;
}

} // namespace webcom
