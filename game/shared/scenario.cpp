// This file is part of Skaar. GPLv2 or later.
#include "scenario.h"

#include <algorithm>

#include "json.hpp"

namespace skaar {
namespace {

using json = nlohmann::json;

Range ReadRange(const json& j, const char* key, Range fallback) {
    if (!j.contains(key)) {
        return fallback;
    }
    const json& v = j.at(key);
    if (v.is_number()) {
        const float f = v.get<float>();
        return {f, f};
    }
    if (v.is_array() && v.size() == 2) {
        Range r{v[0].get<float>(), v[1].get<float>()};
        if (r.min > r.max) {
            std::swap(r.min, r.max);
        }
        return r;
    }
    throw std::runtime_error(std::string(key) + " must be a number or [min, max]");
}

template <typename T>
T Get(const json& j, const char* key, T fallback) {
    return j.contains(key) ? j.at(key).get<T>() : fallback;
}

json ParseJson(std::string_view text) {
    // comments allowed, like the map sources
    return json::parse(text.begin(), text.end(), nullptr, true, true);
}

}  // namespace

bool ParseScenario(std::string_view id, std::string_view text, Scenario& out, std::string& error) {
    try {
        const json j = ParseJson(text);
        Scenario s;
        s.id = id;
        s.name = Get<std::string>(j, "name", s.id);
        s.category = Get<std::string>(j, "category", "click");
        s.description = Get<std::string>(j, "description", "");
        s.map = Get<std::string>(j, "map", s.map);
        s.duration = Get<float>(j, "duration", s.duration);
        s.seed = Get<uint32_t>(j, "seed", 0);

        if (j.contains("player")) {
            s.playerMove = Get<bool>(j.at("player"), "move", false);
            s.ruleset = Get<std::string>(j.at("player"), "ruleset", s.ruleset);
            // it ends up in a cvar and a file name: keep it to a plain id
            if (s.ruleset.empty() || s.ruleset.size() > 32 ||
                s.ruleset.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_") != std::string::npos) {
                throw std::runtime_error("player.ruleset must be a lowercase id like \"cpm\"");
            }
        }

        const std::string fire = Get<std::string>(j, "fire", "click");
        if (fire == "click") {
            s.fire = FireMode::kClick;
        } else if (fire == "hold") {
            s.fire = FireMode::kHold;
        } else {
            throw std::runtime_error("fire must be \"click\" or \"hold\"");
        }

        s.weapon = Get<std::string>(j, "weapon", "");
        if (s.weapon.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_") != std::string::npos) {
            throw std::runtime_error("weapon must be a lowercase id like \"rifle\"");
        }

        if (j.contains("targets")) {
            const json& t = j.at("targets");
            auto& o = s.targets;
            o.alive = Get<int>(t, "alive", o.alive);
            o.radius = Get<float>(t, "radius", o.radius);
            o.health = Get<float>(t, "health", o.health);
            o.lifetime = Get<float>(t, "lifetime", o.lifetime);

            if (t.contains("spawn")) {
                const json& sp = t.at("spawn");
                const std::string shape = Get<std::string>(sp, "shape", "cone");
                if (shape == "cone") {
                    o.shape = SpawnShape::kCone;
                } else if (shape == "grid") {
                    o.shape = SpawnShape::kGrid;
                } else {
                    throw std::runtime_error("spawn.shape must be \"cone\" or \"grid\"");
                }
                o.distance = ReadRange(sp, "distance", o.distance);
                o.yaw = ReadRange(sp, "yaw", o.yaw);
                o.pitch = ReadRange(sp, "pitch", o.pitch);
                o.cols = Get<int>(sp, "cols", o.cols);
                o.rows = Get<int>(sp, "rows", o.rows);
            }
            if (t.contains("motion")) {
                const json& m = t.at("motion");
                const std::string type = Get<std::string>(m, "type", "static");
                if (type == "static") {
                    o.motion = MotionType::kStatic;
                } else if (type == "strafe") {
                    o.motion = MotionType::kStrafe;
                } else if (type == "float") {
                    o.motion = MotionType::kFloat;
                } else {
                    throw std::runtime_error("motion.type must be static, strafe or float");
                }
                o.speed = ReadRange(m, "speed", o.speed);
                o.changeEvery = ReadRange(m, "changeEvery", o.changeEvery);
            }
        }

        if (j.contains("score")) {
            const json& sc = j.at("score");
            s.score.kill = Get<float>(sc, "kill", s.score.kill);
            s.score.accuracyWeight = Get<float>(sc, "accuracyWeight", s.score.accuracyWeight);
            s.score.perSecondOnTarget = Get<float>(sc, "perSecondOnTarget", s.score.perSecondOnTarget);
        }

        // sanity limits: bad data must not hang or crash the sim
        if (s.duration < 5 || s.duration > 600) {
            throw std::runtime_error("duration must be 5..600 seconds");
        }
        if (s.targets.alive < 1 || s.targets.alive > 16) {
            throw std::runtime_error("targets.alive must be 1..16");
        }
        if (s.targets.radius < 1 || s.targets.radius > 256) {
            throw std::runtime_error("targets.radius must be 1..256");
        }
        if (s.targets.health <= 0) {
            throw std::runtime_error("targets.health must be > 0");
        }
        if (s.targets.distance.min < 64) {
            throw std::runtime_error("targets.spawn.distance must be >= 64");
        }
        if (s.targets.cols < 1 || s.targets.rows < 1 || s.targets.cols * s.targets.rows > 256) {
            throw std::runtime_error("targets.spawn cols/rows out of range");
        }
        if (s.targets.shape == SpawnShape::kGrid && s.targets.cols * s.targets.rows <= s.targets.alive) {
            throw std::runtime_error("a grid needs more cells than targets.alive");
        }
        if (s.targets.changeEvery.min < 0.05f) {
            throw std::runtime_error("motion.changeEvery must be >= 0.05");
        }

        out = std::move(s);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

std::string ScenarioToJson(const Scenario& s) {
    auto range = [](Range r) { return r.min == r.max ? json(r.min) : json::array({r.min, r.max}); };
    const auto& t = s.targets;
    json spawn = {{"shape", t.shape == SpawnShape::kGrid ? "grid" : "cone"},
                  {"distance", range(t.distance)},
                  {"yaw", range(t.yaw)},
                  {"pitch", range(t.pitch)}};
    if (t.shape == SpawnShape::kGrid) {
        spawn["cols"] = t.cols;
        spawn["rows"] = t.rows;
    }
    json targets = {{"alive", t.alive}, {"radius", t.radius}, {"health", t.health}, {"spawn", spawn}};
    if (t.lifetime > 0) {
        targets["lifetime"] = t.lifetime;
    }
    if (t.motion != MotionType::kStatic) {
        targets["motion"] = {{"type", t.motion == MotionType::kStrafe ? "strafe" : "float"},
                             {"speed", range(t.speed)},
                             {"changeEvery", range(t.changeEvery)}};
    }
    json j = {
        {"name", s.name},
        {"category", s.category},
        {"description", s.description},
        {"map", s.map},
        {"duration", s.duration},
        {"player", {{"move", s.playerMove}, {"ruleset", s.ruleset}}},
        {"fire", s.fire == FireMode::kHold ? "hold" : "click"},
        {"targets", targets},
        {"score",
         {{"kill", s.score.kill},
          {"accuracyWeight", s.score.accuracyWeight},
          {"perSecondOnTarget", s.score.perSecondOnTarget}}},
    };
    if (s.seed) {
        j["seed"] = s.seed;
    }
    if (!s.weapon.empty()) {
        j["weapon"] = s.weapon;
    }
    return j.dump(2) + "\n";
}

bool ParseBenchmark(std::string_view id, std::string_view text, Benchmark& out, std::string& error) {
    try {
        const json j = ParseJson(text);
        Benchmark b;
        b.id = id;
        b.name = Get<std::string>(j, "name", b.id);
        b.description = Get<std::string>(j, "description", "");
        b.tiers = j.at("tiers").get<std::vector<std::string>>();
        if (b.tiers.empty()) {
            throw std::runtime_error("tiers is empty");
        }
        for (const json& e : j.at("scenarios")) {
            Benchmark::Entry entry;
            entry.scenario = e.at("id").get<std::string>();
            entry.thresholds = e.at("thresholds").get<std::vector<float>>();
            if (entry.thresholds.size() != b.tiers.size()) {
                throw std::runtime_error(entry.scenario + ": thresholds must match tiers");
            }
            if (!std::is_sorted(entry.thresholds.begin(), entry.thresholds.end())) {
                throw std::runtime_error(entry.scenario + ": thresholds must ascend");
            }
            b.entries.push_back(std::move(entry));
        }
        out = std::move(b);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

int TierFor(const Benchmark::Entry& entry, float score, float* progress) {
    const auto& t = entry.thresholds;
    int tier = -1;
    for (size_t i = 0; i < t.size(); i++) {
        if (score >= t[i]) {
            tier = static_cast<int>(i);
        }
    }
    if (progress) {
        if (tier + 1 >= static_cast<int>(t.size())) {
            *progress = 1;
        } else {
            const float lo = tier < 0 ? 0 : t[tier];
            const float hi = t[tier + 1];
            *progress = hi > lo ? std::clamp((score - lo) / (hi - lo), 0.0f, 1.0f) : 0;
        }
    }
    return tier;
}

}  // namespace skaar
