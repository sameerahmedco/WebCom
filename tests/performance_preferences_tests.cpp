#include "performance_preferences.h"

#include <filesystem>
#include <iostream>

namespace {
int checks = 0;
int failures = 0;
void Check(bool ok, const char* name) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } }
}

int main() {
    using namespace webcom;
    const auto root = std::filesystem::temp_directory_path() / "webcom-performance-preferences-tests";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::string error;
    Check(!LoadSleepTimeoutPreference(root, "Default").has_value(), "missing preferences use caller default");
    for (const auto mode : {SleepTimeout::OneMinute, SleepTimeout::FiveMinutes, SleepTimeout::FifteenMinutes, SleepTimeout::ThirtyMinutes, SleepTimeout::Never}) {
        Check(SaveSleepTimeoutPreference(root, "Default", mode, &error), "saves timeout");
        Check(LoadSleepTimeoutPreference(root, "Default") == mode, "loads timeout");
    }
    Check(NormalizeNeverSleepHost("WWW.Example.COM") == "example.com", "normalizes www and case");
    Check(NormalizeNeverSleepHost("dash-board.example") == "dash-board.example", "accepts DNS labels");
    Check(NormalizeNeverSleepHost("bad..example").empty(), "rejects empty DNS label");
    Check(NormalizeNeverSleepHost("https://example.com").empty(), "rejects URL instead of host");
    const std::vector<std::string> hosts{"Example.com", "chat.example.com"};
    Check(SaveNeverSleepHostsPreference(root, "Default", SleepTimeout::ThirtyMinutes, hosts, &error), "saves never-sleep hosts");
    const auto loaded_hosts = LoadNeverSleepHostsPreference(root, "Default");
    Check(loaded_hosts.size() == 2, "loads never-sleep hosts");
    Check(ContainsNeverSleepHost(loaded_hosts, "WWW.EXAMPLE.COM"), "never-sleep match normalized");
    Check(LoadSleepTimeoutPreference(root, "Default") == SleepTimeout::ThirtyMinutes, "saving hosts preserves timeout");
    Check(SaveSleepTimeoutPreference(root, "Default", SleepTimeout::FifteenMinutes, &error), "updates timeout with hosts present");
    Check(LoadNeverSleepHostsPreference(root, "Default").size() == 2, "saving timeout preserves hosts");
    Check(SaveNeverSleepHostsPreference(root, "Default", SleepTimeout::FifteenMinutes, {"dup.example", "WWW.DUP.EXAMPLE", "bad..host"}, &error), "saves normalized host list");
    Check(LoadNeverSleepHostsPreference(root, "Default").size() == 1, "deduplicates and drops invalid hosts");
    std::vector<std::string> many_hosts;
    for (int i = 0; i < 70; ++i) many_hosts.push_back("site" + std::to_string(i) + ".example");
    Check(SaveNeverSleepHostsPreference(root, "Default", SleepTimeout::FifteenMinutes, many_hosts, &error), "saves bounded host list");
    Check(LoadNeverSleepHostsPreference(root, "Default").size() == kMaxNeverSleepHosts, "never-sleep host hard cap enforced");
    Check(!SaveSleepTimeoutPreference(root, "../bad", SleepTimeout::FiveMinutes, &error), "rejects unsafe profile path");
    Check(!SaveSleepTimeoutPreference(root, "CON", SleepTimeout::FiveMinutes, &error), "rejects Windows device profile name");
    std::filesystem::remove_all(root, ec);
    std::cout << (checks - failures) << " performance preference checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
