// Snapshot flow: keep `snap` (the one cg.time is past) and `nextSnap` (the
// one it's heading to) so entities can be interpolated between them, and run
// the server commands each snapshot carries. A port of Q3's cg_snapshot.c.
//
// This file is part of Skaar. GPLv2 or later.
#include "local.h"
#include "weapons.h"
#include "shared/move_ruleset.h"

namespace cg {
namespace {

void ServerCommand() {
    const std::string cmd = trap::Argv(0);

    if (cmd == "cp") {
        cg.centerPrint = trap::Argv(1);
        cg.centerPrintTime = cg.time;
    } else if (cmd == "cs") {
        // the engine already updated its copy; refresh ours
        trap::GetGameState(&cg.gameState);
        if (atoi(trap::Argv(1).c_str()) == skaar::move::kConfigstringRuleset) {
            SyncRuleset();
        }
    } else if (cmd == "print" || cmd == "chat") {
        Com_Printf("%s", trap::Argv(1).c_str());
    } else if (cmd == "map_restart") {
        cg.validPredicted = false;
        cg.centerPrint.clear();
        cg.levelStartTime = atoi(ConfigString(CS_LEVEL_START_TIME));
        Com_DPrintf("cgame: map_restart\n");
    } else if (cmd == "disconnect") {
        // the engine handles the actual disconnect
    } else {
        Com_DPrintf("cgame: unknown server command '%s'\n", cmd.c_str());
    }
}

void ExecuteNewServerCommands(int latestSequence) {
    while (cg.serverCommandSequence < latestSequence) {
        if (trap::GetServerCommand(++cg.serverCommandSequence)) {
            ServerCommand();
        }
    }
}

// The buffer not holding cg.snap
snapshot_t* FreeBuffer() { return cg.snap == &cg.snapshots[0] ? &cg.snapshots[1] : &cg.snapshots[0]; }

snapshot_t* ReadNextSnapshot() {
    while (cg.processedSnapshotNum < cg.latestSnapshotNum) {
        snapshot_t* dest = FreeBuffer();
        cg.processedSnapshotNum++;
        if (trap::GetSnapshot(cg.processedSnapshotNum, dest)) {
            return dest;
        }
        // dropped from the engine's buffer (huge hitch); keep going
        Com_DPrintf("cgame: snapshot %d dropped\n", cg.processedSnapshotNum);
    }
    return nullptr;
}

// Fire an entity's event once (Q3's CG_CheckEvents). Temp event entities
// (eType > ET_EVENTS) carry the event in their type and fire on arrival.
void CheckEvents(CEntity& cent) {
    if (cent.current.number == cg.snap->ps.clientNum) {
        return;  // the local player's events come from the playerState
    }
    int event;
    if (cent.current.eType > ET_EVENTS) {
        if (cent.previousEvent) {
            return;
        }
        cent.previousEvent = 1;
        event = cent.current.eType - ET_EVENTS;
    } else {
        if (cent.current.event == cent.previousEvent) {
            return;
        }
        cent.previousEvent = cent.current.event;
        event = cent.current.event & ~EV_EVENT_BITS;
        if (!event) {
            return;
        }
    }
    weapons::EntityEvent(cent.current, event);
}

void ResetEntity(CEntity& cent) {
    VectorCopy(cent.current.pos.trBase, cent.lerpOrigin);
    VectorCopy(cent.current.apos.trBase, cent.lerpAngles);
}

void SetInitialSnapshot(snapshot_t* snap) {
    cg.snap = snap;
    // nothing was predicted before this, so there's no error to smooth out
    cg.thisFrameTeleport = true;
    BG_PlayerStateToEntityState(&snap->ps, &entities[snap->ps.clientNum].current, qfalse);
    ExecuteNewServerCommands(snap->serverCommandSequence);

    for (int i = 0; i < snap->numEntities; i++) {
        const entityState_t& es = snap->entities[i];
        CEntity& cent = entities[es.number];
        cent.current = es;
        cent.interpolate = false;
        cent.valid = true;
        cent.previousEvent = 0;
        ResetEntity(cent);
        CheckEvents(cent);
    }
    Com_DPrintf("cgame: first snapshot, server time %d, %d entities\n", snap->serverTime, snap->numEntities);
}

void SetNextSnap(snapshot_t* snap) {
    cg.nextSnap = snap;
    BG_PlayerStateToEntityState(&snap->ps, &entities[snap->ps.clientNum].next, qfalse);
    entities[snap->ps.clientNum].interpolate = true;

    for (int i = 0; i < snap->numEntities; i++) {
        const entityState_t& es = snap->entities[i];
        CEntity& cent = entities[es.number];
        cent.next = es;
        // no lerping across teleports or for entities that just appeared
        cent.interpolate = cent.valid && !((cent.current.eFlags ^ es.eFlags) & EF_TELEPORT_BIT);
    }

    // a teleport (or respawn) on the local player: don't smooth across it
    cg.nextFrameTeleport = ((cg.snap->ps.eFlags ^ snap->ps.eFlags) & EF_TELEPORT_BIT) != 0 ||
                           cg.snap->ps.clientNum != snap->ps.clientNum;
}

void TransitionSnapshot() {
    ExecuteNewServerCommands(cg.nextSnap->serverCommandSequence);

    for (int i = 0; i < cg.snap->numEntities; i++) {
        entities[cg.snap->entities[i].number].valid = false;
    }

    snapshot_t* old = cg.snap;
    cg.snap = cg.nextSnap;
    BG_PlayerStateToEntityState(&cg.snap->ps, &entities[cg.snap->ps.clientNum].current, qfalse);
    entities[cg.snap->ps.clientNum].interpolate = false;

    for (int i = 0; i < cg.snap->numEntities; i++) {
        CEntity& cent = entities[cg.snap->entities[i].number];
        if (!cent.valid && !cent.interpolate) {
            cent.previousEvent = 0;  // a new entity (or a reused slot): its events are new
        }
        cent.current = cent.next;
        cent.valid = true;
        if (!cent.interpolate) {
            ResetEntity(cent);
        }
        CheckEvents(cent);
    }

    cg.nextSnap = nullptr;
    cg.thisFrameTeleport = cg.nextFrameTeleport ||
                           ((old->ps.eFlags ^ cg.snap->ps.eFlags) & EF_TELEPORT_BIT) != 0;
    cg.nextFrameTeleport = false;
}

}  // namespace

void ProcessSnapshots() {
    int n;
    trap::GetCurrentSnapshotNumber(&n, &cg.latestSnapshotTime);
    if (n != cg.latestSnapshotNum) {
        if (n < cg.latestSnapshotNum) {
            Com_Error(ERR_DROP, "cgame: snapshot number went backwards");
        }
        cg.latestSnapshotNum = n;
    }

    // wait for the first active snapshot
    while (!cg.snap) {
        snapshot_t* snap = ReadNextSnapshot();
        if (!snap) {
            return;
        }
        if (!(snap->snapFlags & SNAPFLAG_NOT_ACTIVE)) {
            SetInitialSnapshot(snap);
        }
    }

    for (;;) {
        if (!cg.nextSnap) {
            snapshot_t* snap = ReadNextSnapshot();
            if (!snap) {
                break;  // extrapolating past the newest snapshot until another arrives
            }
            SetNextSnap(snap);
            if (cg.nextSnap->serverTime < cg.snap->serverTime) {
                Com_Error(ERR_DROP, "cgame: snapshot server time went backwards");
            }
        }
        if (cg.time >= cg.snap->serverTime && cg.time < cg.nextSnap->serverTime) {
            break;
        }
        TransitionSnapshot();
    }

    if (cg.nextSnap) {
        const int delta = cg.nextSnap->serverTime - cg.snap->serverTime;
        cg.frameInterpolation = delta > 0 ? static_cast<float>(cg.time - cg.snap->serverTime) / delta : 0.0f;
    } else {
        cg.frameInterpolation = 0.0f;
    }
}

}  // namespace cg
