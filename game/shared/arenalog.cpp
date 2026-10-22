// This file is part of Skaar. GPLv2 or later.
#include "arenalog.h"

#include <cmath>

#include "json.hpp"

namespace skaar {
namespace {

using json = nlohmann::json;

template <typename T>
T Get(const json& j, const char* key, T fallback) {
    auto it = j.find(key);
    return it != j.end() && !it->is_null() ? it->get<T>() : fallback;
}

}  // namespace

std::string ArenaSessionToJsonLine(const ArenaSession& s) {
    json weapons = json::object();
    for (const auto& [id, w] : s.weapons) {
        weapons[id] = {w.shots, w.hits};
    }
    json j = {{"when", s.when.Iso()},
              {"map", s.map},
              {"ruleset", s.ruleset},
              {"seconds", std::round(s.seconds * 10) / 10},
              {"damageDealt", s.damageDealt},
              {"damageTaken", s.damageTaken},
              {"kills", s.kills},
              {"deaths", s.deaths},
              {"weapons", weapons}};
    return j.dump();
}

bool ArenaSessionFromJson(std::string_view line, ArenaSession& out) {
    const json j = json::parse(line.begin(), line.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("when")) {
        return false;
    }
    try {
        ArenaSession s;
        if (!RunTime::FromIso(j.at("when").get<std::string>(), s.when)) {
            return false;
        }
        s.map = Get<std::string>(j, "map", "");
        s.ruleset = Get<std::string>(j, "ruleset", "vq3");
        s.seconds = Get<float>(j, "seconds", 0);
        s.damageDealt = Get<int>(j, "damageDealt", 0);
        s.damageTaken = Get<int>(j, "damageTaken", 0);
        s.kills = Get<int>(j, "kills", 0);
        s.deaths = Get<int>(j, "deaths", 0);
        if (j.contains("weapons")) {
            for (const auto& [id, v] : j.at("weapons").items()) {
                s.weapons[id] = {v.at(0).get<int>(), v.at(1).get<int>()};
            }
        }
        out = std::move(s);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<ArenaSession> ParseArenaHistory(std::string_view text) {
    std::vector<ArenaSession> out;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        ArenaSession s;
        if (ArenaSessionFromJson(text.substr(start, end - start), s)) {
            out.push_back(std::move(s));
        }
        start = end + 1;
    }
    return out;
}

}  // namespace skaar
