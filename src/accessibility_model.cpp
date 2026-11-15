#include "accessibility_model.h"

namespace webcom {

AccessibilityPolicy ResolveAccessibilityPolicy(const AccessibilityInputs& inputs) {
    AccessibilityPolicy policy;
    policy.reduce_motion = inputs.profile_reduce_motion || !inputs.system_animations_enabled || inputs.remote_session;
    policy.high_contrast = inputs.profile_high_contrast || inputs.system_high_contrast;
    return policy;
}

} // namespace webcom
