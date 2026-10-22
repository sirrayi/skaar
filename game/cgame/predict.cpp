// Client-side prediction: rerun the user commands the server hasn't
// acknowledged yet through the same Pmove the server uses, so movement and
// view respond immediately. A simplified port of Q3's cg_predict.c.
//
// This file is part of Skaar. GPLv2 or later.
#include "local.h"
#include "shared/move_pmove.h"
#include "trainer.h"

namespace cg {
namespace {

void Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
           int /*skipNumber*/, int mask) {
    // world only for now; solid entities (targets, other players) come with P3
    trap::CM_BoxTrace(result, start, end, mins, maxs, 0, mask);
    result->entityNum = result->fraction != 1.0f ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
}

int PointContents(const vec3_t point, int /*passEntityNum*/) { return trap::CM_PointContents(point, 0); }

}  // namespace

void PredictPlayerState() {
    if (!cg.snap) {
        return;
    }

    if (!cg.validPredicted) {
        cg.predicted = cg.snap->ps;
        cg.validPredicted = true;
    }

    if (cvars.noPredict.integer || (cg.snap->ps.pm_flags & PMF_FOLLOW)) {
        cg.predicted = cg.snap->ps;
        return;
    }

    pmove_t pm{};
    pm.ps = &cg.predicted;
    pm.trace = Trace;
    pm.pointcontents = PointContents;
    pm.tracemask = MASK_PLAYERSOLID;
    pm.noFootsteps = qtrue;
    pm.pmove_fixed = cvars.pmoveFixed.integer;
    pm.pmove_msec = cvars.pmoveMsec.integer < 8 ? 8 : cvars.pmoveMsec.integer > 33 ? 33 : cvars.pmoveMsec.integer;

    const playerState_t old = cg.predicted;
    const int current = trap::GetCurrentCmdNumber();

    // if the oldest command we still have is newer than the snapshot, we lost input history
    usercmd_t oldest;
    trap::GetUserCmd(current - CMD_BACKUP + 1, &oldest);
    if (oldest.serverTime > cg.snap->ps.commandTime && oldest.serverTime < cg.time) {
        if (cvars.showMiss.integer) {
            Com_Printf("cgame: exceeded CMD_BACKUP\n");
        }
        return;
    }

    usercmd_t latest;
    trap::GetUserCmd(current, &latest);

    // start from the newest authoritative state
    if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport) {
        cg.predicted = cg.nextSnap->ps;
        cg.physicsTime = cg.nextSnap->serverTime;
    } else {
        cg.predicted = cg.snap->ps;
        cg.physicsTime = cg.snap->serverTime;
    }

    bool moved = false;
    for (int cmdNum = current - CMD_BACKUP + 1; cmdNum <= current; cmdNum++) {
        usercmd_t cmd;
        if (!trap::GetUserCmd(cmdNum, &cmd)) {
            continue;
        }
        if (pm.pmove_fixed) {
            skaar::move::UpdateViewAngles(pm.ps, &cmd);  // view follows the mouse every frame
        }
        if (cmd.serverTime <= cg.predicted.commandTime || cmd.serverTime > latest.serverTime) {
            continue;  // already in the snapshot, or not sent yet
        }

        // first command past the snapshot: measure how far the old prediction was off
        if (cg.predicted.commandTime == old.commandTime) {
            if (cg.thisFrameTeleport) {
                VectorClear(cg.predictedError);
                cg.thisFrameTeleport = false;
            } else {
                vec3_t delta;
                VectorSubtract(old.origin, cg.predicted.origin, delta);
                if (VectorLength(delta) > 0.1f) {
                    if (cvars.showMiss.integer) {
                        Com_Printf("cgame: prediction miss %.2f\n", VectorLength(delta));
                    }
                    if (cvars.errorDecay.integer) {
                        const int t = cg.time - cg.predictedErrorTime;
                        float f = (cvars.errorDecay.value - t) / cvars.errorDecay.value;
                        f = f < 0 ? 0 : f;
                        VectorScale(cg.predictedError, f, cg.predictedError);
                    } else {
                        VectorClear(cg.predictedError);
                    }
                    VectorAdd(delta, cg.predictedError, cg.predictedError);
                    cg.predictedErrorTime = cg.oldTime;
                }
            }
        }

        if (pm.pmove_fixed) {
            cmd.serverTime = ((cmd.serverTime + pm.pmove_msec - 1) / pm.pmove_msec) * pm.pmove_msec;
        }
        pm.cmd = cmd;
        skaar::move::Pmove(&pm);
        moved = true;

        // each command reaches the trainer once, the first time it's predicted
        if (cmdNum > cg.lastTrainerCmd) {
            cg.lastTrainerCmd = cmdNum;
            trainer::OnCommand(cmd, cg.predicted);
        }
    }

    if (!moved && cvars.showMiss.integer) {
        Com_Printf("cgame: not moved\n");
    }
}

}  // namespace cg
