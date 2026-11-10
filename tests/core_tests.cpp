#include "core.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {
unsigned checks = 0;
unsigned failures = 0;

void Check(bool condition, std::string_view label) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }
}

void Address(std::string_view input, std::string_view expected) {
    const auto result = webcom::ResolveAddress(input);
    Check(result.allowed && result.error.empty() && result.url == expected,
          std::string("resolve ") + std::string(input));
    if (!result.allowed || result.url != expected) {
        std::cerr << "  Expected: " << expected << "\n  Actual: " << result.url
                  << "\n  Error: " << result.error << '\n';
    }
}

void Rejected(std::string_view input) {
    const auto result = webcom::ResolveAddress(input);
    Check(!result.allowed && result.url.empty() && !result.error.empty(),
          std::string("reject ") + std::string(input));
}
}  // namespace

int main() {
    using namespace webcom;
    Address("", "webcom:newtab");
    Address(" \t\r\n ", "webcom:newtab");
    Address("about:blank", "webcom:newtab");
    Address(" WEBCOM:NEWTAB ", "webcom:newtab");
    Address("webcom:history", "webcom:history");
    Address("WEBCOM:DOWNLOADS", "webcom:downloads");
    Address(" webcom:settings ", "webcom:settings");
    Address("webcom:bookmarks", "webcom:bookmarks");
    Address("WEBCOM:PROFILES", "webcom:profiles");
    Address("example.com", "https://example.com");
    Address(" HTTPS://Example.com/a b?q=x y#z ",
            "https://Example.com/a%20b?q=x%20y#z");
    Address("http://example.com:8080/a?q=b#c", "http://example.com:8080/a?q=b#c");
    Address("example.com:8080/path", "https://example.com:8080/path");
    Address("localhost:3000", "http://localhost:3000");
    Address("LOCALHOST/hello", "http://LOCALHOST/hello");
    Address("app.localhost:4000", "http://app.localhost:4000");
    Address("localhost.", "http://localhost.");
    Address("127.0.0.1:3000/path", "http://127.0.0.1:3000/path");
    Address("127.31.0.9", "http://127.31.0.9");
    Address("192.168.1.20:443", "https://192.168.1.20:443");
    Address("intranet:8080", "https://intranet:8080");
    Address("intranet/dashboard", "https://intranet/dashboard");
    Address("https://intranet", "https://intranet");
    Address("server.local", "https://server.local");
    Address("//example.com/path", "https://example.com/path");
    Address("[::1]:8000/test", "http://[::1]:8000/test");
    Address("[0:0:0:0:0:0:0:0001]", "http://[0:0:0:0:0:0:0:0001]");
    Address("::1", "http://[::1]");
    Address("::1/a b", "http://[::1]/a%20b");
    Address("2001:db8::1", "https://[2001:db8::1]");
    Address("fe80::1234", "https://[fe80::1234]");
    Address("[2001:db8::1]:8443", "https://[2001:db8::1]:8443");
    Address("[1::]", "https://[1::]");
    Address("https://[::1]:443/", "https://[::1]:443/");
    Address("HTTP://example.com/path", "http://example.com/path");
    Address("HTTPS://example.com/?email=user@example.com", "https://example.com/?email=user@example.com");
    Address("example.com/100%25", "https://example.com/100%25");
    Address("example.com/a b", "https://example.com/a%20b");
    Address("hello world", "https://www.google.com/search?q=hello%20world");
    Address("intranet", "https://www.google.com/search?q=intranet");
    Address("C++ & C#", "https://www.google.com/search?q=C%2B%2B%20%26%20C%23");
    Address("caf\xC3\xA9 tea", "https://www.google.com/search?q=caf%C3%A9%20tea");
    Address("a+b", "https://www.google.com/search?q=a%2Bb");
    Address("user@example.com", "https://www.google.com/search?q=user%40example.com");
    Address("what is example.com", "https://www.google.com/search?q=what%20is%20example.com");
    Address("https://example.com:65535/", "https://example.com:65535/");
    Address("https://example.com:0/", "https://example.com:0/");

    Rejected("javascript:alert(1)");
    Rejected("JaVaScRiPt:123");
    Rejected("data:text/html,<script>alert(1)</script>");
    Rejected("file:///C:/secret.txt");
    Rejected("mailto:someone@example.com");
    Rejected("mailto:80");
    Rejected("ftp://example.com/");
    Rejected("custom:payload");
    Rejected("chrome://settings");
    Rejected("about:config");
    Rejected("webcom:unknown");
    Rejected("https:example.com");
    Rejected("http:80");
    Rejected("https://");
    Rejected("http:///example.com");
    Rejected("https://exa mple.com/path");
    Rejected("https://example.com:65536");
    Rejected("https://example.com:bad");
    Rejected("https://example.com:");
    Rejected("https://user:pass@example.com/");
    Rejected("http://user@example.com/");
    Rejected("HTTPS://user:pass@example.com/");
    Rejected("https://999.1.1.1");
    Rejected("https://127.1");
    Rejected("https://0177.0.0.1");
    Rejected("https://[::1");
    Rejected("https://[:::1]");
    Rejected("[::1]garbage");
    Rejected("https://[1:2:3:4:5:6:7:8:9]");
    Rejected("https://[1:2:3:4:5:6:7]");
    Rejected("https://[1::2::3]");
    Rejected("https://[1:2:3:4:5:6:7::8]");
    Rejected("java\nscript:alert(1)");
    Rejected("https://exam\tple.com");
    Rejected(std::string("https://example.com/\0payload", 27));

    const std::string fields[]{
        "", "ordinary", "two words", "100%25%", "tab\tnewline\nreturn\r",
        "pipe|equal=slash/backslash\\", "caf\xC3\xA9", std::string("a\0b", 3)
    };
    for (const auto& field : fields) {
        const auto encoded = EscapeField(field);
        Check(UnescapeField(encoded) == field, "state field round trip");
        Check(encoded.find_first_of("\t\r\n|=") == std::string::npos,
              "state field has no delimiters");
    }
    Check(EscapeField("%0A") == "%250A", "literal percent encoded once");
    Check(UnescapeField("%250A") == "%0A", "decoding is not recursive");
    Check(UnescapeField("%4a%4B") == "JK", "mixed-case hex decoding");
    Check(UnescapeField("%Z0/%/%1") == "%Z0/%/%1", "malformed escapes preserved");

    Check(IsValidProfileName("Default"), "simple profile");
    Check(IsValidProfileName("Work 2_test-name"), "profile accepted characters");
    Check(IsValidProfileName(std::string(40, 'A')), "40-character profile");
    Check(!IsValidProfileName(std::string(41, 'A')), "overlong profile rejected");
    Check(!IsValidProfileName(""), "empty profile rejected");
    Check(!IsValidProfileName("   "), "blank profile rejected");
    Check(!IsValidProfileName(" Leading"), "leading space rejected");
    Check(!IsValidProfileName("Trailing "), "trailing space rejected");
    Check(!IsValidProfileName("../escape"), "profile path traversal rejected");
    Check(!IsValidProfileName("Work\\Personal"), "profile path separator rejected");
    Check(!IsValidProfileName("caf\xC3\xA9"), "non-ASCII profile rejected");
    Check(!IsValidProfileName("con"), "Windows device name rejected");
    Check(!IsValidProfileName("LPT9"), "Windows numbered device name rejected");
    Check(IsValidProfileName("Console"), "non-device prefix accepted");

    Check(SuspendDelayMs(MemoryMode::Balanced) == 300'000, "balanced sleep delay");
    Check(SuspendDelayMs(MemoryMode::Saver) == 60'000, "saver sleep delay");
    TabActivity activity;
    activity.last_active_ms = 100;
    Check(!ShouldSuspend(activity, 60'099, MemoryMode::Saver), "not before saver threshold");
    Check(ShouldSuspend(activity, 60'100, MemoryMode::Saver), "at saver threshold");
    Check(!ShouldSuspend(activity, 300'099, MemoryMode::Balanced), "not before balanced threshold");
    Check(ShouldSuspend(activity, 300'100, MemoryMode::Balanced), "at balanced threshold");
    Check(!ShouldSuspend(activity, 99, MemoryMode::Saver), "clock reversal does not underflow");
    Check(!ShouldSuspend(activity, 100, MemoryMode::Saver), "just activated tab stays awake");

    bool TabActivity::* const guards[]{
        &TabActivity::active, &TabActivity::loading, &TabActivity::audible,
        &TabActivity::pinned, &TabActivity::protected_from_sleep,
        &TabActivity::suspend_pending, &TabActivity::suspended
    };
    for (const auto guard : guards) {
        activity.*guard = true;
        Check(!ShouldSuspend(activity, 1'000'000, MemoryMode::Saver),
              "protected tab never suspended");
        activity.*guard = false;
    }
    activity.last_active_ms = std::numeric_limits<std::uint64_t>::max() - 10;
    Check(!ShouldSuspend(activity, 5, MemoryMode::Saver), "large timestamp does not wrap");
    activity.last_active_ms = 0;
    Check(ShouldSuspend(activity, std::numeric_limits<std::uint64_t>::max(),
                        MemoryMode::Balanced), "large valid elapsed duration");

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
