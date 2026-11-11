#include "reader_model.h"

#include <algorithm>
#include <sstream>

namespace webcom {

std::optional<std::wstring> DecodeJsonString(std::wstring_view json, std::size_t maximum_chars) {
    if (json.size() < 2 || json.front() != L'"' || json.back() != L'"') return std::nullopt;
    std::wstring out; out.reserve(std::min(maximum_chars, json.size() - 2));
    const auto hex = [](wchar_t c) -> int {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        if (c >= L'A' && c <= L'F') return c - L'A' + 10;
        return -1;
    };
    for (std::size_t i = 1; i + 1 < json.size();) {
        wchar_t c = json[i++];
        if (c != L'\\') out.push_back(c);
        else {
            if (i >= json.size() - 1) return std::nullopt;
            const wchar_t e = json[i++];
            switch (e) {
            case L'"': out.push_back(L'"'); break;
            case L'\\': out.push_back(L'\\'); break;
            case L'/': out.push_back(L'/'); break;
            case L'b': out.push_back(L'\b'); break;
            case L'f': out.push_back(L'\f'); break;
            case L'n': out.push_back(L'\n'); break;
            case L'r': out.push_back(L'\r'); break;
            case L't': out.push_back(L'\t'); break;
            case L'u': {
                if (i + 4 > json.size() - 1) return std::nullopt;
                unsigned value = 0;
                for (int n = 0; n < 4; ++n) {
                    const int h = hex(json[i++]); if (h < 0) return std::nullopt;
                    value = (value << 4) | static_cast<unsigned>(h);
                }
                if (value >= 0xD800 && value <= 0xDBFF) {
                    if (i + 6 > json.size() - 1 || json[i] != L'\\' || json[i + 1] != L'u') return std::nullopt;
                    i += 2; unsigned low = 0;
                    for (int n = 0; n < 4; ++n) {
                        const int h = hex(json[i++]); if (h < 0) return std::nullopt;
                        low = (low << 4) | static_cast<unsigned>(h);
                    }
                    if (low < 0xDC00 || low > 0xDFFF) return std::nullopt;
                    if constexpr (sizeof(wchar_t) == 2) {
                        out.push_back(static_cast<wchar_t>(value)); out.push_back(static_cast<wchar_t>(low));
                    } else {
                        out.push_back(static_cast<wchar_t>(0x10000 + ((value - 0xD800) << 10) + (low - 0xDC00)));
                    }
                } else if (value >= 0xDC00 && value <= 0xDFFF) return std::nullopt;
                else out.push_back(static_cast<wchar_t>(value));
                break;
            }
            default: return std::nullopt;
            }
        }
        if (out.size() > maximum_chars) { out.resize(maximum_chars); break; }
    }
    return out;
}

namespace {
std::string Utf8FromWide(std::wstring_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned cp = static_cast<unsigned>(value[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                if (i + 1 < value.size()) {
                    const unsigned low = static_cast<unsigned>(value[i + 1]);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        ++i;
                    } else cp = 0xFFFD;
                } else cp = 0xFFFD;
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) cp = 0xFFFD;
        }
        if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}
std::string HtmlEscape(std::wstring_view value) {
    std::string out;
    for (wchar_t c : value) {
        switch (c) {
        case L'&': out += "&amp;"; break;
        case L'<': out += "&lt;"; break;
        case L'>': out += "&gt;"; break;
        case L'\"': out += "&quot;"; break;
        default: out += Utf8FromWide(std::wstring_view(&c,1)); break;
        }
    }
    return out;
}
}

std::string BuildReaderHtml(std::wstring_view title, std::wstring_view source,
                            std::wstring_view text, int text_scale, int width_mode, bool serif) {
    const int font_size = std::clamp(18 + text_scale * 2, 14, 28);
    const int max_width = width_mode <= 0 ? 640 : width_mode == 1 ? 780 : 960;
    std::ostringstream out;
    out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>" << HtmlEscape(title)
        << "</title><style>body{margin:0;background:#fff;color:#111;font:" << font_size
        << "px/1.65 " << (serif ? "Georgia,serif" : "Segoe UI,Arial,sans-serif")
        << ";}main{max-width:" << max_width << "px;margin:48px auto;padding:0 24px;}h1{line-height:1.2;font-size:2em;}"
        << ".source{color:#666;font-size:.82em;word-break:break-all;}p{white-space:pre-wrap;}@media print{main{margin:0 auto;}}</style></head><body><main><h1>"
        << HtmlEscape(title) << "</h1>";
    if (!source.empty()) out << "<div class=\"source\">" << HtmlEscape(source) << "</div>";
    out << "<p>" << HtmlEscape(text) << "</p></main></body></html>";
    return out.str();
}

std::string BuildReaderMarkdown(std::wstring_view title, std::wstring_view source,
                                std::wstring_view text) {
    std::string result;
    if (!title.empty()) result += "# " + Utf8FromWide(title) + "\n\n";
    if (!source.empty()) result += "Source: " + Utf8FromWide(source) + "\n\n";
    result += Utf8FromWide(text);
    if (result.empty() || result.back() != '\n') result.push_back('\n');
    return result;
}

} // namespace webcom
