#include "native_accessibility_model.h"

#include <limits>

namespace webcom {

long NativeAccessibleChildId(long base_child_count, std::size_t action_index) {
    if (base_child_count < 0) base_child_count = 0;
    const auto max_long = static_cast<std::size_t>(std::numeric_limits<long>::max());
    if (action_index >= max_long || static_cast<std::size_t>(base_child_count) > max_long - action_index - 1)
        return 0;
    return base_child_count + static_cast<long>(action_index) + 1;
}

std::optional<std::size_t> NativeAccessibleActionIndex(long base_child_count,
                                                       long child_id,
                                                       std::size_t action_count) {
    if (base_child_count < 0) base_child_count = 0;
    if (child_id <= base_child_count) return std::nullopt;
    const auto relative = static_cast<unsigned long long>(child_id - base_child_count - 1);
    if (relative >= action_count) return std::nullopt;
    return static_cast<std::size_t>(relative);
}

std::optional<std::size_t> NativeAccessibleNextAction(std::size_t current,
                                                      std::size_t action_count,
                                                      bool forward) {
    if (!action_count || current >= action_count) return std::nullopt;
    if (forward) {
        if (current + 1 >= action_count) return std::nullopt;
        return current + 1;
    }
    if (!current) return std::nullopt;
    return current - 1;
}

} // namespace webcom
