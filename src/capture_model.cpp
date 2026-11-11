#include "capture_model.h"

#include <array>
#include <string>

namespace webcom {
namespace {
int Base64Value(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return c - L'A';
    if (c >= L'a' && c <= L'z') return c - L'a' + 26;
    if (c >= L'0' && c <= L'9') return c - L'0' + 52;
    if (c == L'+') return 62;
    if (c == L'/') return 63;
    return -1;
}

std::optional<std::wstring_view> DataField(std::wstring_view json) {
    const auto key = json.find(L"\"data\"");
    if (key == std::wstring_view::npos) return std::nullopt;
    auto pos = json.find(L':', key + 6);
    if (pos == std::wstring_view::npos) return std::nullopt;
    ++pos;
    while (pos < json.size() && (json[pos] == L' ' || json[pos] == L'\t' || json[pos] == L'\r' || json[pos] == L'\n')) ++pos;
    if (pos >= json.size() || json[pos] != L'\"') return std::nullopt;
    const auto begin = ++pos;
    while (pos < json.size()) {
        if (json[pos] == L'\"') return json.substr(begin, pos - begin);
        if (json[pos] == L'\\') return std::nullopt; // base64 never needs JSON escaping
        ++pos;
    }
    return std::nullopt;
}
} // namespace

std::optional<std::vector<std::uint8_t>> DecodeCaptureScreenshotResult(
    std::wstring_view json, std::size_t maximum_bytes) {
    const auto encoded = DataField(json);
    if (!encoded || encoded->empty() || encoded->size() % 4 != 0) return std::nullopt;
    const std::size_t padding = encoded->ends_with(L"==") ? 2 : encoded->ends_with(L"=") ? 1 : 0;
    const std::size_t expected = encoded->size() / 4 * 3 - padding;
    if (expected == 0 || expected > maximum_bytes) return std::nullopt;
    std::vector<std::uint8_t> output;
    output.reserve(expected);
    for (std::size_t i = 0; i < encoded->size(); i += 4) {
        std::array<int,4> v{};
        for (int n = 0; n < 4; ++n) {
            const wchar_t c = (*encoded)[i + static_cast<std::size_t>(n)];
            if (c == L'=') v[n] = -2;
            else {
                v[n] = Base64Value(c);
                if (v[n] < 0) return std::nullopt;
            }
        }
        if (v[0] < 0 || v[1] < 0) return std::nullopt;
        const std::uint32_t block = (static_cast<std::uint32_t>(v[0]) << 18) |
                                    (static_cast<std::uint32_t>(v[1]) << 12) |
                                    (static_cast<std::uint32_t>(v[2] < 0 ? 0 : v[2]) << 6) |
                                    static_cast<std::uint32_t>(v[3] < 0 ? 0 : v[3]);
        output.push_back(static_cast<std::uint8_t>((block >> 16) & 0xff));
        if (v[2] != -2) output.push_back(static_cast<std::uint8_t>((block >> 8) & 0xff));
        if (v[3] != -2) output.push_back(static_cast<std::uint8_t>(block & 0xff));
        if (v[2] == -2 && v[3] != -2) return std::nullopt;
        if ((v[2] == -2 || v[3] == -2) && i + 4 != encoded->size()) return std::nullopt;
        if (output.size() > maximum_bytes) return std::nullopt;
    }
    if (output.size() != expected) return std::nullopt;
    return output;
}

} // namespace webcom
