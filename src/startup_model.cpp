#include "startup_model.h"

#include <algorithm>
#include <cwctype>
#include <string_view>

namespace webcom {

StartupPlan BuildStartupPlan(const SavedProfile& profile, bool force_recovered_session) {
    if (force_recovered_session || profile.startup_mode == StartupMode::Continue)
        return {true, {}};
    if (profile.startup_mode == StartupMode::SpecificPages && !profile.startup_pages.empty())
        return {false, profile.startup_pages};
    return {false, {"webcom:newtab"}};
}


bool IsAcceptanceDataRootAllowed(const std::filesystem::path& candidate,
                                 const std::filesystem::path& temp_root) {
    if (candidate.empty() || temp_root.empty() || !candidate.is_absolute() || !temp_root.is_absolute())
        return false;
    const auto normalized_candidate = candidate.lexically_normal();
    const auto normalized_temp = temp_root.lexically_normal();
    if (normalized_candidate == normalized_temp || normalized_candidate.parent_path().empty()) return false;

    auto candidate_parent = normalized_candidate.parent_path().wstring();
    auto expected_parent = normalized_temp.wstring();
#ifdef _WIN32
    std::transform(candidate_parent.begin(), candidate_parent.end(), candidate_parent.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    std::transform(expected_parent.begin(), expected_parent.end(), expected_parent.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
#endif
    if (candidate_parent != expected_parent) return false;

    const auto leaf = normalized_candidate.filename().wstring();
    constexpr std::wstring_view prefix = L"WebCom-Acceptance-";
    if (leaf.size() <= prefix.size() || leaf.substr(0, prefix.size()) != prefix) return false;
    for (wchar_t c : leaf.substr(prefix.size())) {
        if (!(std::iswalnum(c) || c == L'-' || c == L'_')) return false;
    }
    return true;
}

} // namespace webcom
