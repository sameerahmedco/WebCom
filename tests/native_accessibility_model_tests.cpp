#include "native_accessibility_model.h"

#include <iostream>
#include <limits>

using namespace webcom;

int main() {
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool value, const char* message) {
        ++checks;
        if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };

    check(NativeAccessibleChildId(5, 0) == 6, "first virtual action follows native children");
    check(NativeAccessibleChildId(5, 3) == 9, "virtual action IDs preserve action order");
    check(NativeAccessibleChildId(-1, 0) == 1, "negative base counts fail closed to zero");

    const auto first = NativeAccessibleActionIndex(5, 6, 4);
    check(first && *first == 0, "first virtual child resolves to action zero");
    const auto last = NativeAccessibleActionIndex(5, 9, 4);
    check(last && *last == 3, "last virtual child resolves correctly");
    check(!NativeAccessibleActionIndex(5, 5, 4), "standard Win32 child IDs are not treated as virtual actions");
    check(!NativeAccessibleActionIndex(5, 10, 4), "out-of-range virtual child IDs are rejected");
    check(!NativeAccessibleActionIndex(5, -1, 4), "negative child IDs are rejected");

    const auto next = NativeAccessibleNextAction(1, 4, true);
    check(next && *next == 2, "forward accessibility navigation advances one action");
    const auto previous = NativeAccessibleNextAction(2, 4, false);
    check(previous && *previous == 1, "backward accessibility navigation retreats one action");
    check(!NativeAccessibleNextAction(3, 4, true), "forward navigation stops at the last action");
    check(!NativeAccessibleNextAction(0, 4, false), "backward navigation stops at the first action");
    check(!NativeAccessibleNextAction(0, 0, true), "empty action lists cannot navigate");

    std::cout << checks << " native accessibility model checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
