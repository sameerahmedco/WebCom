#pragma once

namespace webcom {

struct AccessibilityInputs {
    bool profile_reduce_motion = false;
    bool profile_high_contrast = false;
    bool system_animations_enabled = true;
    bool system_high_contrast = false;
    bool remote_session = false;
};

struct AccessibilityPolicy {
    bool reduce_motion = false;
    bool high_contrast = false;
};

AccessibilityPolicy ResolveAccessibilityPolicy(const AccessibilityInputs& inputs);

} // namespace webcom
