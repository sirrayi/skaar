// Scenarios, benchmarks and run history from disk, and the insights over them.
//
// This file is part of Skaar. GPLv2 or later.
#include <algorithm>

#include "local.h"

namespace ui {
namespace {

std::string StripJson(const std::string& name) {
    return name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0 ? name.substr(0, name.size() - 5) : name;
}

skaar::RunTime Now() {
    qtime_t t{};
    trap::RealTime(&t);
    skaar::RunTime r;
    r.year = t.tm_year + 1900;
    r.month = t.tm_mon + 1;
    r.day = t.tm_mday;
    r.hour = t.tm_hour;
    r.minute = t.tm_min;
    r.second = t.tm_sec;
    return r;
}

// Category order in lists: the built-in three first, then anything users add
int CategoryRank(const std::string& c) {
    if (c == "click") {
        return 0;
    }
    if (c == "track") {
        return 1;
    }
    if (c == "switch") {
        return 2;
    }
    return 3;
}

}  // namespace

const skaar::Scenario* Data::FindScenario(const std::string& id) const {
    for (const skaar::Scenario& s : scenarios) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

std::string Data::NameOf(const std::string& scenarioId) const {
    if (const skaar::Scenario* s = FindScenario(scenarioId)) {
        return s->name;
    }
    if (auto it = perScenario.find(scenarioId); it != perScenario.end()) {
        return it->second.name;
    }
    return scenarioId;
}

void LoadData() {
    Data d;
    d.now = Now();

    for (const std::string& file : trap::FileList("scenarios", ".json")) {
        const std::string id = StripJson(file);
        std::string text, error;
        skaar::Scenario s;
        if (!trap::FileRead(skaar::Va("scenarios/%s", file.c_str()), text)) {
            continue;
        }
        if (skaar::ParseScenario(id, text, s, error)) {
            d.scenarios.push_back(std::move(s));
        } else {
            d.scenarioErrors.emplace_back(id, error);
            Com_Printf(S_COLOR_YELLOW "WARNING: scenarios/%s: %s\n", file.c_str(), error.c_str());
        }
    }
    std::stable_sort(d.scenarios.begin(), d.scenarios.end(), [](const skaar::Scenario& a, const skaar::Scenario& b) {
        const int ra = CategoryRank(a.category), rb = CategoryRank(b.category);
        return ra != rb ? ra < rb : a.category != b.category ? a.category < b.category : a.name < b.name;
    });

    for (const std::string& file : trap::FileList("benchmarks", ".json")) {
        std::string text, error;
        skaar::Benchmark b;
        if (trap::FileRead(skaar::Va("benchmarks/%s", file.c_str()), text) &&
            skaar::ParseBenchmark(StripJson(file), text, b, error)) {
            d.benchmarks.push_back(std::move(b));
        } else if (!error.empty()) {
            Com_Printf(S_COLOR_YELLOW "WARNING: benchmarks/%s: %s\n", file.c_str(), error.c_str());
        }
    }

    std::string history;
    if (trap::FileRead(skaar::kHistoryPath, history)) {
        d.history = skaar::ParseHistory(history);
    }
    d.perScenario = skaar::insights::PerScenario(d.history);
    d.overview = skaar::insights::Summarize(d.history, d.now);

    data = std::move(d);
    // progress keeps pointers into data.benchmarks, so build it after the move
    for (const skaar::Benchmark& b : data.benchmarks) {
        data.progress.push_back(skaar::insights::Progress(b, data.perScenario, data.history, data.scenarios));
    }
}

const skaar::RunRecord* LoadRunRecord(const std::string& file) {
    if (file.empty()) {
        return nullptr;
    }
    if (ui.runRecordFile == file) {
        return ui.runRecord ? &*ui.runRecord : nullptr;
    }
    ui.runRecordFile = file;
    ui.runRecord.reset();
    ui.runError.clear();
    std::string text, error;
    if (!trap::FileRead(file.c_str(), text)) {
        ui.runError = "The run file is missing: " + file;
        return nullptr;
    }
    skaar::RunRecord record;
    if (!skaar::RecordFromJson(text, record, error)) {
        ui.runError = "The run file can't be read: " + error;
        return nullptr;
    }
    ui.runRecord = std::move(record);
    return &*ui.runRecord;
}

}  // namespace ui
