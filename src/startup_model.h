#pragma once

#include "storage.h"

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

} // namespace webcom
