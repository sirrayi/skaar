// Play: the scenario browser. Filter by category, pick one, read its spec and
// your history on it, start it.
//
// This file is part of Skaar. GPLv2 or later.
#include <algorithm>
#include <set>

#include "local.h"

namespace ui {
namespace {

constexpr float kListW = 820;
constexpr float kRowH = 72;

const char* FireLabel(skaar::FireMode f) { return f == skaar::FireMode::kHold ? "HOLD" : "CLICK"; }

const char* MotionLabel(skaar::MotionType m) {
    switch (m) {
        case skaar::MotionType::kStrafe:
            return "STRAFING";
        case skaar::MotionType::kFloat:
            return "FLOATING";
        default:
            return "STATIC";
    }
}

// Best tier name reached on this scenario in any benchmark, or ""
std::string TierName(const std::string& id) {
    for (const auto& p : data.progress) {
        for (const auto& e : p.entries) {
            if (e.scenario == id && e.played && e.tier >= 0) {
                return p.benchmark->tiers[e.tier];
            }
        }
    }
    return "";
}

void DrawDetail(const skaar::Scenario& s, float x, float top, float w) {
    const kit::Fonts& f = kit::GetFonts();
    const float h = kit::Height();
    const auto stats = data.perScenario.find(s.id);
    const bool played = stats != data.perScenario.end();

    Label(x, top + 16, skaar::Va("%s   ·   %s   ·   %s TARGETS", s.category.c_str(), FormatDuration(s.duration).c_str(),
                                 MotionLabel(s.targets.motion)),
          theme::kCreamSoft);
    float titleSize = type::kTitle;
    while (titleSize > 40 && f.display.Measure(titleSize, kit::Upper(s.name), theme::kTrackStatement) > w) {
        titleSize -= 4;
    }
    f.display.Draw(x - 4, top + 96, titleSize, kit::Upper(s.name), theme::kCream, theme::kTrackStatement);
    const float descEnd = Paragraph(x, top + 124, std::min(w, 760.0f), s.description, type::kBody, theme::kCreamSoft);

    // spec + standing, cells sharing hairlines
    const float gridY = std::max(top + 230, descEnd + 40);
    const float cellW = w / 3, cellH = 104;
    CellGrid(x, gridY, cellW, cellH, 3, 6);
    Cell({x, gridY, cellW, cellH}, "TARGETS", skaar::Va("%d", s.targets.alive));
    Cell({x + cellW, gridY, cellW, cellH}, "SIZE", skaar::Va("%.0f U", s.targets.radius * 2));
    Cell({x + 2 * cellW, gridY, cellW, cellH}, "FIRE", FireLabel(s.fire));
    Cell({x, gridY + cellH, cellW, cellH}, "BEST", played ? skaar::Va("%.0f", stats->second.best) : "—");
    Cell({x + cellW, gridY + cellH, cellW, cellH}, "RUNS", played ? skaar::Va("%zu", stats->second.runs.size()) : "0");
    const std::string tier = TierName(s.id);
    if (!tier.empty()) {
        Cell({x + 2 * cellW, gridY + cellH, cellW, cellH}, "TIER", kit::Upper(tier));
    } else {
        Cell({x + 2 * cellW, gridY + cellH, cellW, cellH}, "AVG · LAST 10",
             played ? skaar::Va("%.0f", stats->second.recentAvg) : "—");
    }

    // history
    const float chartTop = gridY + 2 * cellH + 56;
    const float chartBottom = h - 250;
    Label(x, chartTop, played ? skaar::Va("SCORE  ·  %zu RUNS", stats->second.runs.size()) : "SCORE");
    if (played && stats->second.runs.size() >= 2 && chartBottom - chartTop > 100) {
        ScoreChart({x + 70, chartTop + 30, w - 70, chartBottom - chartTop - 30}, stats->second.runs);
    } else {
        f.text.Draw(x, chartTop + 50, type::kBody, played ? "One run so far. Play again to see a trend."
                                                          : "No runs yet. Your scores will land here.",
                    theme::kCreamFaint);
    }

    if (ActionButton({x, h - 200, w, 72}, "Start  " + s.name, "ENTER")) {
        StartScenario(s.id);
    }
}

}  // namespace

void DrawPlay() {
    const kit::Fonts& f = kit::GetFonts();
    const float m = theme::kMargin, w = kit::Width(), h = kit::Height();

    if (InGame()) {
        kit::Fill(0, 0, w, h, theme::kStage.Alpha(0.94f));
    } else {
        kit::Background(true);
    }
    Header(skaar::Va("%zu SCENARIOS", data.scenarios.size()));

    // category filter
    std::vector<std::string> categories = {"all"};
    for (const skaar::Scenario& s : data.scenarios) {
        if (std::find(categories.begin(), categories.end(), s.category) == categories.end()) {
            categories.push_back(s.category);
        }
    }
    float x = m;
    for (const std::string& c : categories) {
        float tw = 0;
        if (Tab(x, 186, c, ui.category == c, &tw)) {
            ui.category = c;
        }
        x += tw + 44;
    }

    std::vector<const skaar::Scenario*> shown;
    for (const skaar::Scenario& s : data.scenarios) {
        if (ui.category == "all" || s.category == ui.category) {
            shown.push_back(&s);
        }
    }
    if (shown.empty()) {
        f.text.Draw(m, 300, type::kBody, "No scenarios here. Add JSON files to scenarios/ (docs/scenarios.md).",
                    theme::kCreamSoft);
        Footer("ESC  BACK");
        return;
    }
    if (std::none_of(shown.begin(), shown.end(), [](const skaar::Scenario* s) { return s->id == ui.selectedScenario; })) {
        ui.selectedScenario = shown.front()->id;
    }

    // list (scrolls)
    const Rect view{m - 40, 232, kListW + 40, h - 232 - 130 - (data.scenarioErrors.empty() ? 0 : 60)};
    const int firstId = ui.focusCount;
    float focusTop = -1, focusBottom = -1;
    for (size_t i = 0; i < shown.size(); i++) {
        if (ui.focus == firstId + static_cast<int>(i)) {
            focusTop = i * kRowH;
            focusBottom = focusTop + kRowH;
        }
    }
    const float scroll = Scroll("play", view, shown.size() * kRowH, focusTop, focusBottom);

    for (size_t i = 0; i < shown.size(); i++) {
        const skaar::Scenario& s = *shown[i];
        const float y = view.y + i * kRowH - scroll;
        const Rect r{m - 40, y, kListW + 40, kRowH};
        const bool visible = y >= view.y - 1 && y + kRowH <= view.Bottom() + 1;
        // keep focus ids stable even for rows scrolled out of view
        const int id = visible ? Focusable(r) : ui.focusCount++;
        if (!visible) {
            continue;
        }
        const bool focused = Focused(id);
        if (focused) {
            ui.selectedScenario = s.id;
        }
        const bool selected = s.id == ui.selectedScenario;
        const float baseline = y + kRowH / 2 + 10;

        if (selected) {
            kit::Fill(r.x, r.y, r.w, r.h, theme::kCream.Alpha(0.035f));
            kit::Fill(m - 28, baseline - f.text.CapHeight(type::kRow), 4, f.text.CapHeight(type::kRow), kEmber);
        }
        f.mono.Draw(m, baseline - 1, 17, skaar::Va("%02zu", i + 1), selected ? theme::kCreamSoft : theme::kCreamFaint,
                    theme::kTrackMono);
        f.text.Draw(m + 56, baseline, type::kRow, s.name, selected ? theme::kCream : theme::kCreamSoft);
        Label(m + 420, baseline - 2, s.category, theme::kCreamFaint);
        if (auto it = data.perScenario.find(s.id); it != data.perScenario.end()) {
            f.mono.Draw(m + kListW - 20, baseline, 22, skaar::Va("%.0f", it->second.best), theme::kCreamSoft, 0.01f,
                        Align::kRight);
        } else {
            Label(m + kListW - 20, baseline - 2, "NEW", theme::kCreamFaint, Align::kRight);
        }
        kit::HLine(m, y + kRowH, kListW, theme::kHairline);

        const bool clicked = in.click && r.Contains(in.mx, in.my);
        if (Activated(id, r)) {
            // a click selects (and starts when already selected); Enter starts
            if (!clicked || selected) {
                StartScenario(s.id);
                return;
            }
            ui.selectedScenario = s.id;
        }
    }
    if (!data.scenarioErrors.empty()) {
        const auto& [id, error] = data.scenarioErrors.front();
        Label(m, h - 150, skaar::Va("%zu SCENARIO FILE(S) NOT LOADED  ·  %s: %s", data.scenarioErrors.size(), id.c_str(),
                                    error.c_str()),
              kEmber);
    }

    // detail panel
    if (const skaar::Scenario* s = data.FindScenario(ui.selectedScenario)) {
        const float px = m + kListW + 100;
        DrawDetail(*s, px, 150, w - m - px);
    }
    Footer("↑ ↓  SCENARIO     ENTER  START     TAB  NEXT     ESC  BACK");
}

}  // namespace ui
