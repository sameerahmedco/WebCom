#include "launch_request.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
}

int main() {
    using namespace webcom;
    auto round_trip = [](LaunchRequest request, const char* label) {
        const auto decoded = DecodeLaunchRequest(EncodeLaunchRequest(request));
        Check(decoded && decoded->profile == request.profile && decoded->address == request.address &&
              decoded->private_mode == request.private_mode, label);
    };
    round_trip({}, "default launch");
    round_trip({"Work", "https://example.com/", false}, "named profile URL");
    round_trip({"Personal", "Google search words", true}, "private named profile search");
    round_trip({"Work Space", "https://example.com/\xE2\x98\x83?q=\xF0\x9F\x8C\x8E", false}, "Unicode URL");
    round_trip({std::string(40, 'p'), std::string(MaxLaunchAddressBytes, 'x'), true}, "maximum lengths");
    Check(EncodeLaunchRequest({std::string(41, 'p'), "", false}).empty(), "profile length bounded");
    Check(EncodeLaunchRequest({"Work", std::string(MaxLaunchAddressBytes + 1, 'x'), false}).empty(), "URL length bounded");
    for (const char* name : {"CON", "../Work", " Work", "Work ", "A/B", "A\\B", "Work\nPersonal"})
        Check(EncodeLaunchRequest({name, "", false}).empty(), "unsafe profile rejected");
    for (const std::string& address : {std::string("a\0b", 3), std::string("line\nline"), std::string("x\t"),
             std::string("\xc0\xaf", 2), std::string("\xed\xa0\x80", 3), std::string("\xf4\x90\x80\x80", 4),
             std::string("\x80", 1), std::string("\xe2\x98", 2)})
        Check(EncodeLaunchRequest({"Work", address, false}).empty(), "invalid/control text rejected");
    const auto packet = EncodeLaunchRequest({"Work", "https://example.com", true});
    for (std::size_t size = 0; size < packet.size(); ++size)
        Check(!DecodeLaunchRequest(std::span(packet).first(size)), "truncated packet rejected");
    for (const std::size_t index : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{3},
                                  std::size_t{4}, std::size_t{6}, std::size_t{7}}) {
        auto invalid = packet; invalid[index] ^= 0xff;
        Check(!DecodeLaunchRequest(invalid), "magic/version/reserved bytes rejected");
    }
    auto invalid = packet; invalid[5] = 2;
    Check(!DecodeLaunchRequest(invalid), "unknown flags rejected");
    invalid = packet; invalid.push_back(0);
    Check(!DecodeLaunchRequest(invalid), "trailing bytes rejected");
    invalid = packet; invalid[8] = 41;
    Check(!DecodeLaunchRequest(invalid), "declared profile too long");
    invalid = packet; std::fill(invalid.begin() + 12, invalid.begin() + 16, static_cast<std::uint8_t>(0xff));
    Check(!DecodeLaunchRequest(invalid), "oversized declared URL rejected without overflow");
    invalid = packet; invalid[16] = 0;
    Check(!DecodeLaunchRequest(invalid), "embedded profile NUL rejected");
    invalid = packet; invalid.back() = 0;
    Check(!DecodeLaunchRequest(invalid), "embedded address NUL rejected");
    // IPC validation never resolves a URL into another profile or executes it.
    const auto script = DecodeLaunchRequest(EncodeLaunchRequest({"Work", "javascript:alert(1)", false}));
    Check(script && !ResolveAddress(script->address).allowed, "URL policy remains a separate mandatory check");
    std::cout << checks << " launch-request checks; " << failures << " failures\n";
    return failures ? 1 : 0;
}
