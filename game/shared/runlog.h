// Run records: what the trainer saves after every run and what the insights
// screens read. Pure data + JSON; file IO is the caller's (each module has its
// own syscalls). Layout on disk (home dir, base/):
//   stats/history.jsonl                 one RunSummary per line, appended
//   stats/runs/<scenario>__<stamp>.json one full RunRecord
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace skaar {

constexpr const char* kHistoryPath = "stats/history.jsonl";
constexpr const char* kRunsDir = "stats/runs";

// Local calendar time of a run, from trap_RealTime.
struct RunTime {
    int year = 1970, month = 1, day = 1, hour = 0, minute = 0, second = 0;

    int64_t DayNumber() const;           // days since 1970-01-01 (for streaks)
    int64_t Seconds() const;             // seconds since 1970-01-01, local
    std::string Stamp() const;           // 20261022-183005 (file names)
    std::string Iso() const;             // 2026-10-22T18:30:05
    static bool FromIso(std::string_view s, RunTime& out);
};

struct RunSummary {
    std::string scenario;     // scenario id
    std::string name;         // scenario display name at the time of the run
    std::string category;
    std::string file;         // stats/runs/... of the full record ("" if none)
    RunTime when;
    float duration = 0;       // seconds played (shorter if quit early; such runs aren't saved)
    float score = 0;
    int shots = 0, hits = 0, kills = 0;
    float accuracy = 0;       // 0..1 (hold: time on target / time firing)
    float ttk = 0;            // mean ms from a target's spawn to its kill (0: no kills)
    float onTarget = 0;       // hold: seconds on target
    float sensCm360 = 0;      // cm per 360° at the time (0: unknown dpi)
    float fov = 0;
};

struct ShotEvent {
    float t = 0;      // seconds into the run
    bool hit = false;
    int target = -1;  // target serial the shot was aimed nearest to (-1: none alive)
    float dx = 0, dy = 0;  // aim error at the shot, degrees: right/up of the target centre
    float radius = 0;      // that target's angular radius, degrees (for the dispersion plot)
};

struct KillEvent {
    float t = 0;
    int target = 0;   // serial (spawn order)
    float ttk = 0;    // ms from spawn
};

// View direction and eye position, sampled at a fixed rate (kSampleHz).
struct ViewSample {
    float t = 0;
    float yaw = 0, pitch = 0;  // degrees
    float x = 0, y = 0, z = 0;
};

constexpr float kSampleHz = 60;

struct RunRecord {
    int version = 1;
    RunSummary summary;
    uint32_t seed = 0;
    std::vector<ShotEvent> shots;
    std::vector<KillEvent> kills;
    std::vector<ViewSample> samples;
};

std::string SummaryToJsonLine(const RunSummary& s);   // no trailing newline
bool SummaryFromJson(std::string_view line, RunSummary& out);
// Reads every valid line of history.jsonl, oldest first; bad lines are skipped
// (and counted in *skipped, if given), never fatal.
std::vector<RunSummary> ParseHistory(std::string_view text, int* skipped = nullptr);

std::string RecordToJson(const RunRecord& r);
bool RecordFromJson(std::string_view text, RunRecord& out, std::string& error);

// File name for a new record: stats/runs/<scenario>__<stamp>.json
std::string RecordPath(const RunSummary& s);

}  // namespace skaar
