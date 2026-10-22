// Parse the map's entity lump. P1 knows worldspawn and player spawn points;
// everything else is logged and ignored.
//
// This file is part of Skaar. GPLv2 or later.
#include <cstdlib>
#include <map>
#include <string>

#include "local.h"

namespace sg {
namespace {

using EntityKeys = std::map<std::string, std::string>;

// Reads one { "key" "value" ... } block. Returns false at the end of the lump.
bool ReadEntity(EntityKeys& keys) {
    char token[MAX_TOKEN_CHARS];
    if (!trap::GetEntityToken(token, sizeof(token))) {
        return false;
    }
    if (token[0] != '{') {
        Com_Error(ERR_DROP, "sgame: entity lump: expected '{', got '%s'", token);
    }
    keys.clear();
    for (;;) {
        char key[MAX_TOKEN_CHARS];
        if (!trap::GetEntityToken(key, sizeof(key))) {
            Com_Error(ERR_DROP, "sgame: entity lump: unexpected end");
        }
        if (key[0] == '}') {
            return true;
        }
        if (!trap::GetEntityToken(token, sizeof(token))) {
            Com_Error(ERR_DROP, "sgame: entity lump: key '%s' has no value", key);
        }
        keys[key] = token;
    }
}

void ReadVector(const EntityKeys& keys, const char* key, vec3_t out) {
    VectorClear(out);
    auto it = keys.find(key);
    if (it != keys.end()) {
        std::sscanf(it->second.c_str(), "%f %f %f", &out[0], &out[1], &out[2]);
    }
}

void SpawnPlayerStart(const EntityKeys& keys) {
    SpawnPoint spot{};
    ReadVector(keys, "origin", spot.origin);
    auto angle = keys.find("angle");
    if (angle != keys.end()) {
        spot.angles[YAW] = static_cast<float>(std::atof(angle->second.c_str()));
    }
    level.spawns.push_back(spot);
}

}  // namespace

void SpawnEntitiesFromString() {
    EntityKeys keys;
    bool first = true;
    while (ReadEntity(keys)) {
        const std::string classname = keys.count("classname") ? keys["classname"] : "";
        if (first) {
            if (classname != "worldspawn") {
                Com_Error(ERR_DROP, "sgame: the first entity isn't worldspawn");
            }
            if (keys.count("message")) {
                trap::SetConfigstring(CS_MESSAGE, keys["message"].c_str());
            }
            first = false;
        } else if (classname == "info_player_deathmatch" || classname == "info_player_start") {
            SpawnPlayerStart(keys);
        } else {
            Com_DPrintf("sgame: ignoring entity '%s'\n", classname.c_str());
        }
    }

    if (level.spawns.empty()) {
        Com_Printf(S_COLOR_YELLOW "WARNING: map %s has no info_player_deathmatch; spawning at the origin\n",
                   level.mapname.c_str());
        level.spawns.push_back(SpawnPoint{});
    }
}

}  // namespace sg
