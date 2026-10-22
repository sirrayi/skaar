// Arena sessions (P6) for the insights screens: one JSON line per session in
// stats/arena.jsonl (home dir), written by the server when the player leaves
// the map. Pure data + JSON, like runlog.h.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "runlog.h"

namespace skaar {

constexpr const char* kArenaHistoryPath = "stats/arena.jsonl";

struct ArenaWeaponStats {
    int shots = 0, hits = 0;
    float Accuracy() const { return shots > 0 ? static_cast<float>(hits) / shots : 0; }
};

struct ArenaSession {
    RunTime when;            // when the session ended
    std::string map;
    std::string ruleset;     // movement physics
    float seconds = 0;
    int damageDealt = 0, damageTaken = 0, kills = 0, deaths = 0;
    std::map<std::string, ArenaWeaponStats> weapons;  // by weapon id
};

std::string ArenaSessionToJsonLine(const ArenaSession& s);
bool ArenaSessionFromJson(std::string_view line, ArenaSession& out);
std::vector<ArenaSession> ParseArenaHistory(std::string_view text);  // bad lines skipped

}  // namespace skaar
