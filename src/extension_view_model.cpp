#include "extension_view_model.h"

#include <algorithm>
#include <cwctype>

namespace webcom {

bool ExtensionNeedsAttention(const ExtensionViewState& state) {
    if (state.built_in) return false;
    if (!state.source_recorded) return true;
    if (!state.source_manifest_valid || !state.source_snapshot_available || state.source_changed || !state.source_baseline_known)
        return true;
    return false;
}

bool ExtensionMatchesFilter(const ExtensionViewState& state, ExtensionViewFilter filter) {
    switch (filter) {
    case ExtensionViewFilter::All: return true;
    case ExtensionViewFilter::Enabled: return state.enabled;
    case ExtensionViewFilter::Disabled: return !state.enabled;
    case ExtensionViewFilter::NeedsAttention: return ExtensionNeedsAttention(state);
    }
    return true;
}

bool ExtensionMatchesText(std::wstring_view name, std::wstring_view id, std::wstring_view source_path,
                          std::wstring_view query) {
    if (query.empty()) return true;
    const auto contains_ci = [query](std::wstring_view haystack) {
        return std::search(haystack.begin(), haystack.end(), query.begin(), query.end(),
            [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); }) != haystack.end();
    };
    return contains_ci(name) || contains_ci(id) || contains_ci(source_path);
}

void AddExtensionViewCount(ExtensionViewCounts& counts, const ExtensionViewState& state) {
    ++counts.all;
    if (state.enabled) ++counts.enabled; else ++counts.disabled;
    if (ExtensionNeedsAttention(state)) ++counts.attention;
}

} // namespace webcom
