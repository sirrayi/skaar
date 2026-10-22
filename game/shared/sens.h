// Sensitivity math: cm per 360°, and conversion between games by yaw (the
// degrees one mouse count turns at sensitivity 1). Same physical turn per
// mouse distance = same feel, so a conversion keeps sensitivity × yaw equal.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

namespace skaar::sens {

constexpr float kCmPerInch = 2.54f;
constexpr float kOurYaw = 0.022f;  // m_yaw default

// Yaw values with community-documented standards (docs/sensitivity.md)
constexpr float kYawPresets[] = {0.022f, 0.07f, 0.0066f};

// cm of mouse travel for a full turn; 0 if any input is <= 0
inline float CmPer360(float sensitivity, float yaw, float dpi) {
    const float degPerCount = sensitivity * yaw;
    return degPerCount > 0 && dpi > 0 ? 360.0f / degPerCount / dpi * kCmPerInch : 0;
}

// The sensitivity that gives `cm` per 360 at this yaw and dpi
inline float SensitivityFor(float cm, float yaw, float dpi) {
    return cm > 0 && yaw > 0 && dpi > 0 ? 360.0f * kCmPerInch / (cm * dpi * yaw) : 0;
}

// Another game's sensitivity in ours (same dpi): keep sensitivity × yaw
inline float Convert(float theirSensitivity, float theirYaw, float ourYaw = kOurYaw) {
    return ourYaw > 0 ? theirSensitivity * theirYaw / ourYaw : 0;
}

}  // namespace skaar::sens
