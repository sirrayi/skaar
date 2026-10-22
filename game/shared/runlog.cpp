// This file is part of Skaar. GPLv2 or later.
#include "runlog.h"

#include <cmath>
#include <cstdio>

#include "json.hpp"

namespace skaar {
namespace {

using json = nlohmann::json;

// Howard Hinnant's days_from_civil
int64_t DaysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

// Round to a fixed number of decimals so records stay small and diffable
double R(double v, int decimals) {
    const double k = std::pow(10.0, decimals);
    return std::round(v * k) / k;
}

json SummaryToJson(const RunSummary& s) {
    json j = {
        {"scenario", s.scenario}, {"name", s.name}, {"category", s.category}, {"when", s.when.Iso()},
        {"duration", R(s.duration, 2)}, {"score", R(s.score, 1)}, {"shots", s.shots}, {"hits", s.hits},
        {"kills", s.kills}, {"accuracy", R(s.accuracy, 4)}, {"ttk", R(s.ttk, 1)},
    };
    if (!s.file.empty()) {
        j["file"] = s.file;
    }
    if (s.onTarget > 0) {
        j["onTarget"] = R(s.onTarget, 2);
    }
    if (s.sensCm360 > 0) {
        j["cm360"] = R(s.sensCm360, 2);
    }
    if (s.fov > 0) {
        j["fov"] = R(s.fov, 1);
    }
    return j;
}

template <typename T>
T Get(const json& j, const char* key, T fallback) {
    auto it = j.find(key);
    return it != j.end() && !it->is_null() ? it->get<T>() : fallback;
}

bool SummaryFromJsonValue(const json& j, RunSummary& s) {
    if (!j.is_object() || !j.contains("scenario") || !j.contains("when")) {
        return false;
    }
    s.scenario = j.at("scenario").get<std::string>();
    if (!RunTime::FromIso(j.at("when").get<std::string>(), s.when)) {
        return false;
    }
    s.name = Get<std::string>(j, "name", s.scenario);
    s.category = Get<std::string>(j, "category", "");
    s.file = Get<std::string>(j, "file", "");
    s.duration = Get<float>(j, "duration", 0);
    s.score = Get<float>(j, "score", 0);
    s.shots = Get<int>(j, "shots", 0);
    s.hits = Get<int>(j, "hits", 0);
    s.kills = Get<int>(j, "kills", 0);
    s.accuracy = Get<float>(j, "accuracy", 0);
    s.ttk = Get<float>(j, "ttk", 0);
    s.onTarget = Get<float>(j, "onTarget", 0);
    s.sensCm360 = Get<float>(j, "cm360", 0);
    s.fov = Get<float>(j, "fov", 0);
    return true;
}

}  // namespace

int64_t RunTime::DayNumber() const { return DaysFromCivil(year, month, day); }

int64_t RunTime::Seconds() const { return DayNumber() * 86400 + hour * 3600 + minute * 60 + second; }

std::string RunTime::Stamp() const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d", year, month, day, hour, minute, second);
    return buf;
}

std::string RunTime::Iso() const {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", year, month, day, hour, minute, second);
    return buf;
}

bool RunTime::FromIso(std::string_view s, RunTime& out) {
    const std::string str(s);
    RunTime t;
    if (std::sscanf(str.c_str(), "%d-%d-%dT%d:%d:%d", &t.year, &t.month, &t.day, &t.hour, &t.minute, &t.second) !=
        6) {
        return false;
    }
    if (t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31) {
        return false;
    }
    out = t;
    return true;
}

std::string SummaryToJsonLine(const RunSummary& s) { return SummaryToJson(s).dump(); }

bool SummaryFromJson(std::string_view line, RunSummary& out) {
    const json j = json::parse(line.begin(), line.end(), nullptr, false);
    if (j.is_discarded()) {
        return false;
    }
    try {
        return SummaryFromJsonValue(j, out);
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<RunSummary> ParseHistory(std::string_view text, int* skipped) {
    std::vector<RunSummary> runs;
    if (skipped) {
        *skipped = 0;
    }
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        const std::string_view line = text.substr(start, end - start);
        RunSummary s;
        if (line.find_first_not_of(" \t\r") == std::string_view::npos) {
            // blank line
        } else if (SummaryFromJson(line, s)) {
            runs.push_back(std::move(s));
        } else if (skipped) {
            (*skipped)++;
        }
        start = end + 1;
    }
    return runs;
}

std::string RecordToJson(const RunRecord& r) {
    json j;
    j["version"] = r.version;
    j["summary"] = SummaryToJson(r.summary);
    j["seed"] = r.seed;

    // Events as compact arrays: [t, hit, target, dx, dy, radius]
    json shots = json::array();
    for (const ShotEvent& s : r.shots) {
        shots.push_back({R(s.t, 3), s.hit ? 1 : 0, s.target, R(s.dx, 3), R(s.dy, 3), R(s.radius, 3)});
    }
    j["shots"] = std::move(shots);

    json kills = json::array();
    for (const KillEvent& k : r.kills) {
        kills.push_back({R(k.t, 3), k.target, R(k.ttk, 1)});
    }
    j["kills"] = std::move(kills);

    // [t, yaw, pitch, x, y, z]
    json samples = json::array();
    for (const ViewSample& v : r.samples) {
        samples.push_back({R(v.t, 3), R(v.yaw, 3), R(v.pitch, 3), R(v.x, 1), R(v.y, 1), R(v.z, 1)});
    }
    j["samples"] = std::move(samples);
    j["sampleHz"] = kSampleHz;
    return j.dump();
}

bool RecordFromJson(std::string_view text, RunRecord& out, std::string& error) {
    const json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) {
        error = "not valid JSON";
        return false;
    }
    try {
        RunRecord r;
        r.version = Get<int>(j, "version", 1);
        if (!SummaryFromJsonValue(j.at("summary"), r.summary)) {
            error = "bad summary";
            return false;
        }
        r.seed = Get<uint32_t>(j, "seed", 0);
        for (const json& a : j.at("shots")) {
            ShotEvent s;
            s.t = a.at(0).get<float>();
            s.hit = a.at(1).get<int>() != 0;
            s.target = a.at(2).get<int>();
            s.dx = a.at(3).get<float>();
            s.dy = a.at(4).get<float>();
            s.radius = a.at(5).get<float>();
            r.shots.push_back(s);
        }
        for (const json& a : j.at("kills")) {
            r.kills.push_back({a.at(0).get<float>(), a.at(1).get<int>(), a.at(2).get<float>()});
        }
        for (const json& a : j.at("samples")) {
            ViewSample v;
            v.t = a.at(0).get<float>();
            v.yaw = a.at(1).get<float>();
            v.pitch = a.at(2).get<float>();
            v.x = a.at(3).get<float>();
            v.y = a.at(4).get<float>();
            v.z = a.at(5).get<float>();
            r.samples.push_back(v);
        }
        out = std::move(r);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

std::string RecordPath(const RunSummary& s) {
    return std::string(kRunsDir) + "/" + s.scenario + "__" + s.when.Stamp() + ".json";
}

}  // namespace skaar
