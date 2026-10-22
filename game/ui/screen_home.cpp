// Home, pause menu and loading screen: the numbered item lists.
//
// This file is part of Skaar. GPLv2 or later.
#include <cmath>
#include <functional>

#include "local.h"

namespace ui {
namespace {

struct Item {
    const char* label;
    std::string hint;  // mono, beside the focused item
    std::function<void()> action;
};

// 0 -> 1 over the first 220 ms after opening, eased out
float OpenFade() {
    const float t = std::fmin(1.0f, (ui.realtime - ui.openedAt) / 220.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

// Numbered list; the focused item is full cream with an ember tick and its hint
void Items(float top, const std::vector<Item>& items, float a) {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, step = 76;
    const float cap = f.text.CapHeight(type::kItem);

    for (size_t i = 0; i < items.size(); i++) {
        const float baseline = top + i * step;
        const std::string label = kit::Upper(items[i].label);
        const float labelX = m + 64;
        const float labelW = f.text.Measure(type::kItem, label, 0.12f);
        const Rect r{m - 40, baseline - cap - 20, labelX + labelW - m + 80, step};
        const int id = Focusable(r);
        const bool focused = Focused(id);

        f.mono.Draw(m, baseline, 17, skaar::Va("%02zu", i + 1), (focused ? theme::kCreamSoft : theme::kCreamFaint).Alpha(a),
                    theme::kTrackMono);
        f.text.Draw(labelX, baseline, type::kItem, label, (focused ? theme::kCream : theme::kCreamSoft).Alpha(a), 0.12f);
        if (focused) {
            const float pulse = 0.8f + 0.2f * std::sin(ui.realtime * 0.006f);
            kit::Fill(m - 28, baseline - cap, 4, cap, kEmber.Alpha(a * pulse));
            if (!items[i].hint.empty()) {
                Label(labelX + labelW + 32, baseline - 2, items[i].hint, theme::kCreamFaint.Alpha(a));
            }
        }
        if (Activated(id, r)) {
            items[i].action();
            return;  // the action may have changed screens
        }
    }
    // digits jump straight to an item
    for (int key : in.keys) {
        if (key >= '1' && key < '1' + static_cast<int>(items.size())) {
            items[key - '1'].action();
            return;
        }
    }
}

std::string LastRunHint() {
    const auto& o = data.overview;
    if (o.latest < 0) {
        return "NO RUNS YET";
    }
    const skaar::RunSummary& r = data.history[o.latest];
    return skaar::Va("LAST  %s  %.0f", kit::Upper(r.name).c_str(), r.score);
}

}  // namespace

void DrawHome() {
    const kit::Fonts& f = kit::GetFonts();
    const float a = OpenFade(), m = theme::kMargin, w = kit::Width(), h = kit::Height();

    kit::Background(true);

    // the mark is the object of the screen, right side
    const float markSize = 600;
    kit::Image(w - m - markSize + 40, (h - markSize) / 2 + 20, markSize, markSize, ui.mark, theme::kCream.Alpha(0.92f * a));

    Label(m, 84, "SKAAR   ·   AIM / MOVEMENT", theme::kCreamSoft.Alpha(a));
    Label(w - m, 84, ui.version, theme::kCreamFaint.Alpha(a), Align::kRight);
    kit::HLine(m, 120, w - 2 * m, theme::kHairline.Alpha(a));

    f.display.Draw(m - 6, 330, type::kHero, "TRAIN THE HAND.", theme::kCream.Alpha(a), theme::kTrackStatement);
    f.text.Draw(m, 396, 28, "Aim drills and movement, on an arena engine.", theme::kCreamSoft.Alpha(a));

    const auto& o = data.overview;
    std::vector<Item> items = {
        {"Play", skaar::Va("%zu SCENARIOS", data.scenarios.size()), [] { Open(Screen::kPlay); }},
        {"Insights", LastRunHint(), [] { Open(Screen::kInsights); }},
        {"Settings", "SENSITIVITY  ·  CROSSHAIR  ·  VIDEO", [] { Open(Screen::kSettings); }},
        {"Free roam", "MOVEMENT TEST ROOM", [] {
             Exec("set sk_scenario \"\"; devmap testroom\n");
             Close();
         }},
        {"Quit", "", [] { Exec("quit\n"); }},
    };
    if (InGame()) {
        items.insert(items.begin(), Item{"Resume", "ESC", [] { Close(); }});
    }
    Items(520, items, a);

    // today, at a glance
    const float y = h - 150;
    if (o.totalRuns > 0) {
        std::string line = skaar::Va("STREAK  %d %s", o.streakDays, o.streakDays == 1 ? "DAY" : "DAYS");
        line += skaar::Va("     TODAY  %d %s  ·  %s", o.todayRuns, o.todayRuns == 1 ? "RUN" : "RUNS",
                          FormatDuration(o.todaySeconds).c_str());
        line += skaar::Va("     ALL TIME  %d RUNS", o.totalRuns);
        Label(m, y, line, theme::kCreamSoft.Alpha(a));
    }
    if (!ui.lastError.empty()) {
        Label(m, y - 40, "LAST ERROR  ·  " + ui.lastError, kEmber.Alpha(a));
    }
    Footer("↑ ↓  SELECT     ENTER  CONFIRM     1–6  JUMP     ~  CONSOLE");
}

void DrawInGame() {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, w = kit::Width();
    const float a = OpenFade();

    kit::Fill(0, 0, w, kit::Height(), theme::kStage.Alpha(0.84f * a));
    const std::string map = Info_ValueForKey(trap::GetConfigString(CS_SERVERINFO).c_str(), "mapname");
    const std::string scenarioId = trap::CvarString("sk_scenario");
    const skaar::Scenario* s = data.FindScenario(scenarioId);
    const bool inScenario = s && s->map == map;

    Label(m, 84, "PAUSED   ·   " + kit::Upper(map) + (inScenario ? "   ·   " + kit::Upper(s->name) : ""),
          theme::kCreamSoft.Alpha(a));
    Label(w - m, 84, ui.version, theme::kCreamFaint.Alpha(a), Align::kRight);
    kit::HLine(m, 120, w - 2 * m, theme::kHairline.Alpha(a));
    f.display.Draw(m - 4, 290, type::kTitle, "PAUSED", theme::kCream.Alpha(a), theme::kTrackStatement);
    if (inScenario) {
        f.text.Draw(m, 340, type::kBody, "Leaving a run for this menu ends it. It isn't saved.", theme::kCreamSoft.Alpha(a));
    }

    std::vector<Item> items = {
        {"Resume", "ESC", [] { Close(); }},
    };
    if (inScenario) {
        items.push_back({"Restart run", "R", [] {
                             Exec("sk_restart\n");
                             Close();
                         }});
    }
    items.push_back({"Scenarios", "", [] { Open(Screen::kPlay); }});
    items.push_back({"Insights", "", [] { Open(Screen::kInsights); }});
    items.push_back({"Settings", "", [] { Open(Screen::kSettings); }});
    items.push_back({"Leave", "BACK TO THE MAIN MENU", [] { Exec("disconnect\n"); }});
    items.push_back({"Quit", "", [] { Exec("quit\n"); }});
    Items(440, items, a);
    Footer("ESC  RESUME     ↑ ↓  SELECT     ENTER  CONFIRM");
}

void DrawConnect(bool overlay) {
    if (overlay) {
        return;
    }
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, w = kit::Width();

    uiClientState_t cs;
    trap::GetClientState(&cs);

    kit::Background(true);
    Label(m, 84, "LOADING", theme::kCreamSoft);
    Label(w - m, 84, ui.version, theme::kCreamFaint, Align::kRight);
    kit::HLine(m, 120, w - 2 * m, theme::kHairline);
    kit::Image(m - 12, 180, 160, 160, ui.mark, theme::kCream);

    std::string map = Info_ValueForKey(trap::GetConfigString(CS_SERVERINFO).c_str(), "mapname");
    std::string title = map.empty() ? cs.servername : map;
    // name the scenario being loaded, if it's for this map
    std::string scenario;
    if (const skaar::Scenario* s = data.FindScenario(trap::CvarString("sk_scenario"))) {
        if (map.empty() || s->map == map) {
            title = s->name;
            scenario = s->description;
        }
    }
    f.display.Draw(m - 4, 470, type::kTitle, kit::Upper(title), theme::kCream, theme::kTrackStatement);

    const char* status = "";
    switch (cs.connState) {
        case CA_CONNECTING:
        case CA_CHALLENGING:
            status = "CONNECTING";
            break;
        case CA_CONNECTED:
            status = "AWAITING GAMESTATE";
            break;
        case CA_LOADING:
            status = "LOADING MAP";
            break;
        case CA_PRIMED:
            status = "AWAITING FIRST SNAPSHOT";
            break;
        default:
            break;
    }
    Label(m, 528, std::string(status) + "   ·   " + kit::Upper(map.empty() ? cs.servername : map), theme::kCreamSoft);
    if (!scenario.empty()) {
        f.text.Draw(m, 590, type::kBody, scenario, theme::kCreamSoft);
    }
    if (cs.messageString[0]) {
        Label(m, 640, cs.messageString, kEmber);
    }
}

}  // namespace ui
