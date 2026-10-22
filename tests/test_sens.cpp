// This file is part of Skaar. GPLv2 or later.
#include "check.h"
#include "shared/sens.h"

using namespace skaar::sens;

TEST("cm/360 and conversions match the published formulas") {
    // 800 dpi, sens 2 at yaw 0.022: 360 / 0.044 / 800 * 2.54 = 25.977 cm
    CHECK_NEAR(CmPer360(2, 0.022f, 800), 25.977, 1e-3);
    CHECK_NEAR(SensitivityFor(25.977f, 0.022f, 800), 2.0, 1e-3);
    // community reference: a 0.07-yaw game at 0.4 equals 1.2727 at 0.022 (x 3.1818)
    CHECK_NEAR(Convert(0.4f, 0.07f), 1.2727, 1e-3);
    // 0.0066 at 5 equals 1.5 at 0.022 (x 0.3)
    CHECK_NEAR(Convert(5, 0.0066f), 1.5, 1e-4);
    // the same physical turn after converting
    for (float yaw : kYawPresets) {
        CHECK_NEAR(CmPer360(Convert(3, yaw), kOurYaw, 1600), CmPer360(3, yaw, 1600), 1e-3);
    }
    CHECK(CmPer360(0, 0.022f, 800) == 0 && SensitivityFor(30, 0.022f, 0) == 0);
}
