#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace webcom {

// Decodes the {"data":"..."} JSON object returned by CDP Page.captureScreenshot.
// Only a single base64 data field is accepted and output is capped so malformed
// renderer responses cannot allocate unbounded host memory.
std::optional<std::vector<std::uint8_t>> DecodeCaptureScreenshotResult(
    std::wstring_view json, std::size_t maximum_bytes = 64u * 1024u * 1024u);

} // namespace webcom
