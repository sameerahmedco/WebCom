#pragma once

#include <optional>

namespace webcom {

struct CaptureSelectionRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

std::optional<CaptureSelectionRect> NormalizeCaptureSelection(
    int x1, int y1, int x2, int y2, int viewport_width, int viewport_height,
    int minimum_size = 4);

CaptureSelectionRect ScaleCaptureSelection(const CaptureSelectionRect& selection,
                                           int viewport_width, int viewport_height,
                                           int bitmap_width, int bitmap_height);

} // namespace webcom
