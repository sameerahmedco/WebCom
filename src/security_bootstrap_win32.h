#pragma once

#include <string>

namespace webcom {

struct ProcessSecurityBaselineResult {
    bool safe_dll_search = false;
    bool heap_corruption_termination = false;
    bool image_load_policy = false;
};

bool ApplyProcessSecurityBaseline(ProcessSecurityBaselineResult* result = nullptr,
                                  std::wstring* error = nullptr);

} // namespace webcom
