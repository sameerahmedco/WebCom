#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace webcom {

struct AddressResult {
    std::string url;
    bool allowed = false;
    std::string error;
};

// Accepts UTF-8 omnibox/launch text. Successful results are HTTP(S) URLs,
// supported local-drive HTML/PDF file URLs, or native WebCom pages. Arbitrary
// operating-system URL schemes are never returned. Ambiguous single words are
// searched unless a port or path makes the hostname intent explicit.
AddressResult ResolveAddress(std::string_view input);

// Returns true only for a canonical, supported local file URL accepted by
// ResolveAddress. WebCom intentionally limits local navigation to HTML and PDF
// files and never enables Chromium's broad file-to-file access switch.
bool IsLocalFileAddress(std::string_view input);

// Byte-preserving percent encoding for tab/newline-delimited UTF-8 state.
// UnescapeField leaves malformed percent sequences unchanged.
std::string EscapeField(std::string_view input);
std::string UnescapeField(std::string_view input);

// Portable, Windows-safe profile names: 1-40 ASCII letters, digits, spaces,
// underscores or hyphens, with no leading/trailing spaces or device names.
bool IsValidProfileName(std::string_view name);

enum class MemoryMode { Balanced, Saver };

struct TabActivity {
    bool active = false;
    bool loading = false;
    bool audible = false;
    bool pinned = false;
    bool protected_from_sleep = false;
    bool suspend_pending = false;
    bool suspended = false;
    std::uint64_t last_active_ms = 0;
};

std::uint64_t SuspendDelayMs(MemoryMode mode);
bool ShouldSuspend(const TabActivity& activity, std::uint64_t now_ms,
                   MemoryMode mode);

}  // namespace webcom
