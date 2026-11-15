#include "home_preferences.h"

#include <filesystem>
#include <iostream>

namespace {
int checks = 0;
int failures = 0;
void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
}

int main() {
    using namespace webcom;
    HomePreferences prefs;
    std::string error;
    Check(AddHomeShortcut(prefs, {"OpenAI", "https://openai.com/"}, &error), "adds valid shortcut");
    Check(!AddHomeShortcut(prefs, {"Duplicate", "https://openai.com/"}, &error), "rejects duplicate URL");
    Check(!AddHomeShortcut(prefs, {"Bad", "javascript:alert(1)"}, &error), "rejects unsafe scheme");
    Check(HideHomeRecentHost(prefs, "Example.COM"), "hides normalized host");
    Check(IsHomeRecentHostHidden(prefs, "example.com"), "hidden lookup is case insensitive");
    Check(!HideHomeRecentHost(prefs, "example.com"), "does not duplicate hidden host");
    Check(ShowHomeRecentHost(prefs, "EXAMPLE.com"), "can unhide host");
    Check(!IsHomeRecentHostHidden(prefs, "example.com"), "unhide removes host");

    const auto root = std::filesystem::temp_directory_path() / "webcom-home-preferences-tests";
    std::error_code ec; std::filesystem::remove_all(root, ec);
    Check(SaveHomePreferences(root, "Default", prefs, &error), "saves preferences");
    const auto loaded = LoadHomePreferences(root, "Default");
    Check(loaded.shortcuts.size() == 1 && loaded.shortcuts[0].title == "OpenAI", "loads shortcut");
    Check(loaded.hidden_recent_hosts.empty(), "loads hidden hosts");
    auto edited = loaded;
    Check(RemoveHomeShortcut(edited, 0), "removes shortcut");
    Check(!RemoveHomeShortcut(edited, 0), "rejects out-of-range removal");
    std::filesystem::remove_all(root, ec);

    std::cout << (checks - failures) << " home preference checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
