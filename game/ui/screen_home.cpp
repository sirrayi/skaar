// Home, pause menu and loading screen: the numbered item lists.
//
// This file is part of Skaar. GPLv2 or later.
#include <algorithm>
#include <cmath>
#include <functional>

#include "local.h"

namespace ui {
namespace {

struct Item {
    const char* label;
    std::function<void()> action;
};

// 0 -> 1 over the first 120 ms after opening: quick, so screens feel instant
float OpenFade() {
    const float t = std::fmin(1.0f, (ui.realtime - ui.openedAt) / 120.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// Big list. The focused item brightens and eases a few units to the right
// while the marker slides to it; digits jump straight to an item.
void Items(float top, const std::vector<Item>& items, float a) {
    static float nudge[16];
    static int lastTime;
    const float k = 1.0f - std::exp(-std::clamp(ui.realtime - lastTime, 0, 100) / 40.0f);
    lastTime = ui.realtime;

    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, step = 84;
    const float cap = f.text.CapHeight(type::kItem);

    for (size_t i = 0; i < items.size() && i < 16; i++) {
        const float baseline = top + i * step;
        const std::string label = kit::Upper(items[i].label);
        const float labelX = m + 56;
        const float labelW = f.text.Measure(type::kItem, label, 0.12f);
        const Rect r{m - 40, baseline - cap - 24, labelX + labelW - m + 100, step};
        const int id = Focusable(r);
        const bool focused = Focused(id);
        nudge[i] += ((focused ? 14.0f : 0.0f) - nudge[i]) * k;

        f.mono.Draw(m, baseline, 16, skaar::Va("%zu", i + 1), theme::kCreamFaint.Alpha(a), 0);
        f.text.Draw(labelX + nudge[i], baseline, type::kItem, label,
                    (focused ? theme::kCream : theme::kCreamSoft).Alpha(a), 0.12f);
        if (focused) {
            Marker(m - 28, baseline, cap);
        }
        if (Activated(id, r)) {
            items[i].action();
            return;  // the action may have changed screens
        }
    }
    for (int key : in.keys) {
        if (key >= '1' && key < '1' + static_cast<int>(items.size())) {
            items[key - '1'].action();
            return;
        }
    }
}

}  // namespace

void DrawHome() {
    const kit::Fonts& f = kit::GetFonts();
    const float a = OpenFade(), m = theme::kMargin, w = kit::Width(), h = kit::Height();

    kit::Background(false);

    // the mark is the object of the screen; the name and the list are all the words
    const float markSize = 560;
    kit::Image(w - m - markSize + 40, (h - markSize) / 2, markSize, markSize, ui.mark, theme::kCream.Alpha(0.92f * a));
    f.display.Draw(m - 6, 300, type::kHero, "SKAAR", theme::kCream.Alpha(a), 0.04f);

    std::vector<Item> items = {
        {"Play", [] { Open(Screen::kPlay); }},
        {"Insights", [] { Open(Screen::kInsights); }},
        {"Settings", [] { Open(Screen::kSettings); }},
        {"Free roam", [] {
             Exec("set sk_scenario \"\"; devmap testroom\n");
             Close();
         }},
        {"Quit", [] { Exec("quit\n"); }},
    };
    if (InGame()) {
        items.insert(items.begin(), Item{"Resume", [] { Close(); }});
    }
    Items(450, items, a);

    // one quiet line of standing, if there is any
    const auto& o = data.overview;
    if (o.totalRuns > 0) {
        std::string line = skaar::Va("%d-day streak", o.streakDays);
        if (o.todayRuns > 0) {
            line += skaar::Va("   ·   %d %s today", o.todayRuns, o.todayRuns == 1 ? "run" : "runs");
        }
        f.text.Draw(m, h - 110, 20, line, theme::kCreamFaint.Alpha(a));
    }
    if (!ui.lastError.empty()) {
        Label(m, h - 150, ui.lastError, kEmber.Alpha(a));
    }
    Label(w - m, h - 56, ui.version, theme::kCreamFaint.Alpha(a * 0.7f), Align::kRight);
}

void DrawInGame() {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, w = kit::Width();
    const float a = OpenFade();

    kit::Fill(0, 0, w, kit::Height(), theme::kStage.Alpha(0.86f * a));
    const std::string map = Info_ValueForKey(trap::GetConfigString(CS_SERVERINFO).c_str(), "mapname");
    const skaar::Scenario* s = data.FindScenario(trap::CvarString("sk_scenario"));
    const bool inScenario = s && s->map == map;

    f.display.Draw(m - 4, 300, type::kTitle, inScenario ? kit::Upper(s->name) : "PAUSED", theme::kCream.Alpha(a),
                   theme::kTrackStatement);

    std::vector<Item> items = {{"Resume", [] { Close(); }}};
    if (inScenario) {
        items.push_back({"Restart", [] {
                             Exec("sk_restart\n");
                             Close();
                         }});
    }
    items.push_back({"Scenarios", [] { Open(Screen::kPlay); }});
    items.push_back({"Insights", [] { Open(Screen::kInsights); }});
    items.push_back({"Settings", [] { Open(Screen::kSettings); }});
    items.push_back({"Leave", [] { Exec("disconnect\n"); }});
    Items(420, items, a);
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
