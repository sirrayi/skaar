// Server game module: vmMain dispatch, level init and frame.
//
// This file is part of Skaar. GPLv2 or later.
#include <cstdarg>
#include <cstdio>
#include <exception>

#include "local.h"
#include "shared/move_ruleset.h"

namespace sg {

std::array<gentity_t, MAX_GENTITIES> g_entities{};
std::array<gclient_t, MAX_CLIENTS> g_clients{};
Level level;
Cvars cvars;

namespace {

void RegisterCvars() {
    trap::CvarRegister(&cvars.speed, "g_speed", "320", 0);
    trap::CvarRegister(&cvars.gravity, "g_gravity", "800", 0);
    // SYSTEMINFO: synced to clients so prediction runs the same fixed steps
    trap::CvarRegister(&cvars.pmoveFixed, "pmove_fixed", "1", CVAR_SYSTEMINFO);
    trap::CvarRegister(&cvars.pmoveMsec, "pmove_msec", "8", CVAR_SYSTEMINFO);
    trap::CvarRegister(&cvars.cheats, "sv_cheats", "", 0);
    // movement ruleset id: rulesets/<id>.json or a built-in (vq3, cpm)
    trap::CvarRegister(&cvars.movement, "g_movement", "vq3", CVAR_SERVERINFO | CVAR_ARCHIVE);

    vmCvar_t unused;
    trap::CvarRegister(&unused, "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH);
    trap::CvarRegister(&unused, "gamename", "skaar", CVAR_SERVERINFO | CVAR_ROM);
}

// Points level.ruleset at g_movement and tells clients its values, so
// prediction runs the same physics.
void SelectRuleset() {
    int index = skaar::move::FindRuleset(cvars.movement.string);
    if (index < 0) {
        Com_Printf(S_COLOR_YELLOW "WARNING: g_movement: no ruleset '%s', using vq3\n", cvars.movement.string);
        index = skaar::move::FindRuleset("vq3");
    }
    level.ruleset = index;
    trap::SetConfigstring(skaar::move::kConfigstringRuleset, skaar::move::EncodeRuleset(index).c_str());
}

void LoadRulesets() {
    std::vector<std::string> errors;
    skaar::move::LoadRulesets(trap::FileList, [](const std::string& path, std::string& text) {
        return trap::FileRead(path.c_str(), text);
    }, errors);
    for (const std::string& e : errors) {
        Com_Printf(S_COLOR_YELLOW "WARNING: rulesets/%s\n", e.c_str());
    }
}

void InitGame(int levelTime, int randomSeed, int restart) {
    (void)randomSeed;
    level = Level{};
    level.time = levelTime;
    level.startTime = levelTime;
    g_entities = {};
    g_clients = {};

    RegisterCvars();

    char mapname[MAX_QPATH];
    trap_Cvar_VariableStringBuffer("mapname", mapname, sizeof(mapname));
    level.mapname = mapname;

    // clients occupy the first MAX_CLIENTS entity slots
    level.numEntities = MAX_CLIENTS;
    g_entities[ENTITYNUM_WORLD].s.number = ENTITYNUM_WORLD;
    g_entities[ENTITYNUM_WORLD].classname = "worldspawn";
    LinkGameData();

    trap::SetConfigstring(CS_GAME_VERSION, GAME_VERSION);
    trap::SetConfigstring(CS_LEVEL_START_TIME, skaar::Va("%i", level.startTime));
    LoadRulesets();
    SelectRuleset();

    SpawnEntitiesFromString();

    Com_DPrintf("sgame: init %s%s, %zu spawn points, pmove %s %d ms\n", level.mapname.c_str(),
                restart ? " (restart)" : "", level.spawns.size(), cvars.pmoveFixed.integer ? "fixed" : "variable",
                cvars.pmoveMsec.integer);
}

void ShutdownGame(int restart) {
    Com_DPrintf("sgame: shutdown%s\n", restart ? " (restart)" : "");
}

void RunFrame(int levelTime) {
    level.previousTime = level.time;
    level.time = levelTime;

    trap::CvarUpdate(&cvars.speed);
    trap::CvarUpdate(&cvars.gravity);
    trap::CvarUpdate(&cvars.pmoveFixed);
    trap::CvarUpdate(&cvars.pmoveMsec);
    trap::CvarUpdate(&cvars.cheats);

    const int movementCount = cvars.movement.modificationCount;
    trap::CvarUpdate(&cvars.movement);
    if (cvars.movement.modificationCount != movementCount) {
        SelectRuleset();
    }
}

intptr_t Dispatch(int command, int arg0, int arg1, int arg2) {
    switch (command) {
        case GAME_INIT:
            InitGame(arg0, arg1, arg2);
            return 0;
        case GAME_SHUTDOWN:
            ShutdownGame(arg0);
            return 0;
        case GAME_CLIENT_CONNECT:
            return reinterpret_cast<intptr_t>(ClientConnect(arg0, arg1 != 0, arg2 != 0));
        case GAME_CLIENT_BEGIN:
            ClientBegin(arg0);
            return 0;
        case GAME_CLIENT_USERINFO_CHANGED:
            ClientUserinfoChanged(arg0);
            return 0;
        case GAME_CLIENT_DISCONNECT:
            ClientDisconnect(arg0);
            return 0;
        case GAME_CLIENT_COMMAND:
            ClientCommand(arg0);
            return 0;
        case GAME_CLIENT_THINK:
            ClientThink(arg0);
            return 0;
        case GAME_RUN_FRAME:
            RunFrame(arg0);
            return 0;
        case GAME_CONSOLE_COMMAND:
            return 0;  // no server console commands yet
        case BOTAI_START_FRAME:
            return 0;  // no bots until P6
        default:
            Com_Printf(S_COLOR_YELLOW "WARNING: sgame: unknown vmMain command %i\n", command);
            return -1;
    }
}

}  // namespace

void LinkGameData() {
    trap::LocateGameData(g_entities.data(), level.numEntities, sizeof(gentity_t), &g_clients[0].ps, sizeof(gclient_t));
}

}  // namespace sg

SKAAR_MODULE_EXPORT intptr_t vmMain(int command, int arg0, int arg1, int arg2, int, int, int, int, int, int, int,
                                    int, int) {
    // C++ exceptions must never unwind into the engine
    try {
        return sg::Dispatch(command, arg0, arg1, arg2);
    } catch (const std::exception& e) {
        Com_Error(ERR_DROP, "sgame: %s", e.what());
    }
}

extern "C" void QDECL Com_Printf(const char* fmt, ...) {
    va_list ap;
    char text[skaar::kMaxPrint];
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    sg::trap::Print(text);
}

extern "C" void QDECL Com_DPrintf(const char* fmt, ...) {
    if (!sg::trap::CvarInt("developer")) {
        return;
    }
    va_list ap;
    char text[skaar::kMaxPrint];
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    sg::trap::Print(text);
}

extern "C" void QDECL Com_Error(int level, const char* fmt, ...) {
    (void)level;
    va_list ap;
    char text[skaar::kMaxPrint];
    va_start(ap, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    sg::trap::Error(text);
}
