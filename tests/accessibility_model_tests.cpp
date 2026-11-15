#include "accessibility_model.h"
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;
int checks = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
}

int main() {
    using webcom::AccessibilityInputs;
    using webcom::ResolveAccessibilityPolicy;

    auto policy = ResolveAccessibilityPolicy({});
    Check(!policy.reduce_motion, "default policy keeps chrome motion enabled");
    Check(!policy.high_contrast, "default policy keeps normal contrast");

    AccessibilityInputs profile_motion{};
    profile_motion.profile_reduce_motion = true;
    policy = ResolveAccessibilityPolicy(profile_motion);
    Check(policy.reduce_motion, "profile reduced-motion preference is honored");
    Check(!policy.high_contrast, "reduced motion does not force high contrast");

    AccessibilityInputs windows_motion{};
    windows_motion.system_animations_enabled = false;
    policy = ResolveAccessibilityPolicy(windows_motion);
    Check(policy.reduce_motion, "Windows animations-off forces reduced browser chrome motion");

    AccessibilityInputs remote{};
    remote.remote_session = true;
    policy = ResolveAccessibilityPolicy(remote);
    Check(policy.reduce_motion, "remote sessions suppress browser chrome motion");

    AccessibilityInputs profile_contrast{};
    profile_contrast.profile_high_contrast = true;
    policy = ResolveAccessibilityPolicy(profile_contrast);
    Check(policy.high_contrast, "profile high-contrast preference is honored");
    Check(!policy.reduce_motion, "profile contrast alone does not reduce motion");

    AccessibilityInputs windows_contrast{};
    windows_contrast.system_high_contrast = true;
    policy = ResolveAccessibilityPolicy(windows_contrast);
    Check(policy.high_contrast, "Windows High Contrast forces high-contrast browser chrome");

    AccessibilityInputs combined{};
    combined.profile_reduce_motion = false;
    combined.profile_high_contrast = false;
    combined.system_animations_enabled = false;
    combined.system_high_contrast = true;
    combined.remote_session = true;
    policy = ResolveAccessibilityPolicy(combined);
    Check(policy.reduce_motion && policy.high_contrast, "system accessibility policies compose safely");

    std::cout << checks << " accessibility policy checks, " << failures << " failures\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
