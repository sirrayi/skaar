// Settings: aim (sensitivity in cm/360, conversion from other games' yaw,
// FOV), crosshair (with a live preview), video and audio.
//
// This file is part of Skaar. GPLv2 or later.
#include <cmath>
#include <cstdio>

#include "../shared/reticle.h"
#include "local.h"

namespace ui {
namespace {

constexpr float kRowStep = 64;
constexpr float kInchCm = 2.54f;

float CmPer360(float sensitivity, float yaw, float dpi) {
    const float degPerCount = sensitivity * yaw;
    return degPerCount > 0 && dpi > 0 ? 360.0f / degPerCount / dpi * kInchCm : 0;
}

void DrawAim(float top) {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin;
    const float sens = trap::CvarValue("sensitivity");
    const float yaw = trap::CvarValue("m_yaw") > 0 ? trap::CvarValue("m_yaw") : 0.022f;
    float dpi = trap::CvarValue("sk_dpi");
    if (dpi <= 0) {
        dpi = 800;
    }
    const float cm = CmPer360(sens, yaw, dpi);
    float y = top;
    float v = 0;

    if (NumberRow(y, "Sensitivity", sens, 0.001f, 100, "%.3f", v)) {
        trap::CvarSet("sensitivity", skaar::Va("%g", v));
    }
    y += kRowStep;
    if (NumberRow(y, "Mouse DPI", dpi, 50, 64000, "%.0f", v)) {
        trap::CvarSet("sk_dpi", skaar::Va("%g", v));
    }
    y += kRowStep;
    if (NumberRow(y, "cm / 360°", cm, 1, 500, "%.2f", v)) {
        // sensitivity giving v cm per 360 at this dpi
        trap::CvarSet("sensitivity", skaar::Va("%.4f", 360.0f * kInchCm / (v * dpi * yaw)));
    }
    y += kRowStep;

    // conversion from another game: same deg/count = same feel
    static const std::vector<std::pair<std::string, std::string>> yaws = {
        {"0.022", "Yaw 0.022"}, {"0.07", "Yaw 0.07"}, {"0.0066", "Yaw 0.0066"}, {"0.005555", "Yaw 0.005555"},
        {"0.0225", "Yaw 0.0225"}};
    if (trap::CvarString("sk_convertYaw").empty()) {
        trap::CvarSet("sk_convertYaw", "0.022");
    }
    ChoiceRow(y, "Convert from", "sk_convertYaw", yaws, "THE OTHER GAME'S YAW");
    y += kRowStep;
    const float otherYaw = std::atof(trap::CvarString("sk_convertYaw").c_str());
    const float otherSens = trap::CvarValue("sk_convertSens");
    const float converted = otherYaw > 0 ? otherSens * otherYaw / yaw : 0;
    if (NumberRow(y, "Their sensitivity", otherSens, 0.0001f, 1000, "%.3f", v,
                  otherSens > 0 ? skaar::Va("= %.3f HERE  ·  %.2f CM/360", converted,
                                            CmPer360(converted, yaw, dpi))
                                : "")) {
        trap::CvarSet("sk_convertSens", skaar::Va("%g", v));
    }
    y += kRowStep;
    if (ActionButton({m, y + 14, 420, 56}, "Apply conversion", converted > 0 ? skaar::Va("→ %.3f", converted) : "")) {
        if (converted > 0) {
            trap::CvarSet("sensitivity", skaar::Va("%.4f", converted));
        }
    }
    y += kRowStep + 30;

    SliderRow(y, "Field of view", "cg_fov", 60, 130, 1, "%.0f°", "HORIZONTAL");
    y += kRowStep + 40;

    Label(m, y, "RAW INPUT, NO ACCELERATION   ·   CHECK A 360 WITH SENS_TEST", theme::kCreamFaint);
    (void)f;
}

void DrawCrosshairPreview(const Rect& r) {
    // a patch of range: the ground tinted a touch towards the ink, like the range walls
    kit::Fill(r.x, r.y, r.w, r.h, theme::kStage);
    kit::Fill(r.x, r.y, r.w, r.h, theme::kCream.Alpha(0.06f));
    kit::HLine(r.x, r.y, r.w, theme::kHairlineStrong);
    kit::HLine(r.x, r.Bottom(), r.w, theme::kHairlineStrong);
    kit::VLine(r.x, r.y, r.h, theme::kHairlineStrong);
    kit::VLine(r.Right(), r.y, r.h, theme::kHairlineStrong);
    // a target just off centre, as in a run
    chart::Disc(r.x + r.w / 2 + 70, r.y + r.h / 2 - 46, 26, kEmber);
    Label(r.x + 20, r.Bottom() - 20, "PREVIEW", theme::kCreamFaint);

    // the same reticle the game draws (game/shared/reticle.h), centred on a whole pixel
    const float s = kit::Scale();
    const float cx = std::floor((r.x + r.w / 2) * s) / s, cy = std::floor((r.y + r.h / 2) * s) / s;
    kit::DrawReticle(cx, cy, kit::ReticleFromCvars(trap::CvarString));
}

void DrawCrosshair(float top) {
    const float m = theme::kMargin, w = kit::Width();
    float y = top;
    ChoiceRow(y, "Style", "cg_crosshairStyle", {{"0", "Cross"}, {"1", "Dot"}, {"2", "Cross + dot"}});
    y += kRowStep;
    SliderRow(y, "Length", "cg_crosshairSize", 0, 16, 1, "%.0f");
    y += kRowStep;
    SliderRow(y, "Gap", "cg_crosshairGap", 0, 10, 1, "%.0f");
    y += kRowStep;
    SliderRow(y, "Thickness", "cg_crosshairThickness", 1, 6, 0.5f, "%.1f");
    y += kRowStep;
    ToggleRow(y, "Outline", "cg_crosshairOutline");
    y += kRowStep;
    ChoiceRow(y, "Colour", "cg_crosshairColor",
              {{"f7e08a", "Pastel yellow"}, {"ffffff", "White"}, {"232323", "Graphite"}, {"3cff7a", "Green"},
               {"3ce8ff", "Cyan"}, {"ff4df0", "Magenta"}});
    y += kRowStep;
    DrawCrosshairPreview({w - m - 420, top + kRowStep * 7 + 30, 420, 260});
    (void)y;
}

void DrawVideo(float top) {
    const float m = theme::kMargin;
    float y = top;
    if (ToggleRow(y, "Fullscreen", "r_fullscreen", "NEEDS APPLY")) {
        // fullscreen at the desktop resolution; windowed at the last custom size
        trap::CvarSet("r_mode", trap::CvarValue("r_fullscreen") != 0 ? "-2" : "-1");
    }
    y += kRowStep;
    ToggleRow(y, "Retina resolution", "r_allowHighDPI", "NEEDS APPLY");
    y += kRowStep;
    ToggleRow(y, "Vertical sync", "r_swapInterval", "NEEDS APPLY");
    y += kRowStep;
    ChoiceRow(y, "Frame rate cap", "com_maxfps",
              {{"0", "Uncapped"}, {"144", "144"}, {"240", "240"}, {"360", "360"}, {"500", "500"}, {"1000", "1000"}});
    y += kRowStep;
    SliderRow(y, "Brightness", "r_gamma", 0.8f, 1.6f, 0.05f, "%.2f", "");
    y += kRowStep + 30;
    if (ActionButton({m, y, 420, 64}, "Apply video changes", "RESTARTS VIDEO")) {
        Exec("vid_restart\n");
    }
}

void DrawAudio(float top) {
    float y = top;
    SliderRow(y, "Volume", "s_volume", 0, 1, 0.05f, "%.2f");
    y += kRowStep;
    ToggleRow(y, "Hit and kill sounds", "sk_hitSound");
}

}  // namespace

void DrawSettings() {
    const float m = theme::kMargin, w = kit::Width(), h = kit::Height();
    if (InGame()) {
        kit::Fill(0, 0, w, h, theme::kStage.Alpha(0.95f));
    } else {
        kit::Background(true);
    }
    Header("");

    struct T {
        const char* label;
        SettingsTab tab;
    };
    const T tabs[] = {{"Aim", SettingsTab::kAim},
                      {"Crosshair", SettingsTab::kCrosshair},
                      {"Video", SettingsTab::kVideo},
                      {"Audio", SettingsTab::kAudio}};
    float x = m;
    for (const T& t : tabs) {
        float tw = 0;
        if (Tab(x, 186, t.label, ui.settingsTab == t.tab, &tw)) {
            ui.settingsTab = t.tab;
        }
        x += tw + 44;
    }

    const float top = 240;
    switch (ui.settingsTab) {
        case SettingsTab::kAim:
            DrawAim(top);
            break;
        case SettingsTab::kCrosshair:
            DrawCrosshair(top);
            break;
        case SettingsTab::kVideo:
            DrawVideo(top);
            break;
        case SettingsTab::kAudio:
            DrawAudio(top);
            break;
    }
    // Left/Right on a row that doesn't take it (buttons): next/previous tab
    if (const int dir = TakeAdjust()) {
        const int n = 4, i = static_cast<int>(ui.settingsTab);
        ui.settingsTab = static_cast<SettingsTab>((i + dir + n) % n);
        ui.focus = 0;
        ResetMarker();
    }
    Footer(ui.editing >= 0 ? "ENTER  SAVE     ESC  CANCEL" : "← →  CHANGE     ESC  BACK");
}

}  // namespace ui
