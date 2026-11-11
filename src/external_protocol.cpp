#include "external_protocol.h"

#include <cctype>

namespace webcom {
namespace {
constexpr std::size_t MaxUriBytes = 4096;

char Lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
}

ExternalProtocolResult Reject(std::string message) {
    return {ExternalProtocolKind::None, {}, false, std::move(message)};
}

ExternalProtocolResult Accept(ExternalProtocolKind kind, std::string uri) {
    return {kind, std::move(uri), true, {}};
}

bool SafePrintable(std::string_view value) {
    for (unsigned char c : value)
        if (c < 0x20 || c == 0x7f) return false;
    return true;
}

bool ValidTelephonePayload(std::string_view value, bool allow_query) {
    if (value.empty() || value.size() > 512) return false;
    bool seen_number = false;
    bool in_query = false;
    for (unsigned char c : value) {
        if (c == '?' && allow_query && !in_query) { in_query = true; continue; }
        if (in_query) {
            if (std::isalnum(c) || c=='%' || c=='&' || c=='=' || c=='+' || c=='-' || c=='.' || c=='_' || c=='~') continue;
            return false;
        }
        if (std::isdigit(c)) { seen_number = true; continue; }
        if (c=='+' || c=='-' || c=='.' || c=='(' || c==')' || c==';' || c==',') continue;
        return false;
    }
    return seen_number;
}
}

ExternalProtocolResult ResolveExternalProtocol(std::string_view input) {
    if (input.empty() || input.size() > MaxUriBytes || !SafePrintable(input))
        return Reject("The external link is empty, too long, or contains control characters.");
    const auto colon = input.find(':');
    if (colon == std::string_view::npos || colon == 0)
        return Reject("The external link does not contain a supported scheme.");
    const auto scheme = input.substr(0, colon);
    const auto payload = input.substr(colon + 1);
    if (EqualsIgnoreCase(scheme, "mailto")) {
        if (payload.empty() || payload.size() > 3072 || payload.starts_with("//") ||
            payload.find('\\') != std::string_view::npos)
            return Reject("The email link is malformed.");
        return Accept(ExternalProtocolKind::Mailto, std::string(input));
    }
    if (EqualsIgnoreCase(scheme, "tel")) {
        if (!ValidTelephonePayload(payload, false)) return Reject("The telephone link is malformed.");
        return Accept(ExternalProtocolKind::Telephone, std::string(input));
    }
    if (EqualsIgnoreCase(scheme, "sms")) {
        if (!ValidTelephonePayload(payload, true)) return Reject("The SMS link is malformed.");
        return Accept(ExternalProtocolKind::Sms, std::string(input));
    }
    return Reject("WebCom does not hand this link type to another application.");
}

std::string_view ExternalProtocolName(ExternalProtocolKind kind) {
    switch (kind) {
    case ExternalProtocolKind::Mailto: return "email application";
    case ExternalProtocolKind::Telephone: return "calling application";
    case ExternalProtocolKind::Sms: return "messaging application";
    case ExternalProtocolKind::None: break;
    }
    return "external application";
}

} // namespace webcom
