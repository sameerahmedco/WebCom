#pragma once

#include <cstddef>
#include <string_view>

namespace webcom {

enum class ExtensionViewFilter { All, Enabled, Disabled, NeedsAttention };

struct ExtensionViewState {
    bool enabled = false;
    bool built_in = false;
    bool source_recorded = false;
    bool source_manifest_valid = true;
    bool source_snapshot_available = true;
    bool source_changed = false;
    bool source_baseline_known = true;
};

struct ExtensionViewCounts {
    std::size_t all = 0;
    std::size_t enabled = 0;
    std::size_t disabled = 0;
    std::size_t attention = 0;
};

bool ExtensionNeedsAttention(const ExtensionViewState& state);
bool ExtensionMatchesFilter(const ExtensionViewState& state, ExtensionViewFilter filter);
bool ExtensionMatchesText(std::wstring_view name, std::wstring_view id, std::wstring_view source_path,
                          std::wstring_view query);
void AddExtensionViewCount(ExtensionViewCounts& counts, const ExtensionViewState& state);

} // namespace webcom
