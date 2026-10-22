// Insights: everything the stats screens show, derived from run history.
// Pure functions over RunSummary lists, so they can be unit-tested.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "runlog.h"
#include "scenario.h"

namespace skaar::insights {

struct ScenarioStats {
    std::string id, name, category;
    std::vector<int> runs;      // indices into the history, oldest first
    int bestRun = -1;           // index of the highest score
    float best = 0, last = 0;
    float recentAvg = 0;        // mean score of the last kRecent runs
    float recentAccuracy = 0;   // mean accuracy of the last kRecent runs
    float recentTtk = 0;        // mean TTK (ms) of the last kRecent runs with kills
    float trend = 0;            // recent mean vs the kRecent before it, as a fraction (+0.05 = 5% better)
    float seconds = 0;          // time played
    int64_t lastPlayed = 0;     // RunTime::Seconds of the newest run
};

constexpr int kRecent = 10;

struct Overview {
    int totalRuns = 0;
    float totalSeconds = 0;
    int streakDays = 0;         // consecutive days with a run, ending today (or yesterday)
    bool playedToday = false;
    int todayRuns = 0;
    float todaySeconds = 0;
    int sessionRuns = 0;        // runs in the current session (no gap > kSessionGap)
    float sessionSeconds = 0;
    int weekPersonalBests = 0;  // runs in the last 7 days that set a new PB
    int latest = -1;            // index of the newest run
    bool latestIsPb = false;
};

constexpr int64_t kSessionGap = 30 * 60;  // seconds

struct BenchmarkEntryProgress {
    std::string scenario;
    float best = 0;
    bool played = false;
    int tier = -1;
    float progress = 0;         // toward the next tier
};

struct BenchmarkProgress {
    const Benchmark* benchmark = nullptr;
    std::vector<BenchmarkEntryProgress> entries;
    int overallTier = -1;       // lowest tier across entries (-1 if any is unplayed or below tier 1)
    float overallScore = 0;     // mean of (tier + 1 + progress) over entries, 0..tiers.size()
    int recommended = -1;       // entry to focus on next
    std::string weakestCategory;
};

std::map<std::string, ScenarioStats> PerScenario(const std::vector<RunSummary>& history);
Overview Summarize(const std::vector<RunSummary>& history, const RunTime& now);
BenchmarkProgress Progress(const Benchmark& benchmark, const std::map<std::string, ScenarioStats>& stats,
                           const std::vector<RunSummary>& history, const std::vector<Scenario>& scenarios);

// True if run `index` beat every earlier run of its scenario (the first run counts).
bool IsPersonalBest(const std::vector<RunSummary>& history, int index);

}  // namespace skaar::insights
