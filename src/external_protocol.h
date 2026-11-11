#pragma once

#include <string>
#include <string_view>

namespace webcom {

enum class ExternalProtocolKind { None, Mailto, Telephone, Sms };

struct ExternalProtocolResult {
    ExternalProtocolKind kind = ExternalProtocolKind::None;
    std::string uri;
    bool allowed = false;
    std::string error;
};

// Only a deliberately small allow-list can leave WebCom. This parser is used
// for links initiated by web content; the omnibox itself remains HTTP(S)-only.
ExternalProtocolResult ResolveExternalProtocol(std::string_view input);
std::string_view ExternalProtocolName(ExternalProtocolKind kind);

} // namespace webcom
