#include "startup_model.h"

namespace webcom {

StartupPlan BuildStartupPlan(const SavedProfile& profile, bool force_recovered_session) {
    if (force_recovered_session || profile.startup_mode == StartupMode::Continue)
        return {true, {}};
    if (profile.startup_mode == StartupMode::SpecificPages && !profile.startup_pages.empty())
        return {false, profile.startup_pages};
    return {false, {"webcom:newtab"}};
}

} // namespace webcom
