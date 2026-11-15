#include "tab_batch_model.h"

#include <algorithm>
#include <unordered_set>

namespace webcom {
namespace {
std::string Key(const TabBatchItem& item) {
    // WebCom stores canonical navigation URLs. Keep path/query case intact because
    // some servers treat those components as case-sensitive.
    return item.url + (item.private_mode ? "\nprivate" : "\nnormal");
}
}

std::vector<std::uint64_t> DuplicateTabCloseCandidates(
    const std::vector<TabBatchItem>& items,
    std::uint64_t target_id,
    std::string_view workspace) {
    std::vector<std::uint64_t> result;
    const auto target = std::find_if(items.begin(), items.end(), [&](const auto& item) {
        return item.id == target_id;
    });
    if (target == items.end() || target->url.empty() || target->workspace != workspace) return result;
    const auto target_key = Key(*target);
    for (const auto& item : items) {
        if (item.id == target_id || item.workspace != workspace || Key(item) != target_key) continue;
        if (item.active || item.pinned || item.has_download) continue;
        result.push_back(item.id);
    }
    return result;
}

} // namespace webcom
