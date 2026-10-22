// Aim-trainer scenarios and benchmarks, parsed from data/base/scenarios/*.json
// and data/base/benchmarks/*.json. Shared by the cgame (runs them) and the ui
// (lists them). Format: docs/scenarios.md.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace skaar {

struct Range {
    float min = 0, max = 0;
};

enum class FireMode { kClick, kHold };            // one shot per press / continuous beam
enum class SpawnShape { kCone, kGrid };           // random in an angular window / fixed cells
enum class MotionType { kStatic, kStrafe, kFloat };

struct Scenario {
    std::string id;           // file name without .json
    std::string name;
    std::string category;     // click | track | switch (free text, used for grouping)
    std::string description;
    std::string map = "range";
    float duration = 60;      // seconds
    uint32_t seed = 0;        // 0: new seed every run

    bool playerMove = false;  // false: frozen in place (view still turns)
    std::string ruleset = "vq3";  // movement physics: vq3 | cpm | rulesets/<id>.json (game/shared/move_ruleset.h)

    FireMode fire = FireMode::kClick;

    struct Targets {
        int alive = 1;            // on screen at once
        float radius = 16;        // world units
        float health = 1;         // click: hits to kill; hold: seconds on target to kill
        float lifetime = 0;       // seconds before a target times out (0: never)

        SpawnShape shape = SpawnShape::kCone;
        Range distance{800, 800}; // from the eye, world units
        Range yaw{-30, 30};       // degrees around the spawn facing (left is +)
        Range pitch{-10, 10};     // degrees, up is +
        int cols = 5, rows = 5;   // grid cells, spread across yaw/pitch

        MotionType motion = MotionType::kStatic;
        Range speed{0, 0};        // world units per second
        Range changeEvery{0.4f, 1.2f};  // seconds between direction changes
    } targets;

    struct Score {
        float kill = 100;          // per kill
        float accuracyWeight = 0.5f;  // click: total *= (1 - w) + w * accuracy
        float perSecondOnTarget = 100;  // hold: points per second of beam on target
    } score;
};

// Parses one scenario; id comes from the file name. Returns false and sets
// error on bad JSON or bad values.
bool ParseScenario(std::string_view id, std::string_view json, Scenario& out, std::string& error);

struct Benchmark {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> tiers;  // lowest first
    struct Entry {
        std::string scenario;
        std::vector<float> thresholds;  // score needed for each tier, same length as tiers
    };
    std::vector<Entry> entries;
};

bool ParseBenchmark(std::string_view id, std::string_view json, Benchmark& out, std::string& error);

// Tier reached by a score: -1 below the first, else the index of the highest
// threshold met. progress is 0..1 toward the next tier (1 when maxed).
int TierFor(const Benchmark::Entry& entry, float score, float* progress = nullptr);

}  // namespace skaar
