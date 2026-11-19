#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "security_bootstrap_win32.h"
#include "security_baseline_model.h"

namespace webcom {
namespace {

void AppendError(std::wstring* error, const wchar_t* text) {
    if (!error) return;
    if (!error->empty()) *error += L" ";
    *error += text;
}

} // namespace

bool ApplyProcessSecurityBaseline(ProcessSecurityBaselineResult* output, std::wstring* error) {
    const auto policy = DefaultSecurityBaselinePolicy();
    ProcessSecurityBaselineResult result{};

    // Avoid removable/network/current-directory DLL probing for any later dynamic loads.
    if (policy.safe_dll_search) {
        result.safe_dll_search = SetDefaultDllDirectories(
            LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
            LOAD_LIBRARY_SEARCH_SYSTEM32 |
            LOAD_LIBRARY_SEARCH_USER_DIRS) != FALSE;
        if (!result.safe_dll_search) AppendError(error, L"Windows could not enable WebCom's safe DLL search policy.");
    }

    // Modern Windows already terminates on detected heap corruption; requesting it
    // explicitly documents and preserves the fail-closed expectation for the host.
    if (policy.terminate_on_heap_corruption) {
        result.heap_corruption_termination = HeapSetInformation(
            nullptr, HeapEnableTerminationOnCorruption, nullptr, 0) != FALSE;
        if (!result.heap_corruption_termination) AppendError(error, L"Windows could not enable heap-corruption termination.");
    }

    PROCESS_MITIGATION_IMAGE_LOAD_POLICY image_policy{};
    image_policy.NoRemoteImages = policy.reject_remote_images ? 1u : 0u;
    image_policy.NoLowMandatoryLabelImages = policy.reject_low_integrity_images ? 1u : 0u;
    image_policy.PreferSystem32Images = policy.prefer_system32_images ? 1u : 0u;
    result.image_load_policy = SetProcessMitigationPolicy(
        ProcessImageLoadPolicy, &image_policy, sizeof(image_policy)) != FALSE;
    if (!result.image_load_policy) AppendError(error, L"Windows could not enable WebCom's image-load mitigation policy.");

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    if (output) *output = result;
    return result.safe_dll_search && result.heap_corruption_termination && result.image_load_policy;
}

} // namespace webcom

#endif
