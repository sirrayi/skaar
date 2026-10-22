// Skaar UI theme: a light, greyish taupe ground with graphite ink and one
// pastel-red accent, the colour of the targets. See docs/plan-visual.md.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

namespace skaar::ui {

struct Color {
    float r, g, b, a;
    constexpr Color Alpha(float alpha) const { return {r, g, b, a * alpha}; }
};

constexpr Color Rgb(int r, int g, int b, float a = 1.0f) {
    return {r / 255.0f, g / 255.0f, b / 255.0f, a};
}

namespace theme {

// Colour: off-white ground, graphite ink, one accent. The names are from the
// earlier dark theme and kept so callers don't change: kStage is the ground,
// kCream (and its Soft/Faint steps) is the ink.
constexpr Color kStage = Rgb(230, 227, 221);
constexpr Color kCream = Rgb(35, 35, 35);
constexpr Color kCreamSoft = kCream.Alpha(0.62f);
constexpr Color kCreamFaint = kCream.Alpha(0.34f);
constexpr Color kHairline = kCream.Alpha(0.08f);
constexpr Color kHairlineStrong = kCream.Alpha(0.14f);
constexpr Color kGrid = kCream.Alpha(0.025f);
constexpr Color kEmber = Rgb(234, 150, 146);  // pastel chalk red: targets, the selected item, PBs

// Layout, in virtual units on a 1080-tall canvas.
constexpr float kMargin = 120.0f;
constexpr float kGridStep = 64.0f;

// Type sizes (em, virtual units) and tracking (fraction of the em).
constexpr float kStatementSize = 112.0f;   // Montserrat 200, uppercase
constexpr float kTitleSize = 76.0f;        // Montserrat 200
constexpr float kItemSize = 38.0f;         // Montserrat 300, uppercase
constexpr float kLedeSize = 28.0f;         // Montserrat 300
constexpr float kMonoSize = 17.0f;         // JetBrains Mono, uppercase metadata
constexpr float kHudMonoSize = 20.0f;

constexpr float kTrackStatement = -0.01f;
constexpr float kTrackItem = 0.12f;
constexpr float kTrackMono = 0.12f;

}  // namespace theme
}  // namespace skaar::ui
