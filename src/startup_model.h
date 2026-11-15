#pragma once

#include "storage.h"

#include <filesystem>
#include <string>
#include <vector>

namespace webcom {

struct StartupPlan {
    bool restore_saved_session = true;
    std::vector<std::string> urls;
};

// Produces the tab plan for a normal profile launch. Recovery is deliberately
// able to override the configured startup preference for the current run so a
// user who chose "Keep recovered tabs" actually sees the recovered session.
StartupPlan BuildStartupPlan(const SavedProfile& profile, bool force_recovered_session = false);

// QA launches may use an isolated data root, but only as a direct child of the
// caller-supplied temp directory and only with the reserved acceptance prefix.
bool IsAcceptanceDataRootAllowed(const std::filesystem::path& candidate,
                                 const std::filesystem::path& temp_root);

} // namespace webcom
