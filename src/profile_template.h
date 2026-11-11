#pragma once

#include "appearance.h"
#include <filesystem>
#include <string>

namespace webcom {

struct ProfileTemplate {
    Appearance appearance;
    bool save_passwords = true;
    bool general_autofill = true;
    bool vertical_tabs = false;
    bool vertical_tabs_collapsed = false;
};

// Portable profile-setup exchange. This format deliberately excludes profile
// keys, URLs/tabs, cookies, history, passwords and other browsing data. It only
// carries user-facing browser preferences that are safe to apply to another
// profile.
bool ExportProfileTemplate(const std::filesystem::path& destination,
                           const ProfileTemplate& value,
                           std::string* error = nullptr);
bool ImportProfileTemplate(const std::filesystem::path& source,
                           ProfileTemplate& value,
                           std::string* error = nullptr);

} // namespace webcom
