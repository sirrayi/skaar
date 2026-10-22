// skaar_tests: unit tests for the engine-free game logic in game/shared.
//
// This file is part of Skaar. GPLv2 or later.
#include "check.h"

int main() {
    for (const check::Case& c : check::Cases()) {
        const int before = check::Failures();
        c.fn();
        std::printf("%s %s\n", check::Failures() == before ? "ok  " : "FAIL", c.name);
    }
    std::printf("%zu tests, %d failed checks\n", check::Cases().size(), check::Failures());
    return check::Failures() ? 1 : 0;
}
