// The reticle: one implementation for the HUD, the trainer and the settings
// preview, so they can't drift apart. Driven by the cg_crosshair* cvars.
//
// Default: a small "+" (arms meeting in the middle) in pastel yellow, with a
// thin graphite outline so it reads on light and dark ground alike.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <string>

#include "theme.h"

namespace skaar::ui {

enum class ReticleStyle { kCross = 0, kDot = 1, kCrossDot = 2 };

struct Reticle {
    ReticleStyle style = ReticleStyle::kCross;
    float length = 4.0f;     // arm length from the centre line, virtual units (1080p)
    float gap = 0.0f;        // 0 = a solid plus
    float thickness = 2.0f;
    Color color = Rgb(247, 224, 138);  // pastel yellow, #f7e08a
    bool outline = true;
};

// Defaults as cvar strings, for registration and default.cfg
namespace reticle_defaults {
constexpr const char* kStyle = "0";
constexpr const char* kSize = "4";
constexpr const char* kGap = "0";
constexpr const char* kThickness = "2";
constexpr const char* kColor = "f7e08a";
constexpr const char* kOutline = "1";
}  // namespace reticle_defaults

// Reads cg_crosshairStyle/Size/Gap/Thickness/Color/Outline through the
// module's own cvar lookup (each module has different syscalls).
Reticle ReticleFromCvars(std::string (*cvarString)(const char* name));

// Draws centred on (cx, cy) in virtual units. The centre and every bar land on
// whole physical pixels so the arms stay symmetric at any resolution.
void DrawReticle(float cx, float cy, const Reticle& reticle);

// Centre of the screen, snapped to a physical pixel
void ScreenCentre(float* cx, float* cy);

}  // namespace skaar::ui
