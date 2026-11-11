#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace webcom {

// Decodes the JSON string value returned by WebView2 ExecuteScript. Only a
// single JSON string is accepted; objects/arrays/numbers are deliberately
// rejected so Reader Mode never treats arbitrary script output as page text.
std::optional<std::wstring> DecodeJsonString(std::wstring_view json,
                                             std::size_t maximum_chars = 300000);

std::string BuildReaderHtml(std::wstring_view title, std::wstring_view source,
                            std::wstring_view text, int text_scale, int width_mode, bool serif);
std::string BuildReaderMarkdown(std::wstring_view title, std::wstring_view source,
                                std::wstring_view text);

} // namespace webcom
