#pragma once

#include "core.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace webcom {

enum class StartupMode : std::uint8_t { Continue = 0, NewTab = 1, SpecificPages = 2 };

struct SavedTab {
    std::string url;
    std::string title;
    bool pinned = false;
    bool keep_awake = false;
    std::string group;
    std::string workspace = "Main";
};

struct SavedTabGroup {
    std::string name;
    std::uint8_t color = 0;
    bool collapsed = false;
};

struct SavedWorkspace {
    std::string name = "Main";
    std::uint8_t color = 0;
};

struct SavedProfile {
    std::string name;
    std::vector<SavedTab> tabs;
    std::size_t active_tab = 0;
    std::vector<SavedTabGroup> groups;
    // Credential/form preferences are profile-local in WebView2. Version 4
    // session metadata persists the host preference so separate WebCom
    // profile windows no longer overwrite one another's setting.
    bool save_passwords = true;
    bool general_autofill = true;
    std::vector<SavedWorkspace> workspaces{{"Main", 0}};
    std::string active_workspace = "Main";
    bool vertical_tabs = false;
    bool vertical_tabs_collapsed = false;
    StartupMode startup_mode = StartupMode::Continue;
    std::vector<std::string> startup_pages;
};

struct AppState {
    MemoryMode memory_mode = MemoryMode::Balanced;
    std::string active_profile = "Default";
    bool save_passwords = true;
    std::vector<SavedProfile> profiles;
};

// Returns %LOCALAPPDATA%/WebCom. Throws if Windows cannot resolve the known
// folder; never falls back to the application directory or current directory.
std::filesystem::path DataDirectory();

// Invalid, incomplete, oversized, or unsupported primary state falls back to
// the last validated recovery generation, then to a safe Default profile.
// recovered_from_backup is set only when state.v1.bak supplied the result.
AppState LoadState(const std::filesystem::path& root, bool* recovered_from_backup = nullptr);

// Removes one saved profile from in-memory session metadata. The last profile
// cannot be removed; if the active profile is deleted, another saved profile
// becomes active. This does not delete WebView2 browser data by itself.
bool RemoveProfile(AppState& state, std::string_view profile, std::string* error = nullptr);

// Drops only restored tab/group state after backup recovery. Profile identities,
// workspaces and preferences remain intact. Each profile receives one normal
// New Tab in its active workspace.
void ResetRecoveredSessionTabs(AppState& state);

// Only normal-profile metadata belongs here. The caller must omit private tabs.
// WebView2 owns cookies and password data; neither is read or written here.
// A successful save atomically replaces state.v1 after flushing the new file.
// On failure, leaves the previous state intact and optionally describes why.
bool SaveState(const std::filesystem::path& root, const AppState& state,
               std::string* error = nullptr);

// Persists a state after destructive profile removal and ensures the recovery
// generation cannot resurrect the deleted profile's saved metadata.
bool SaveStateAfterProfileDeletion(const std::filesystem::path& root, const AppState& state,
                                   std::string* error = nullptr);

// Persists a user-requested post-recovery reset twice so state.v1.bak cannot
// bring the discarded recovered tabs back on the next startup.
bool SaveStateAfterRecoveryReset(const std::filesystem::path& root, const AppState& state,
                                 std::string* error = nullptr);

}  // namespace webcom
