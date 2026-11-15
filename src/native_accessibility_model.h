#pragma once

#include <cstddef>
#include <optional>

namespace webcom {

long NativeAccessibleChildId(long base_child_count, std::size_t action_index);
std::optional<std::size_t> NativeAccessibleActionIndex(long base_child_count,
                                                       long child_id,
                                                       std::size_t action_count);
std::optional<std::size_t> NativeAccessibleNextAction(std::size_t current,
                                                      std::size_t action_count,
                                                      bool forward);

} // namespace webcom
