#include "selection_capture_model.h"

#include <algorithm>
#include <cmath>

namespace webcom {

std::optional<CaptureSelectionRect> NormalizeCaptureSelection(
    int x1, int y1, int x2, int y2, int viewport_width, int viewport_height,
    int minimum_size) {
    if (viewport_width <= 0 || viewport_height <= 0 || minimum_size <= 0) return std::nullopt;
    const int left = std::clamp(std::min(x1, x2), 0, viewport_width);
    const int top = std::clamp(std::min(y1, y2), 0, viewport_height);
    const int right = std::clamp(std::max(x1, x2), 0, viewport_width);
    const int bottom = std::clamp(std::max(y1, y2), 0, viewport_height);
    if (right - left < minimum_size || bottom - top < minimum_size) return std::nullopt;
    return CaptureSelectionRect{left, top, right, bottom};
}

CaptureSelectionRect ScaleCaptureSelection(const CaptureSelectionRect& selection,
                                           int viewport_width, int viewport_height,
                                           int bitmap_width, int bitmap_height) {
    if (viewport_width <= 0 || viewport_height <= 0 || bitmap_width <= 0 || bitmap_height <= 0)
        return {};
    const double scale_x = static_cast<double>(bitmap_width) / viewport_width;
    const double scale_y = static_cast<double>(bitmap_height) / viewport_height;
    const auto scale = [](int value, double factor, int maximum) {
        return std::clamp(static_cast<int>(std::lround(value * factor)), 0, maximum);
    };
    CaptureSelectionRect result{
        scale(selection.left, scale_x, bitmap_width),
        scale(selection.top, scale_y, bitmap_height),
        scale(selection.right, scale_x, bitmap_width),
        scale(selection.bottom, scale_y, bitmap_height)};
    if (result.right <= result.left && result.left < bitmap_width) result.right = result.left + 1;
    if (result.bottom <= result.top && result.top < bitmap_height) result.bottom = result.top + 1;
    return result;
}

} // namespace webcom
