#include "storage.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

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

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto parent = std::filesystem::temp_directory_path();
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::random_device random;
        for (unsigned attempt = 0; attempt < 100; ++attempt) {
            const auto candidate = parent / ("webcom-storage-tests-" +
                std::to_string(stamp) + "-" + std::to_string(random()) + "-" +
                std::to_string(attempt));
            std::error_code ec;
            if (std::filesystem::create_directory(candidate, ec)) {
                path_ = candidate;
                return;
            }
            if (ec && ec != std::errc::file_exists)
                throw std::runtime_error("Cannot create test directory: " + ec.message());
        }
        throw std::runtime_error("Cannot allocate a unique test directory.");
    }

    ~TemporaryDirectory() {
        // Only a directory successfully created by this object can be removed.
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
            Check(!ec, "temporary test data is cleaned up");
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void Write(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("Cannot open test fixture for writing.");
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.flush();
    if (!file) throw std::runtime_error("Cannot write test fixture.");
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open test fixture for reading.");
    const std::string contents{std::istreambuf_iterator<char>(file),
                               std::istreambuf_iterator<char>()};
    if (file.bad()) throw std::runtime_error("Cannot read test fixture.");
    return contents;
}

bool IsDefaultNewTab(const webcom::AppState& state) {
    return state.active_profile == "Default" && state.profiles.size() == 1 &&
        state.profiles[0].name == "Default" && state.profiles[0].active_tab == 0 &&
        state.profiles[0].tabs.size() == 1 &&
        state.profiles[0].tabs[0].url == "webcom:newtab";
}

std::string Document(std::string_view url, std::string_view title = "A page",
                     std::string_view activeIndex = "0") {
    using webcom::EscapeField;
    return "WEBCOM_STATE\t1\nsettings\tbalanced\t1\tWork\nprofile\tWork\t" +
        std::string(activeIndex) + "\ntab\t0\t0\t" + EscapeField(url) + "\t" +
        EscapeField(title) + "\nendprofile\nend\n";
}

void RunTests() {
    using namespace webcom;
    TemporaryDirectory temporary;
    // Never call DataDirectory() or operate on the user's real WebCom folder.
    const auto root = temporary.path() / "session";
    const auto stateFile = root / "state.v1";
    Check(IsDefaultNewTab(LoadState(root)), "missing file starts with a native new tab");

    AppState state;
    state.memory_mode = MemoryMode::Saver;
    state.active_profile = "WORK";
    state.save_passwords = false;
    const std::string unicodeTitle = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
                                     "\tTitle\n100%\r\\";
    state.profiles = {
        {"Default", {}, 0, {}, true, true, {{"Main",0}}, "Main", false, false, StartupMode::Continue, {}},
        {"Work", {
            {"https://example.com/a?q=%20&b=1", unicodeTitle, true, true, "Research", "Focus"},
            {"http://localhost:8080/", "Local development", false, false, "", "Main"}}, 1, {}, true, true,
            {{"Main",0}}, "Main", false, false, StartupMode::Continue, {}}
    };
    state.profiles[1].groups = {{"Research", 4, true}};
    state.profiles[1].workspaces = {{"Main", 0}, {"Focus", 4}};
    state.profiles[1].active_workspace = "Main";
    state.profiles[1].vertical_tabs = true;
    state.profiles[1].vertical_tabs_collapsed = true;
    state.profiles[1].startup_mode = StartupMode::SpecificPages;
    state.profiles[1].startup_pages = {"https://example.com/start", "https://example.org/"};
    state.profiles[0].save_passwords = true;
    state.profiles[0].general_autofill = false;
    state.profiles[1].save_passwords = false;
    state.profiles[1].general_autofill = true;
    std::string error = "old error";
    Check(SaveState(root, state, &error), "valid state saves into a new directory");
    Check(error.empty(), "successful save clears an old error");
    const auto loaded = LoadState(root);
    Check(loaded.active_profile == "Work", "active profile casing resolves to its stored name");
    Check(loaded.memory_mode == MemoryMode::Saver && !loaded.save_passwords,
          "preferences round trip");
    Check(loaded.profiles.size() == 2, "all normal profiles round trip");
    if (loaded.profiles.size() == 2) {
        Check(loaded.profiles[0].tabs.size() == 1 &&
              loaded.profiles[0].tabs[0].url == "webcom:newtab",
              "an empty profile retains a new tab");
        Check(loaded.profiles[1].active_tab == 1 && loaded.profiles[1].tabs.size() == 2,
              "active tab and tab count round trip");
        if (loaded.profiles[1].tabs.size() == 2) {
            Check(loaded.profiles[1].tabs[0].title == unicodeTitle,
                  "Unicode, delimiters, percent, and backslash round trip in titles");
            Check(loaded.profiles[1].tabs[0].pinned &&
                  loaded.profiles[1].tabs[0].keep_awake,
                  "pinned and keep-awake flags round trip");
            Check(loaded.profiles[1].tabs[0].url == state.profiles[1].tabs[0].url,
                  "URL percent escapes are decoded only once");
            Check(loaded.profiles[1].tabs[0].group == "Research",
                  "tab group name round trips in version 3 state");
            Check(loaded.profiles[1].tabs[0].workspace == "Focus" && loaded.profiles[1].tabs[1].workspace == "Main",
                  "tab workspaces round trip in version 5 state");
        }
        Check(loaded.profiles[1].groups.size()==1 && loaded.profiles[1].groups[0].name=="Research" &&
              loaded.profiles[1].groups[0].color==4 && loaded.profiles[1].groups[0].collapsed,
              "tab group color and collapsed state round trip");
        Check(loaded.profiles[0].save_passwords && !loaded.profiles[0].general_autofill &&
              !loaded.profiles[1].save_passwords && loaded.profiles[1].general_autofill,
              "password-save and general-autofill preferences round trip per profile in version 4 state");
        Check(loaded.profiles[1].workspaces.size() == 2 && loaded.profiles[1].active_workspace == "Main" &&
              loaded.profiles[1].vertical_tabs && loaded.profiles[1].vertical_tabs_collapsed,
              "workspace metadata and vertical-tab rail preferences round trip in version 6 state");
        Check(loaded.profiles[1].startup_mode == StartupMode::SpecificPages &&
              loaded.profiles[1].startup_pages.size() == 2 &&
              loaded.profiles[1].startup_pages[0] == "https://example.com/start" &&
              loaded.profiles[1].startup_pages[1] == "https://example.org/",
              "startup mode and specific pages round trip in version 7 state");
    }

    // Version 3 had one app-wide password-save flag and no general-autofill field.
    // Migration applies that legacy flag to each profile and keeps form autofill on.
    const auto legacyV3 = root / "legacy-v3";
    std::filesystem::create_directories(legacyV3);
    Write(legacyV3 / "state.v1",
        "WEBCOM_STATE\t3\nsettings\tbalanced\t0\tDefault\n"
        "profile\tDefault\t0\n"
        "tab\t0\t0\twebcom%3Anewtab\tNew%20tab\t\n"
        "endprofile\nend\n");
    const auto migratedV3 = LoadState(legacyV3);
    Check(migratedV3.profiles.size() == 1 && !migratedV3.profiles[0].save_passwords &&
          migratedV3.profiles[0].general_autofill,
          "version 3 credential preferences migrate into the profile");


    // Version 4 had profile-local credential preferences but no workspace metadata.
    // Migration creates a Main workspace and keeps horizontal tabs.
    const auto legacyV4 = root / "legacy-v4";
    std::filesystem::create_directories(legacyV4);
    Write(legacyV4 / "state.v1",
        "WEBCOM_STATE\t4\nsettings\tbalanced\t1\tDefault\n"
        "profile\tDefault\t0\t1\t1\n"
        "tab\t0\t0\twebcom%3Anewtab\tNew%20tab\t\n"
        "endprofile\nend\n");
    const auto migratedV4 = LoadState(legacyV4);
    Check(migratedV4.profiles.size() == 1 && migratedV4.profiles[0].workspaces.size() == 1 &&
          migratedV4.profiles[0].workspaces[0].name == "Main" && migratedV4.profiles[0].active_workspace == "Main" &&
          !migratedV4.profiles[0].vertical_tabs && migratedV4.profiles[0].tabs[0].workspace == "Main",
          "version 4 state migrates into the Main workspace with horizontal tabs");


    // Version 5 had workspaces and vertical tabs but no persisted compact rail.
    const auto legacyV5 = root / "legacy-v5";
    std::filesystem::create_directories(legacyV5);
    Write(legacyV5 / "state.v1",
        "WEBCOM_STATE\t5\nsettings\tbalanced\t1\tDefault\n"
        "profile\tDefault\t0\t1\t1\tMain\t1\n"
        "workspace\tMain\t0\n"
        "tab\t0\t0\twebcom%3Anewtab\tNew%20tab\t\tMain\n"
        "endprofile\nend\n");
    const auto migratedV5 = LoadState(legacyV5);
    Check(migratedV5.profiles.size()==1 && migratedV5.profiles[0].vertical_tabs &&
          !migratedV5.profiles[0].vertical_tabs_collapsed,
          "version 5 vertical tabs migrate with the rail expanded");

    // Version 6 had no startup preference. Migration keeps the established
    // continue-where-you-left-off behavior and no configured startup pages.
    const auto legacyV6 = root / "legacy-v6";
    std::filesystem::create_directories(legacyV6);
    Write(legacyV6 / "state.v1",
        "WEBCOM_STATE\t6\nsettings\tbalanced\t1\tDefault\n"
        "profile\tDefault\t0\t1\t1\tMain\t1\t1\n"
        "workspace\tMain\t0\n"
        "tab\t0\t0\twebcom%3Anewtab\tNew%20tab\t\tMain\n"
        "endprofile\nend\n");
    const auto migratedV6 = LoadState(legacyV6);
    Check(migratedV6.profiles.size() == 1 &&
          migratedV6.profiles[0].startup_mode == StartupMode::Continue &&
          migratedV6.profiles[0].startup_pages.empty(),
          "version 6 state migrates to continue startup mode");

    const auto baseline = Read(stateFile);
    Check(std::filesystem::exists(root / "state.v1.bak"),
          "a successful save seeds a recovery generation");
    Write(stateFile, "corrupt current session");
    bool recoveredFromBackup = false;
    const auto recoveredState = LoadState(root, &recoveredFromBackup);
    Check(recoveredFromBackup && recoveredState.active_profile == "Work",
          "corrupt primary session falls back to the last known-good backup");
    Write(stateFile, baseline);

    // Storage accepts normal-session metadata only. Private-tab exclusion is a
    // caller contract, so app-level tests must verify that boundary. This test
    // verifies the other half: no WebView2 cookie/password store is inspected,
    // included in metadata, or changed by a save.
    const auto engineProfile = root / "UserData" / "Default";
    std::filesystem::create_directories(engineProfile);
    constexpr std::string_view cookieSentinel = "COOKIE_SECRET_NOT_SESSION_METADATA";
    constexpr std::string_view passwordSentinel = "PASSWORD_SECRET_NOT_SESSION_METADATA";
    Write(engineProfile / "Cookies", cookieSentinel);
    Write(engineProfile / "Login Data", passwordSentinel);
    Check(SaveState(root, state, &error), "save works beside unrelated runtime data");
    Check(Read(engineProfile / "Cookies") == cookieSentinel &&
          Read(engineProfile / "Login Data") == passwordSentinel,
          "runtime cookie and password files are untouched");
    Check(Read(stateFile).find(cookieSentinel) == std::string::npos &&
          Read(stateFile).find(passwordSentinel) == std::string::npos,
          "runtime private data is absent from session metadata");

    {
        const auto localRoot = root / "local-file-roundtrip";
        auto localState = state;
        localState.profiles[1].tabs[0].url = "file:///C:/Docs/WebCom%20Guide.html";
        localState.profiles[1].tabs[0].title = "Local guide";
        std::string localError;
        Check(SaveState(localRoot, localState, &localError), "supported local HTML can be saved in session metadata");
        const auto loadedLocal = LoadState(localRoot);
        Check(loadedLocal.profiles.size() > 1 && loadedLocal.profiles[1].tabs[0].url == localState.profiles[1].tabs[0].url,
              "supported local HTML survives state round trip");
        localState.profiles[1].tabs[0].url = "webcom:diagnostics";
        Check(SaveState(localRoot, localState, &localError), "current native management pages are restorable");
        localState.profiles[1].tabs[0].url = "webcom:recovery";
        Check(SaveState(localRoot, localState, &localError), "recovery management page is restorable");
    }

    const auto rejectSave = [&](const AppState& invalid, std::string_view label) {
        error.clear();
        Check(!SaveState(root, invalid, &error) && !error.empty(), label);
        Check(Read(stateFile) == baseline, "rejected save preserves the previous file exactly");
    };
    const std::string unsafeUrls[]{
        "javascript:alert(1)", "file:///C:/private.txt", "data:text/html,hello",
        "mailto:user@example.com", "chrome://settings",
        "search these words", "//example.com/path", "https://",
        "https://example.com\\@evil.example", "https://user:password@example.com/",
        "https://example.com/line\nbreak", " https://example.com/",
        "https://example.com/trailing "
    };
    for (const auto& url : unsafeUrls) {
        auto invalid = state;
        invalid.profiles[1].tabs[0].url = url;
        rejectSave(invalid, std::string("unsafe URL cannot be saved: ") + url);
    }
    auto invalid = state;
    invalid.profiles.push_back({"wOrK", {}, 0, {}, true, true, {{"Main",0}}, "Main", false, false, StartupMode::Continue, {}});
    rejectSave(invalid, "case-insensitive duplicate profile rejected");
    invalid = state; invalid.active_profile = "Missing";
    rejectSave(invalid, "missing active profile rejected");
    invalid = state; invalid.profiles[1].active_tab = 2;
    rejectSave(invalid, "out-of-range active tab rejected");
    invalid = state; invalid.profiles[0].active_tab = 1;
    rejectSave(invalid, "empty profile cannot carry a nonzero active index");
    invalid = state; invalid.profiles[1].tabs[0].title = std::string("\xC0\xAF", 2);
    rejectSave(invalid, "overlong UTF-8 rejected");
    invalid = state; invalid.profiles[1].tabs[0].title = std::string("\xED\xA0\x80", 3);
    rejectSave(invalid, "UTF-8 surrogate rejected");
    invalid = state; invalid.profiles[1].tabs[0].title = std::string(4097, 'x');
    rejectSave(invalid, "oversized title rejected");
    invalid = state; invalid.profiles[1].tabs[0].url = "https://example.com/" + std::string(16384, 'x');
    rejectSave(invalid, "oversized URL rejected");
    invalid = state; invalid.profiles[1].groups[0].color = 8;
    rejectSave(invalid, "invalid tab group color rejected");
    invalid = state; invalid.profiles[1].workspaces[1].color = 8;
    rejectSave(invalid, "invalid workspace color rejected");
    invalid = state; invalid.profiles[1].workspaces.push_back({"focus", 1});
    rejectSave(invalid, "case-insensitive duplicate workspace rejected");
    invalid = state; invalid.profiles[1].tabs[0].workspace = "Missing";
    rejectSave(invalid, "tab cannot reference a missing workspace");
    invalid = state; invalid.profiles[1].active_workspace = "Missing";
    rejectSave(invalid, "active workspace must exist");
    invalid = state; invalid.profiles[1].startup_mode = static_cast<StartupMode>(99);
    rejectSave(invalid, "invalid startup mode rejected");
    invalid = state; invalid.profiles[1].startup_pages = {"webcom:settings"};
    rejectSave(invalid, "internal page cannot be configured as a startup page");
    invalid = state; invalid.profiles[1].startup_pages = {"https://user:pass@example.com/"};
    rejectSave(invalid, "startup page credentials rejected");
    invalid = state; invalid.profiles[1].startup_pages = {"https://example.com/", "HTTPS://EXAMPLE.COM/"};
    rejectSave(invalid, "case-insensitive duplicate startup pages rejected");
    invalid = state; invalid.profiles[1].startup_pages.assign(17, "https://example.com/");
    rejectSave(invalid, "startup page count limit enforced");
    invalid = state; invalid.profiles[1].groups.push_back({"research", 1, false});
    rejectSave(invalid, "case-insensitive duplicate tab group rejected");
    invalid = state;
    invalid.profiles[1].tabs.assign(201, {"https://example.com/", "Page", false, false, ""});
    rejectSave(invalid, "per-profile tab limit enforced");
    invalid = state;
    for (unsigned i = 0; i < 31; ++i)
        invalid.profiles.push_back({"Profile " + std::to_string(i), {}, 0, {}, true, true, {{"Main",0}}, "Main", false, false, StartupMode::Continue, {}});
    rejectSave(invalid, "profile count limit enforced");
    invalid = state;
    for (auto& profile : invalid.profiles)
        profile.tabs.assign(200, {"https://example.com/", std::string(4096, ' '), false, false, ""});
    rejectSave(invalid, "encoded file size limit enforced before replacement");

    Check(!SaveState({}, state, &error) && !error.empty(), "empty directory rejected");
    Check(!SaveState(std::filesystem::path("relative-directory"), state, &error) &&
          !error.empty(), "relative directory rejected");
    Write(root / "obstructed", "test sentinel");
    Check(!SaveState(root / "obstructed" / "child", state, &error) && !error.empty(),
          "filesystem save failure returns an error");
    Check(Read(stateFile) == baseline, "filesystem failure preserves existing session");

    const auto rejectLoad = [&](std::string_view contents, std::string_view label) {
        std::error_code ec;
        std::filesystem::remove(root / "state.v1.bak", ec);
        Write(stateFile, contents);
        Check(IsDefaultNewTab(LoadState(root)), label);
    };
    for (const auto& url : unsafeUrls)
        rejectLoad(Document(url), std::string("unsafe restored URL falls back safely: ") + url);
    rejectLoad(Document("https://example.com/", "Title", "1"),
               "out-of-range restored index rejected");
    rejectLoad(Document("https://example.com/", "Title", "-1"),
               "negative restored index rejected");
    rejectLoad(Document("https://example.com/", std::string("\xF4\x90\x80\x80", 4)),
               "out-of-range Unicode rejected on load");
    rejectLoad(Document("https://example.com/", std::string("nul\0title", 9)),
               "encoded NUL rejected on load");
    Write(stateFile, "WEBCOM_STATE\t2\nsettings\tbalanced\t1\tWork\nprofile\tWork\t0\n"
         "tab\t0\t0\twebcom%3Asettings\tSettings\t\nendprofile\nend\n");
    const auto internal = LoadState(root);
    Check(internal.profiles.size()==1 && internal.profiles[0].tabs[0].url=="webcom:settings",
          "native settings page restores safely");
    Write(stateFile, "WEBCOM_STATE\t3\nsettings\tbalanced\t1\tWork\nprofile\tWork\t0\n"
         "group\tResearch\t6\t1\n"
         "tab\t0\t0\thttps%3A%2F%2Fexample.com%2F\tPage\tResearch\nendprofile\nend\n");
    const auto v3 = LoadState(root);
    Check(v3.profiles.size()==1 && v3.profiles[0].groups.size()==1 && v3.profiles[0].groups[0].color==6 && v3.profiles[0].groups[0].collapsed,
          "version 3 group metadata loads");
    auto corrupted = Document("https://example.com/");
    corrupted.replace(0, std::string("WEBCOM_STATE\t1").size(), "WEBCOM_STATE\t99");
    rejectLoad(corrupted, "unknown format version rejected");
    corrupted = Document("https://example.com/");
    corrupted.resize(corrupted.size() - 4);
    rejectLoad(corrupted, "missing end record rejected");
    rejectLoad(Document("https://example.com/") + "extra\n", "trailing garbage rejected");
    rejectLoad("WEBCOM_STATE\t1\nsettings\tbalanced\t1\tWork\nprofile\tWork\t0\n"
               "tab\t0\t0\thttps%3A%2F%2Fexample.com%2F\tbad%ZZ\nendprofile\nend\n",
               "malformed percent escape rejected");
    rejectLoad("WEBCOM_STATE\t1\nsettings\tbalanced\t1\tWork\nprofile\tWork\t0\n"
               "endprofile\nprofile\twork\t0\nendprofile\nend\n",
               "duplicate restored profile rejected ignoring case");
    rejectLoad(std::string(4 * 1024 * 1024 + 1, 'x'), "oversized state file rejected");
    rejectLoad("WEBCOM_STATE\t1\n" + std::string(100000, '\t'),
               "many malformed fields rejected with bounded parsing");
    rejectLoad({}, "empty file rejected safely");

    {
        AppState profiles = state;
        profiles.active_profile = "Work";
        Check(RemoveProfile(profiles,"work",&error), "profile removal is case-insensitive");
        Check(profiles.profiles.size()==1 && profiles.profiles[0].name=="Default",
              "profile removal deletes only the requested metadata");
        Check(profiles.active_profile=="Default", "deleting the active profile selects a surviving profile");
        Check(!RemoveProfile(profiles,"Default",&error) && !error.empty(),
              "the final profile cannot be deleted");

        AppState deletion_state = state;
        Check(SaveState(root,deletion_state,&error), "pre-deletion state saved for recovery test");
        Check(RemoveProfile(deletion_state,"Work",&error), "profile removed before destructive persistence");
        Check(SaveStateAfterProfileDeletion(root,deletion_state,&error), "profile deletion state persisted");
        Write(root / "state.v1", "corrupt");
        bool recovered_deleted = false;
        const auto recovered_after_delete = LoadState(root,&recovered_deleted);
        Check(recovered_deleted && recovered_after_delete.profiles.size()==1 && recovered_after_delete.profiles[0].name=="Default",
              "recovery backup cannot resurrect a deleted profile");
    }

    {
        AppState recovered = state;
        recovered.profiles[0].active_workspace = "Main";
        recovered.profiles[0].tabs = {{"https://example.com/one", "One", true, true, "", "Main"},
                                      {"https://example.com/two", "Two", false, false, "", "Main"}};
        recovered.profiles[0].groups = {{"Recovered", 2, true}};
        recovered.profiles[1].workspaces = {{"Main",0},{"Focus",3}};
        recovered.profiles[1].active_workspace = "Focus";
        recovered.profiles[1].tabs = {{"https://example.org/", "Work", false, false, "", "Focus"}};
        const bool work_passwords = recovered.profiles[1].save_passwords;
        const bool work_autofill = recovered.profiles[1].general_autofill;
        ResetRecoveredSessionTabs(recovered);
        Check(recovered.profiles[0].tabs.size()==1 && recovered.profiles[0].tabs[0].url=="webcom:newtab" &&
              recovered.profiles[0].groups.empty() && recovered.profiles[0].active_tab==0,
              "recovery reset replaces restored tabs and groups with one new tab");
        Check(recovered.profiles[1].tabs.size()==1 && recovered.profiles[1].tabs[0].workspace=="Focus" &&
              recovered.profiles[1].workspaces.size()==2 && recovered.profiles[1].save_passwords==work_passwords &&
              recovered.profiles[1].general_autofill==work_autofill,
              "recovery reset preserves workspaces and profile preferences");
        Check(SaveState(root,state,&error), "recovered source state saved before reset persistence test");
        Check(SaveStateAfterRecoveryReset(root,recovered,&error), "recovery reset state persisted with sanitized backup");
        Write(root / "state.v1", "corrupt");
        bool recovered_reset = false;
        const auto fallback_after_reset = LoadState(root,&recovered_reset);
        Check(recovered_reset && fallback_after_reset.profiles[0].tabs.size()==1 &&
              fallback_after_reset.profiles[0].tabs[0].url=="webcom:newtab",
              "recovery backup cannot resurrect discarded restored tabs");
    }

    Check(SaveState(root, AppState{}, &error) && IsDefaultNewTab(LoadState(root)),
          "empty default state has a usable new tab");
    unsigned temporaryFiles = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.path().filename().string().starts_with("state.v1.tmp-")) ++temporaryFiles;
    }
    Check(temporaryFiles == 0, "successful and rejected saves leave no temporary files");
}
}  // namespace

int main() {
    try {
        RunTests();
    } catch (const std::exception& error) {
        Check(false, std::string("unexpected test failure: ") + error.what());
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
