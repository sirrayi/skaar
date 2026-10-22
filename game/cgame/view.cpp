// Per-frame client flow: snapshots -> prediction -> camera -> world -> HUD.
//
// This file is part of Skaar. GPLv2 or later.
#include <cmath>

#include "local.h"
#include "trainer.h"

namespace cg {
namespace {

// cg_fov is the horizontal FOV of the actual window; derive the vertical one
void CalcFov() {
    float fovX = cvars.fov.value;
    fovX = fovX < 10 ? 10 : fovX > 150 ? 150 : fovX;
    const float aspect = static_cast<float>(cg.refdef.height) / cg.refdef.width;
    const float fovY = 2.0f * std::atan(std::tan(DEG2RAD(fovX) * 0.5f) * aspect);
    cg.refdef.fov_x = fovX;
    cg.refdef.fov_y = RAD2DEG(fovY);
}

void CalcViewValues() {
    refdef_t& rd = cg.refdef;
    rd = refdef_t{};
    rd.width = cg.glconfig.vidWidth;
    rd.height = cg.glconfig.vidHeight;
    rd.time = cg.time;

    const playerState_t& ps = cg.predicted;
    VectorCopy(ps.origin, rd.vieworg);
    rd.vieworg[2] += ps.viewheight;
    VectorCopy(ps.viewangles, cg.viewAngles);

    // fade out the last prediction error instead of snapping
    if (cvars.errorDecay.value > 0) {
        const int t = cg.time - cg.predictedErrorTime;
        const float f = (cvars.errorDecay.value - t) / cvars.errorDecay.value;
        if (f > 0 && f < 1) {
            VectorMA(rd.vieworg, f, cg.predictedError, rd.vieworg);
        } else {
            VectorClear(cg.predictedError);
        }
    }

    AnglesToAxis(cg.viewAngles, rd.viewaxis);
    std::memcpy(rd.areamask, cg.snap->areamask, sizeof(rd.areamask));
    CalcFov();
}

void AddEntities() {
    // P1: nothing besides the world. Other players arrive later; targets via the trainer.
    trainer::AddEntities();
}

}  // namespace

void DrawActiveFrame(int serverTime, bool /*demoPlayback*/) {
    cg.oldTime = cg.time;
    cg.time = serverTime;
    cg.frametime = cg.time - cg.oldTime;
    if (cg.frametime < 0) {
        cg.frametime = 0;
    }
    cg.realTime = trap::Milliseconds();

    // mouse sensitivity scale (zoom later); without this the engine ignores the mouse
    trap::SetUserCmdValue(0, 1.0f);

    ProcessSnapshots();
    if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
        return;  // the engine draws the loading screen meanwhile
    }

    PredictPlayerState();
    CalcViewValues();

    trap::R_ClearScene();
    AddEntities();
    trap::R_RenderScene(&cg.refdef);

    trainer::DrawHud();
    DrawHud();
}

}  // namespace cg
