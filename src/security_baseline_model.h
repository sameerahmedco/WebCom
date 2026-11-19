#pragma once

namespace webcom {

struct SecurityBaselinePolicy {
    bool safe_dll_search = true;
    bool terminate_on_heap_corruption = true;
    bool reject_remote_images = true;
    bool reject_low_integrity_images = true;
    bool prefer_system32_images = true;
};

SecurityBaselinePolicy DefaultSecurityBaselinePolicy();

} // namespace webcom
