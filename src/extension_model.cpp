#include "extension_model.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

namespace webcom {
namespace {
constexpr std::size_t kMaxManifestBytes = 2u * 1024u * 1024u;
constexpr std::size_t kMaxPreviewStrings = 64;

void SkipSpace(std::string_view text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
}

std::optional<std::size_t> ValueStart(std::string_view text, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    auto pos = text.find(needle);
    if (pos == std::string_view::npos) return std::nullopt;
    pos += needle.size(); SkipSpace(text, pos);
    if (pos >= text.size() || text[pos] != ':') return std::nullopt;
    ++pos; SkipSpace(text, pos);
    return pos;
}

std::optional<std::string> ParseJsonString(std::string_view text, std::size_t& pos) {
    if (pos >= text.size() || text[pos] != '"') return std::nullopt;
    ++pos; std::string out;
    while (pos < text.size()) {
        const char c = text[pos++];
        if (c == '"') return out;
        if (c == '\\') {
            if (pos >= text.size()) return std::nullopt;
            const char escaped = text[pos++];
            switch (escaped) {
            case '"': out.push_back('"'); break; case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break; case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break; case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break; case 't': out.push_back('\t'); break;
            default: return std::nullopt; // preview intentionally skips \u decoding
            }
        } else {
            if (static_cast<unsigned char>(c) < 0x20) return std::nullopt;
            out.push_back(c);
        }
        if (out.size() > 4096) return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> JsonStringValue(std::string_view text, std::string_view key) {
    const auto start = ValueStart(text, key); if (!start) return std::nullopt;
    auto pos = *start; return ParseJsonString(text, pos);
}

std::optional<int> JsonIntegerValue(std::string_view text, std::string_view key) {
    const auto start = ValueStart(text, key); if (!start) return std::nullopt;
    auto pos = *start;
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return std::nullopt;
    int value = 0;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        value = value * 10 + (text[pos++] - '0'); if (value > 99) return std::nullopt;
    }
    return value;
}

std::vector<std::string> JsonStringArrayValue(std::string_view text, std::string_view key) {
    std::vector<std::string> values;
    const auto start = ValueStart(text, key); if (!start) return values;
    auto pos = *start; if (pos >= text.size() || text[pos] != '[') return values;
    ++pos;
    while (pos < text.size() && values.size() < kMaxPreviewStrings) {
        SkipSpace(text, pos);
        if (pos < text.size() && text[pos] == ']') break;
        const auto value = ParseJsonString(text, pos); if (!value) return {};
        values.push_back(*value); SkipSpace(text, pos);
        if (pos < text.size() && text[pos] == ',') { ++pos; continue; }
        if (pos < text.size() && text[pos] == ']') break;
        return {};
    }
    return values;
}

bool HasKey(std::string_view text, std::string_view key) {
    return text.find("\"" + std::string(key) + "\"") != std::string_view::npos;
}

bool HasToolbarUi(std::string_view text) {
    return HasKey(text,"action") || HasKey(text,"browser_action") || HasKey(text,"page_action") || HasKey(text,"default_popup");
}

std::string JoinPreview(const std::vector<std::string>& values, std::size_t max_items = 8) {
    if (values.empty()) return "None declared";
    std::ostringstream out;
    const auto count = std::min(values.size(), max_items);
    for (std::size_t i = 0; i < count; ++i) { if (i) out << ", "; out << values[i]; }
    if (values.size() > count) out << " +" << (values.size() - count) << " more";
    return out.str();
}
} // namespace

ExtensionFolderInfo InspectExtensionFolder(const std::filesystem::path& folder) {
    ExtensionFolderInfo result; result.folder = folder.wstring();
    std::error_code ec;
    if (folder.empty() || !std::filesystem::is_directory(folder, ec) || ec) {
        result.error = "Choose the top-level folder of an unpacked extension."; return result;
    }
    const auto manifest = folder / "manifest.json";
    if (!std::filesystem::is_regular_file(manifest, ec) || ec) {
        result.error = "The selected folder does not contain manifest.json."; return result;
    }
    const auto bytes = std::filesystem::file_size(manifest, ec);
    if (ec || bytes == 0 || bytes > kMaxManifestBytes) {
        result.error = "manifest.json is empty or larger than WebCom's 2 MiB preview limit."; return result;
    }
    std::ifstream input(manifest, std::ios::binary);
    if (!input) { result.error = "manifest.json could not be read."; return result; }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto name = JsonStringValue(text,"name");
    const auto version = JsonStringValue(text,"version");
    const auto manifest_version = JsonIntegerValue(text,"manifest_version");
    if (!name || name->empty() || !version || version->empty() || !manifest_version) {
        result.error = "manifest.json is missing a readable name, version, or manifest_version."; return result;
    }
    result.name=*name; result.version=*version; result.manifest_version=*manifest_version;
    result.description=JsonStringValue(text,"description").value_or("");
    result.minimum_chrome_version=JsonStringValue(text,"minimum_chrome_version").value_or("");
    result.permissions=JsonStringArrayValue(text,"permissions");
    result.host_permissions=JsonStringArrayValue(text,"host_permissions");
    result.optional_permissions=JsonStringArrayValue(text,"optional_permissions");
    result.optional_host_permissions=JsonStringArrayValue(text,"optional_host_permissions");
    result.default_popup=JsonStringValue(text,"default_popup").value_or("");
    result.options_page=JsonStringValue(text,"options_page").value_or("");
    result.declares_toolbar_ui=HasToolbarUi(text);
    result.has_background=HasKey(text,"background") || HasKey(text,"service_worker");
    result.has_content_scripts=HasKey(text,"content_scripts");
    result.has_options_page=HasKey(text,"options_page") || HasKey(text,"options_ui");
    result.has_side_panel=HasKey(text,"side_panel");
    result.has_devtools_page=HasKey(text,"devtools_page");
    result.has_commands=HasKey(text,"commands");
    result.has_omnibox=HasKey(text,"omnibox");
    result.has_web_accessible_resources=HasKey(text,"web_accessible_resources");
    result.valid=true; return result;
}

std::string ExtensionCompatibilitySummary(const ExtensionFolderInfo& info) {
    if (!info.valid) return info.error;
    std::ostringstream out;
    out << info.name << " " << info.version << " (Manifest V" << info.manifest_version << ")\n";
    if (!info.description.empty()) out << info.description << "\n";
    if (!info.minimum_chrome_version.empty()) out << "Minimum Chromium version: " << info.minimum_chrome_version << "\n";
    out << "Permissions: " << JoinPreview(info.permissions) << "\n";
    out << "Host permissions: " << JoinPreview(info.host_permissions) << "\n";
    out << "Optional permissions: " << JoinPreview(info.optional_permissions) << "\n";
    out << "Optional host permissions: " << JoinPreview(info.optional_host_permissions) << "\n";
    out << "Background/service worker: " << (info.has_background?"Yes":"No") << "\n";
    out << "Content scripts: " << (info.has_content_scripts?"Yes":"No") << "\n";
    out << "Options page: " << (info.has_options_page?"Declared":"No") << "\n";
    out << "Commands: " << (info.has_commands?"Declared":"No") << "\n";
    out << "Omnibox integration: " << (info.has_omnibox?"Declared":"No") << "\n";
    out << "DevTools page: " << (info.has_devtools_page?"Declared":"No") << "\n";
    out << "Web-accessible resources: " << (info.has_web_accessible_resources?"Declared":"No") << "\n";
    out << "Chrome toolbar/popup UI: " << (info.declares_toolbar_ui?"Declared - WebView2 may not expose its normal toolbar entry point":"Not declared") << "\n";
    out << "Chrome side-panel UI: " << (info.has_side_panel?"Declared - WebView2 does not provide Chrome's browser side-panel host UI":"Not declared");
    return out.str();
}

} // namespace webcom
