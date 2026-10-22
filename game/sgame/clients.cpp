// Client lifecycle and movement on the server: connect, spawn, think (pmove).
// The think/spawn flow follows Q3's g_client.c / g_active.c, minus everything
// the aim trainer doesn't need yet (items, damage, teams, bots).
//
// This file is part of Skaar. GPLv2 or later.
#include <cstring>

#include "local.h"

namespace sg {
namespace {

const vec3_t kPlayerMins = {-15, -15, MINS_Z};
const vec3_t kPlayerMaxs = {15, 15, 32};

gentity_t& EntityFor(int clientNum) { return g_entities[clientNum]; }

void SendPrint(int clientNum, const char* text) { trap::SendServerCommand(clientNum, skaar::Va("print \"%s\"", text)); }

void SetViewAngles(gclient_t& client, gentity_t& ent, const vec3_t angles) {
    // The client's command angles are absolute; delta_angles makes them start at `angles`
    for (int i = 0; i < 3; i++) {
        client.ps.delta_angles[i] = ANGLE2SHORT(angles[i]) - client.cmd.angles[i];
    }
    VectorCopy(angles, ent.s.angles);
    VectorCopy(angles, client.ps.viewangles);
}

void Spawn(int clientNum) {
    gclient_t& client = g_clients[clientNum];
    gentity_t& ent = EntityFor(clientNum);

    const SpawnPoint& spot = level.spawns[level.nextSpawn++ % level.spawns.size()];
    vec3_t origin;
    VectorCopy(spot.origin, origin);
    origin[2] += 9;  // as Q3: spawn slightly above the marker so the box doesn't start in the floor

    // a fresh player state, but keep the bits the engine owns
    const int ping = client.ps.ping;
    const int eFlags = client.ps.eFlags;
    std::memset(&client.ps, 0, sizeof(client.ps));
    client.ps.ping = ping;
    client.ps.clientNum = clientNum;
    client.ps.eFlags = (eFlags & EF_TELEPORT_BIT) ^ EF_TELEPORT_BIT;  // no interpolation from the old spot
    client.ps.stats[STAT_HEALTH] = client.ps.stats[STAT_MAX_HEALTH] = 100;
    client.ps.pm_type = PM_NORMAL;
    client.ps.weapon = WP_NONE;
    client.ps.viewheight = DEFAULT_VIEWHEIGHT;
    VectorCopy(origin, client.ps.origin);
    // brief knockback lock so a held key doesn't move the player on the spawn frame
    client.ps.pm_flags |= PMF_TIME_KNOCKBACK;
    client.ps.pm_time = 100;
    client.ps.commandTime = level.time - 100;

    ent.s.number = clientNum;
    ent.s.eType = ET_PLAYER;
    ent.client = &client;
    ent.inuse = true;
    ent.classname = "player";
    ent.r.contents = CONTENTS_BODY;
    ent.r.svFlags = 0;
    VectorCopy(kPlayerMins, ent.r.mins);
    VectorCopy(kPlayerMaxs, ent.r.maxs);
    VectorCopy(origin, ent.r.currentOrigin);

    trap::GetUsercmd(clientNum, &client.cmd);
    SetViewAngles(client, ent, spot.angles);

    client.spawned = true;
    BG_PlayerStateToEntityState(&client.ps, &ent.s, qtrue);
    trap::LinkEntity(&ent);

    Com_DPrintf("sgame: client %d spawned at (%.0f %.0f %.0f) yaw %.0f\n", clientNum, origin[0], origin[1],
                origin[2], spot.angles[YAW]);
}

bool CheatsAllowed(int clientNum) {
    if (cvars.cheats.integer) {
        return true;
    }
    SendPrint(clientNum, "Cheats are off on this server (start the map with devmap).\n");
    return false;
}

}  // namespace

const char* ClientConnect(int clientNum, bool firstTime, bool isBot) {
    if (isBot) {
        return "bots arrive in P6";
    }
    gclient_t& client = g_clients[clientNum];
    std::memset(&client, 0, sizeof(client));
    client.connected = true;
    ClientUserinfoChanged(clientNum);
    Com_DPrintf("sgame: client %d connected (%s)%s\n", clientNum, client.name, firstTime ? "" : ", reconnect");
    return nullptr;
}

void ClientBegin(int clientNum) {
    gentity_t& ent = EntityFor(clientNum);
    if (ent.r.linked) {
        trap::UnlinkEntity(&ent);
    }
    std::memset(&ent, 0, sizeof(ent));
    Spawn(clientNum);
}

void ClientUserinfoChanged(int clientNum) {
    gclient_t& client = g_clients[clientNum];
    const std::string info = trap::GetUserinfo(clientNum);
    char name[64];
    Q_strncpyz(name, Info_ValueForKey(info.c_str(), "name"), sizeof(name));
    Q_CleanStr(name);
    Q_strncpyz(client.name, name[0] ? name : "player", sizeof(client.name));
    trap::SetConfigstring(CS_PLAYERS + clientNum, skaar::Va("n\\%s", client.name));
}

void ClientDisconnect(int clientNum) {
    gentity_t& ent = EntityFor(clientNum);
    trap::UnlinkEntity(&ent);
    std::memset(&ent, 0, sizeof(ent));
    std::memset(&g_clients[clientNum], 0, sizeof(gclient_t));
    trap::SetConfigstring(CS_PLAYERS + clientNum, "");
    Com_DPrintf("sgame: client %d disconnected\n", clientNum);
}

void ClientCommand(int clientNum) {
    gclient_t& client = g_clients[clientNum];
    if (!client.spawned) {
        return;
    }
    const std::string cmd = trap::Argv(0);

    if (!Q_stricmp(cmd.c_str(), "noclip")) {
        if (CheatsAllowed(clientNum)) {
            client.noclip = !client.noclip;
            SendPrint(clientNum, client.noclip ? "noclip ON\n" : "noclip OFF\n");
        }
    } else if (!Q_stricmp(cmd.c_str(), "sk_freeze")) {
        // sent by the aim trainer for scenarios where the player doesn't move
        client.frozen = atoi(trap::Argv(1).c_str()) != 0;
        Com_DPrintf("sgame: client %d %s\n", clientNum, client.frozen ? "frozen" : "unfrozen");
    } else if (!Q_stricmp(cmd.c_str(), "kill") || !Q_stricmp(cmd.c_str(), "respawn") ||
               !Q_stricmp(cmd.c_str(), "sk_restart")) {
        // sk_restart reaches the server only when the cgame trainer didn't handle it
        // (no scenario loaded): fall back to a respawn
        Spawn(clientNum);
    } else {
        SendPrint(clientNum, skaar::Va("unknown command: %s\n", cmd.c_str()));
    }
}

void ClientThink(int clientNum) {
    gclient_t& client = g_clients[clientNum];
    gentity_t& ent = EntityFor(clientNum);
    if (!client.spawned) {
        return;
    }

    usercmd_t ucmd;
    trap::GetUsercmd(clientNum, &ucmd);
    client.cmd = ucmd;

    // clamp the command time into a sane window around the server's clock (as Q3)
    if (ucmd.serverTime > level.time + 200) {
        ucmd.serverTime = level.time + 200;
    }
    if (ucmd.serverTime < level.time - 1000) {
        ucmd.serverTime = level.time - 1000;
    }
    if (ucmd.serverTime - client.ps.commandTime < 1) {
        return;
    }

    int msec = cvars.pmoveMsec.integer;
    if (msec < 8 || msec > 33) {
        msec = msec < 8 ? 8 : 33;
        trap::CvarSet("pmove_msec", skaar::Va("%i", msec));
    }
    if (cvars.pmoveFixed.integer) {
        ucmd.serverTime = ((ucmd.serverTime + msec - 1) / msec) * msec;
    }

    client.ps.pm_type = client.noclip ? PM_NOCLIP : client.frozen ? PM_FREEZE : PM_NORMAL;
    client.ps.gravity = cvars.gravity.integer;
    client.ps.speed = cvars.speed.integer;

    pmove_t pm{};
    pm.ps = &client.ps;
    pm.cmd = ucmd;
    pm.tracemask = MASK_PLAYERSOLID;
    pm.trace = trap::Trace;
    pm.pointcontents = trap::PointContents;
    pm.noFootsteps = qtrue;
    pm.pmove_fixed = cvars.pmoveFixed.integer;
    pm.pmove_msec = msec;
    Pmove(&pm);

    BG_PlayerStateToEntityState(&client.ps, &ent.s, qtrue);
    VectorCopy(client.ps.origin, ent.r.currentOrigin);
    VectorCopy(pm.mins, ent.r.mins);
    VectorCopy(pm.maxs, ent.r.maxs);
    trap::LinkEntity(&ent);
}

}  // namespace sg
