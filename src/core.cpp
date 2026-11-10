#include "core.h"

#include <array>
#include <cstddef>
#include <utility>

namespace webcom {
namespace {

constexpr bool IsAsciiAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr bool IsAsciiDigit(char c) { return c >= '0' && c <= '9'; }

constexpr char Lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (Lower(a[i]) != Lower(b[i])) return false;
    }
    return true;
}

bool EndsWithIgnoreCase(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           EqualsIgnoreCase(value.substr(value.size() - suffix.size()), suffix);
}

constexpr bool IsAsciiWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\f' || c == '\v';
}

std::string_view Trim(std::string_view value) {
    while (!value.empty() && IsAsciiWhitespace(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && IsAsciiWhitespace(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

constexpr int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

constexpr bool IsUnreserved(unsigned char c) {
    return IsAsciiAlpha(static_cast<char>(c)) ||
           IsAsciiDigit(static_cast<char>(c)) || c == '-' || c == '.' ||
           c == '_' || c == '~';
}

std::string PercentEncode(std::string_view input) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size());
    for (const unsigned char c : input) {
        if (IsUnreserved(c)) {
            result.push_back(static_cast<char>(c));
        } else {
            result.push_back('%');
            result.push_back(digits[c >> 4]);
            result.push_back(digits[c & 15]);
        }
    }
    return result;
}

AddressResult Reject(std::string message) {
    return {{}, false, std::move(message)};
}

AddressResult Accept(std::string url) {
    return {std::move(url), true, {}};
}

bool IsScheme(std::string_view value) {
    if (value.empty() || !IsAsciiAlpha(value.front())) return false;
    for (char c : value) {
        if (!IsAsciiAlpha(c) && !IsAsciiDigit(c) && c != '+' && c != '-' &&
            c != '.') return false;
    }
    return true;
}

bool IsReservedScheme(std::string_view value) {
    constexpr std::array schemes{
        "javascript", "vbscript", "data", "file", "mailto", "about",
        "webcom", "chrome", "edge", "devtools", "blob", "filesystem",
        "view-source", "ftp", "ftps", "ws", "wss", "tel", "sms",
        "shell", "ms-settings", "ms-appx", "ms-appx-web", "ms-browser-extension"
    };
    for (const char* scheme : schemes) {
        if (EqualsIgnoreCase(value, scheme)) return true;
    }
    return false;
}

bool ValidPort(std::string_view port) {
    if (port.empty() || port.size() > 5) return false;
    unsigned value = 0;
    for (char c : port) {
        if (!IsAsciiDigit(c)) return false;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    return value <= 65535;
}

bool ParseIPv4(std::string_view host, std::array<unsigned, 4>* parts = nullptr) {
    std::array<unsigned, 4> values{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto dot = host.find('.');
        const auto part = host.substr(0, dot);
        if (part.empty() || part.size() > 3) return false;
        // Avoid Chromium's legacy octal interpretation of zero-prefixed IPv4.
        if (part.size() > 1 && part.front() == '0') return false;
        for (char c : part) {
            if (!IsAsciiDigit(c)) return false;
            values[i] = values[i] * 10 + static_cast<unsigned>(c - '0');
        }
        if (values[i] > 255) return false;
        if (i == values.size() - 1) {
            if (dot != std::string_view::npos) return false;
        } else {
            if (dot == std::string_view::npos) return false;
            host.remove_prefix(dot + 1);
        }
    }
    if (parts) *parts = values;
    return true;
}

bool ParseIPv6(std::string_view host, bool* loopback = nullptr) {
    if (host.empty() || host.find(':') == std::string_view::npos) return false;
    const auto compression = host.find("::");
    if (compression != std::string_view::npos &&
        host.find("::", compression + 1) != std::string_view::npos) return false;
    if ((host.front() == ':' && !host.starts_with("::")) ||
        (host.back() == ':' && !host.ends_with("::"))) return false;

    unsigned groups = 0;
    bool all_before_last_zero = true;
    unsigned last_value = 0;
    std::size_t cursor = 0;
    while (cursor < host.size()) {
        if (host[cursor] == ':') {
            ++cursor;
            continue;
        }
        const auto end = host.find(':', cursor);
        const auto part = host.substr(cursor, end == std::string_view::npos
                                                ? host.size() - cursor
                                                : end - cursor);
        if (groups > 0 && last_value != 0) all_before_last_zero = false;
        if (part.find('.') != std::string_view::npos) {
            if (end != std::string_view::npos || !ParseIPv4(part)) return false;
            groups += 2;
            all_before_last_zero = false;
            last_value = 0;
        } else {
            if (part.empty() || part.size() > 4) return false;
            last_value = 0;
            for (char c : part) {
                const int digit = HexValue(c);
                if (digit < 0) return false;
                last_value = last_value * 16 + static_cast<unsigned>(digit);
            }
            ++groups;
        }
        if (end == std::string_view::npos) break;
        cursor = end + 1;
    }
    if ((compression == std::string_view::npos && groups != 8) ||
        (compression != std::string_view::npos && groups >= 8)) return false;
    // With trailing compression, the last group belongs before the inserted
    // zeros, so the address cannot be ::1 even if that group is one.
    if (loopback) {
        *loopback = all_before_last_zero && last_value == 1 &&
                    !host.ends_with("::");
    }
    return true;
}

bool ValidHostName(std::string_view host) {
    if (host.empty() || host.size() > 253) return false;
    if (host.back() == '.') host.remove_suffix(1);
    if (host.empty()) return false;
    while (!host.empty()) {
        const auto dot = host.find('.');
        const auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || label.front() == '-' ||
            label.back() == '-') return false;
        for (unsigned char c : label) {
            // Non-ASCII bytes are passed to WebView2 for IDNA processing.
            if (c < 0x80 && !IsAsciiAlpha(static_cast<char>(c)) &&
                !IsAsciiDigit(static_cast<char>(c)) && c != '-' && c != '_') {
                return false;
            }
        }
        if (dot == std::string_view::npos) break;
        host.remove_prefix(dot + 1);
    }
    return true;
}

struct Authority {
    bool valid = false;
    bool loopback = false;
    bool has_port = false;
    bool bracketed = false;
    std::string_view host;
};

Authority ParseAuthority(std::string_view authority, bool allow_user_info) {
    Authority result;
    const auto at = authority.rfind('@');
    if (at != std::string_view::npos) {
        if (!allow_user_info || at == 0) return result;
        authority.remove_prefix(at + 1);
    }
    if (authority.empty()) return result;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos) return result;
        result.host = authority.substr(1, close - 1);
        result.bracketed = true;
        if (!ParseIPv6(result.host, &result.loopback)) return result;
        const auto suffix = authority.substr(close + 1);
        if (!suffix.empty()) {
            if (suffix.front() != ':' || !ValidPort(suffix.substr(1))) return result;
            result.has_port = true;
        }
        result.valid = true;
        return result;
    }
    const auto colon = authority.find(':');
    result.host = authority.substr(0, colon);
    if (colon != std::string_view::npos) {
        if (!ValidPort(authority.substr(colon + 1))) return result;
        result.has_port = true;
    }
    if (!ValidHostName(result.host)) return result;

    auto host = result.host;
    if (host.back() == '.') host.remove_suffix(1);
    bool only_digits_and_dots = true;
    for (char c : host) {
        if (!IsAsciiDigit(c) && c != '.') only_digits_and_dots = false;
    }
    if (only_digits_and_dots) {
        std::array<unsigned, 4> ipv4{};
        // Avoid ambiguous legacy single-number and short-form IPv4 addresses.
        if (!ParseIPv4(host, &ipv4)) return result;
        result.loopback = ipv4[0] == 127;
    } else {
        result.loopback = EqualsIgnoreCase(host, "localhost") ||
                          EndsWithIgnoreCase(host, ".localhost");
    }
    result.valid = true;
    return result;
}

std::string EncodeUrlSpaces(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (char c : input) {
        if (c == ' ') result += "%20";
        else result.push_back(c);
    }
    return result;
}

}  // namespace

AddressResult ResolveAddress(std::string_view input) {
    input = Trim(input);
    if (input.empty() || EqualsIgnoreCase(input, "about:blank") ||
        EqualsIgnoreCase(input, "webcom:newtab")) {
        return Accept("webcom:newtab");
    }
    for (const char* page : {"webcom:history", "webcom:downloads", "webcom:settings",
                             "webcom:bookmarks", "webcom:profiles"}) {
        if (EqualsIgnoreCase(input, page)) return Accept(page);
    }
    for (unsigned char c : input) {
        if (c < 0x20 || c == 0x7f) {
            return Reject("The address contains an unsupported control character.");
        }
    }

    const auto colon = input.find(':');
    const auto scheme = input.substr(0, colon);
    if (colon != std::string_view::npos &&
        (EqualsIgnoreCase(scheme, "http") || EqualsIgnoreCase(scheme, "https"))) {
        if (input.substr(colon, 3) != "://") {
            return Reject("Use a complete address beginning with http:// or https://.");
        }
        const auto remainder = input.substr(colon + 3);
        if (remainder.find('\\') != std::string_view::npos) {
            return Reject("Use forward slashes in website addresses.");
        }
        const auto authority = remainder.substr(0, remainder.find_first_of("/?#"));
        if (authority.find('@') != std::string_view::npos) {
            return Reject("Sign in on the website instead of putting credentials in its address.");
        }
        if (authority.find(' ') != std::string_view::npos ||
            !ParseAuthority(authority, false).valid) {
            return Reject("Enter a valid website address and port.");
        }
        return Accept(std::string(EqualsIgnoreCase(scheme, "https") ? "https://" : "http://") +
                      EncodeUrlSpaces(remainder));
    }
    if (colon != std::string_view::npos && IsReservedScheme(scheme)) {
        return Reject("WebCom opens http:// and https:// addresses from the address bar.");
    }

    auto candidate = input;
    const bool network_path = candidate.starts_with("//");
    if (network_path) candidate.remove_prefix(2);
    const auto path = candidate.find_first_of("/?#");
    const auto authority_text = candidate.substr(0, path);

    bool raw_ipv6_loopback = false;
    if (ParseIPv6(authority_text, &raw_ipv6_loopback)) {
        return Accept(std::string(raw_ipv6_loopback ? "http://[" : "https://[") +
                      std::string(authority_text) + "]" +
                      (path == std::string_view::npos ? "" :
                       EncodeUrlSpaces(candidate.substr(path))));
    }

    const auto authority = ParseAuthority(authority_text, false);
    if (colon != std::string_view::npos && IsScheme(scheme) &&
        !(authority.valid && authority.has_port)) {
        return Reject("WebCom opens http:// and https:// addresses from the address bar.");
    }
    const bool looks_like_host = network_path || authority.has_port ||
        authority.bracketed || authority.loopback ||
        authority.host.find('.') != std::string_view::npos ||
        path != std::string_view::npos;
    if (authority.valid && looks_like_host) {
        return Accept(std::string(authority.loopback ? "http://" : "https://") +
                      EncodeUrlSpaces(candidate));
    }
    if (network_path || candidate.starts_with('[')) {
        return Reject("Enter a valid website address and port.");
    }
    return Accept("https://www.google.com/search?q=" + PercentEncode(input));
}

std::string EscapeField(std::string_view input) { return PercentEncode(input); }

std::string UnescapeField(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            const int high = HexValue(input[i + 1]);
            const int low = HexValue(input[i + 2]);
            if (high >= 0 && low >= 0) {
                result.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        result.push_back(input[i]);
    }
    return result;
}

bool IsValidProfileName(std::string_view name) {
    if (name.empty() || name.size() > 40 || name.front() == ' ' ||
        name.back() == ' ') return false;
    for (char c : name) {
        if (!IsAsciiAlpha(c) && !IsAsciiDigit(c) && c != ' ' && c != '_' &&
            c != '-') return false;
    }
    constexpr std::array device_names{"CON", "PRN", "AUX", "NUL"};
    for (const char* device : device_names) {
        if (EqualsIgnoreCase(name, device)) return false;
    }
    if (name.size() == 4 && name[3] >= '1' && name[3] <= '9' &&
        (EqualsIgnoreCase(name.substr(0, 3), "COM") ||
         EqualsIgnoreCase(name.substr(0, 3), "LPT"))) return false;
    return true;
}

std::uint64_t SuspendDelayMs(MemoryMode mode) {
    return mode == MemoryMode::Saver ? 60'000 : 300'000;
}

bool ShouldSuspend(const TabActivity& activity, std::uint64_t now_ms,
                   MemoryMode mode) {
    if (activity.active || activity.loading || activity.audible ||
        activity.pinned || activity.protected_from_sleep ||
        activity.suspend_pending || activity.suspended ||
        now_ms < activity.last_active_ms) return false;
    return now_ms - activity.last_active_ms >= SuspendDelayMs(mode);
}

}  // namespace webcom
