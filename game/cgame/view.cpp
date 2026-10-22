// Per-frame client flow: snapshots -> prediction -> camera -> world -> HUD.
//
// This file is part of Skaar. GPLv2 or later.
#include <cmath>

#include "local.h"
#include "trainer.h"
#include "weapons.h"

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

    trainer::OverrideView(rd.vieworg, cg.viewAngles);  // replays drive the camera
    weapons::ViewOffset(rd.vieworg, cg.viewAngles);     // tac recoil: camera punch only (skaar-07)
    AnglesToAxis(cg.viewAngles, rd.viewaxis);
    std::memcpy(rd.areamask, cg.snap->areamask, sizeof(rd.areamask));
    CalcFov();
}

// Position entities for this frame: interpolate between the two snapshots
// when possible, else follow the trajectory from the current one
void LerpEntity(CEntity& cent) {
    if (cent.interpolate && cg.nextSnap) {
        vec3_t a, b;
        BG_EvaluateTrajectory(&cent.current.pos, cg.snap->serverTime, a);
        BG_EvaluateTrajectory(&cent.next.pos, cg.nextSnap->serverTime, b);
        for (int i = 0; i < 3; i++) {
            cent.lerpOrigin[i] = a[i] + cg.frameInterpolation * (b[i] - a[i]);
        }
        BG_EvaluateTrajectory(&cent.current.apos, cg.snap->serverTime, a);
        BG_EvaluateTrajectory(&cent.next.apos, cg.nextSnap->serverTime, b);
        for (int i = 0; i < 3; i++) {
            cent.lerpAngles[i] = LerpAngle(a[i], b[i], cg.frameInterpolation);
        }
    } else {
        BG_EvaluateTrajectory(&cent.current.pos, cg.time, cent.lerpOrigin);
        BG_EvaluateTrajectory(&cent.current.apos, cg.time, cent.lerpAngles);
    }
}

void AddEntities() {
    for (int i = 0; i < cg.snap->numEntities; i++) {
        CEntity& cent = entities[cg.snap->entities[i].number];
        if (cent.current.number == cg.snap->ps.clientNum) {
            continue;
        }
        LerpEntity(cent);
        weapons::AddPacketEntity(cent);
    }
    trainer::AddEntities();
}

// Local player events, each once: the predicted ones (ps.events, in order of
// eventSequence) and the server's external events
void PlayerEvents() {
    const playerState_t& ps = cg.predicted;
    if (ps.eventSequence < cg.lastPlayerEventSeq || ps.eventSequence - cg.lastPlayerEventSeq > MAX_PS_EVENTS) {
        cg.lastPlayerEventSeq = ps.eventSequence - MAX_PS_EVENTS;  // reset (respawn, map restart) or a long gap
        if (cg.lastPlayerEventSeq < 0) {
            cg.lastPlayerEventSeq = 0;
        }
    }
    for (int seq = cg.lastPlayerEventSeq; seq < ps.eventSequence; seq++) {
        const int slot = seq & (MAX_PS_EVENTS - 1);
        weapons::PlayerEvent(ps.events[slot] & ~EV_EVENT_BITS, ps.eventParms[slot], ps);
    }
    cg.lastPlayerEventSeq = ps.eventSequence;

    const playerState_t& snap = cg.snap->ps;
    if (snap.externalEvent && snap.externalEventTime != cg.lastExternalEventTime) {
        cg.lastExternalEventTime = snap.externalEventTime;
        weapons::PlayerEvent(snap.externalEvent & ~EV_EVENT_BITS, snap.externalEventParm, snap);
    }
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

    // selected weapon (goes into every usercmd) and mouse sensitivity scale
    // (zoom later); without this call the engine ignores the mouse
    trap::SetUserCmdValue(weapons::SelectedWeapon(), 1.0f);

    ProcessSnapshots();
    if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
        return;  // the engine draws the loading screen meanwhile
    }

    PredictPlayerState();
    PlayerEvents();
    CalcViewValues();

    trap::R_ClearScene();
    AddEntities();
    trap::R_RenderScene(&cg.refdef);

    trainer::DrawHud();
    weapons::DrawHud();
    DrawHud();
}

}  // namespace cg
