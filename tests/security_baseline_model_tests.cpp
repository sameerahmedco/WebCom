#include "security_baseline_model.h"

#include <cstdlib>
#include <iostream>

namespace {
int checks = 0;
int failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
}

int main() {
    const auto policy = webcom::DefaultSecurityBaselinePolicy();
    Check(policy.safe_dll_search, "safe DLL search is required");
    Check(policy.terminate_on_heap_corruption, "heap corruption terminates the host");
    Check(policy.reject_remote_images, "remote DLL images are rejected");
    Check(policy.reject_low_integrity_images, "low-integrity DLL images are rejected");
    Check(policy.prefer_system32_images, "System32 is preferred for system DLL resolution");
    std::cout << checks << " security baseline checks, " << failures << " failures\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
