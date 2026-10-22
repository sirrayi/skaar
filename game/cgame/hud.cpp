// HUD: crosshair, speed, mode line, center print. Drawn with the shared kit
// in the Skaar theme (docs/ui-style.md); crosshair options grow in P2.
//
// This file is part of Skaar. GPLv2 or later.
#include <cmath>

#include "../shared/draw2d.h"
#include "local.h"
#include "trainer.h"

namespace cg {
namespace {

using namespace skaar::ui;

constexpr int kCenterPrintMsec = 3000;

void DrawCrosshair() {
    const float cx = Width() / 2, cy = Height() / 2;
    const float len = cvars.crosshairSize.value, gap = cvars.crosshairGap.value;
    const float t = 2.0f;  // thickness
    const Color c = theme::kCream;
    Fill(cx - gap - len, cy - t / 2, len, t, c);
    Fill(cx + gap, cy - t / 2, len, t, c);
    Fill(cx - t / 2, cy - gap - len, t, len, c);
    Fill(cx - t / 2, cy + gap, t, len, c);
}

void DrawSpeed() {
    const playerState_t& ps = cg.predicted;
    const float ups = std::sqrt(ps.velocity[0] * ps.velocity[0] + ps.velocity[1] * ps.velocity[1]);
    GetFonts().text.Draw(Width() / 2, Height() - 72, 26, skaar::Va("%.0f", ups), theme::kCreamSoft, 0.02f,
                         Align::kCenter);
}

void DrawModeLine() {
    // bottom-left, quiet: map, clock, flags
    const int ms = cg.time - cg.levelStartTime;
    const int secs = ms > 0 ? ms / 1000 : 0;
    std::string line = std::string(cg.mapname) + skaar::Va("   %d:%02d", secs / 60, secs % 60);
    if (cg.predicted.pm_type == PM_NOCLIP) {
        line += "   noclip";
    }
    GetFonts().text.Draw(theme::kMargin / 2, Height() - 48, 18, line, theme::kCreamFaint, 0.02f);
}

void DrawCenterPrint() {
    if (cg.centerPrint.empty() || cg.time - cg.centerPrintTime > kCenterPrintMsec) {
        return;
    }
    const float fade = 1.0f - static_cast<float>(cg.time - cg.centerPrintTime) / kCenterPrintMsec;
    GetFonts().text.Draw(Width() / 2, Height() * 0.3f, 30, cg.centerPrint,
                         theme::kCream.Alpha(std::fmin(1.0f, fade * 3)), 0.0f, Align::kCenter);
}

bool ReadFileForKit(const char* path, std::string& out) { return trap::FileRead(path, out); }

}  // namespace

void InitHud() {
    Backend backend{};
    backend.registerShader = trap::R_RegisterShader;
    backend.setColor = trap::R_SetColor;
    backend.drawStretchPic = trap::R_DrawStretchPic;
    backend.readFile = ReadFileForKit;
    if (!Init(backend, cg.glconfig.vidWidth, cg.glconfig.vidHeight)) {
        Com_Printf(S_COLOR_YELLOW "WARNING: cgame: HUD fonts missing (run tools/gen_assets.py)\n");
    }
}

void DrawHud() {
    // menus draw on top; keep the HUD out from under them
    if (!cvars.drawHud.integer || (trap::Key_GetCatcher() & KEYCATCH_UI)) {
        return;
    }
    if (!trainer::Active()) {
        if (cvars.drawCrosshair.integer) {
            DrawCrosshair();
        }
        if (cvars.drawSpeed.integer) {
            DrawSpeed();
        }
        DrawModeLine();
    }
    DrawCenterPrint();
}

}  // namespace cg
