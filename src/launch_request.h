#pragma once

#include "core.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace webcom {

// A bounded, versioned IPC packet. UTF-8 and explicit little-endian lengths keep
// the format independent of compiler structure packing and wchar_t width.
struct LaunchRequest {
    std::string profile;
    std::string address;
    bool private_mode = false;
};

inline constexpr std::size_t MaxLaunchAddressBytes = 65536;
inline constexpr std::size_t MaxLaunchPacketBytes = 16 + 40 + MaxLaunchAddressBytes;

namespace launch_detail {
inline bool ValidUtf8(std::string_view input) {
    for (std::size_t i = 0; i < input.size();) {
        const auto first = static_cast<unsigned char>(input[i++]);
        if (first < 0x80) {
            if (first < 0x20 || first == 0x7f) return false;
            continue;
        }
        unsigned following = 0;
        std::uint32_t value = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { following = 1; value = first & 31; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { following = 2; value = first & 15; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { following = 3; value = first & 7; minimum = 0x10000; }
        else return false;
        if (following > input.size() - i) return false;
        while (following--) {
            const auto next = static_cast<unsigned char>(input[i++]);
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

inline bool Valid(const LaunchRequest& request) {
    return (request.profile.empty() || IsValidProfileName(request.profile)) &&
        request.address.size() <= MaxLaunchAddressBytes && ValidUtf8(request.address);
}

inline void Put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

inline std::uint32_t Get32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8 * i);
    return value;
}
} // namespace launch_detail

inline std::vector<std::uint8_t> EncodeLaunchRequest(const LaunchRequest& request) {
    if (!launch_detail::Valid(request)) return {};
    std::vector<std::uint8_t> bytes(16, 0);
    bytes[0] = 'W'; bytes[1] = 'C'; bytes[2] = 'L'; bytes[3] = '3';
    bytes[4] = 1;
    bytes[5] = request.private_mode ? 1 : 0;
    launch_detail::Put32(bytes, 8, static_cast<std::uint32_t>(request.profile.size()));
    launch_detail::Put32(bytes, 12, static_cast<std::uint32_t>(request.address.size()));
    bytes.insert(bytes.end(), request.profile.begin(), request.profile.end());
    bytes.insert(bytes.end(), request.address.begin(), request.address.end());
    return bytes;
}

inline std::optional<LaunchRequest> DecodeLaunchRequest(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 16 || bytes.size() > MaxLaunchPacketBytes ||
        bytes[0] != 'W' || bytes[1] != 'C' || bytes[2] != 'L' || bytes[3] != '3' ||
        bytes[4] != 1 || bytes[5] > 1 || bytes[6] || bytes[7]) return std::nullopt;
    const auto profile_length = launch_detail::Get32(bytes, 8);
    const auto address_length = launch_detail::Get32(bytes, 12);
    if (profile_length > 40 || address_length > MaxLaunchAddressBytes ||
        bytes.size() != 16 + static_cast<std::size_t>(profile_length) + address_length) return std::nullopt;
    LaunchRequest request;
    request.private_mode = bytes[5] != 0;
    request.profile.assign(reinterpret_cast<const char*>(bytes.data() + 16), profile_length);
    request.address.assign(reinterpret_cast<const char*>(bytes.data() + 16 + profile_length), address_length);
    if (!launch_detail::Valid(request)) return std::nullopt;
    return request;
}

} // namespace webcom
