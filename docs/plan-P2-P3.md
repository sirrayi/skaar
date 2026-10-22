# Plan P1–P3 build-out: readable menus, aim trainer v1, stats ("insights")

Status 2026-10-22 evening: built. Menus (99e49613), trainer + recording + replay (in trainer.cpp), insights,
settings, HiDPI (de4c8165). Open items: summary/replay screens not yet screenshot-verified in game; benchmark
thresholds uncalibrated; no unit-test target yet (logic was tested with standalone harnesses).

Background: P1 (`plan-P1.md`) was only planned. The client still ran the stock Q3 `q3_ui`, which draws Q3 art we
don't ship, so the menus were unreadable. This plan does P1, P2 and P3 together, plus an insights section inspired
by RefleK's (a GPLv3 KovaaK's stats tracker). We take ideas only, no code: overview dashboard, run history, run
analysis charts, mouse-trajectory view, benchmarks with a "focus next" pick, and run replays.

## Shape
```
game/shared/  q3 bridge, 2D kit (fonts, rects, lines, theme), scenario + benchmark parsing, run records (JSON)
game/sgame/   qagame: spawn, pmove (bg_pmove.c), noclip, freeze for static scenarios, reset to spawn
game/cgame/   cgame: snapshots, prediction, view, HUD, crosshair, the trainer (client-side), run recording, replay
game/ui/      ui: main menu, play (scenario browser), insights, settings, in-game menu, loading screen
```

### The trainer is client-side (cgame)
Solo play, so the cgame owns targets: it simulates them from the scenario seed and time, and evaluates every usercmd
(not every frame) against them with that command's own view angles. Hits register at input resolution with zero
network delay. The server only moves the player.

### Run records
- `stats/runs/<scenario>__<YYYYMMDD-HHMMSS>.json`: full record (summary, every shot with its angular error,
  kills with TTK, view-angle samples at 60 Hz, seed). Enough to replay the run.
- `stats/history.jsonl`: one summary line per run, appended. The insights screens load only this file.

### Insights (ui)
| Screen | Shows |
|---|---|
| Overview | latest run, day streak, today's session (runs, time), stat cards, benchmark focus |
| Scenarios | per-scenario history: score curve with PB line, accuracy and TTK curves, last runs table, pin/compare |
| Run | one run: stats, kill timeline, shot dispersion around the target, view-angle trajectory, replay button |
| Benchmarks | tiers per scenario, progress, the recommended next scenario |

## Scenarios (data-driven, `data/base/scenarios/*.json`)
Click: wall grid, micro flick, wide flick, pop (timed). Track: strafe track, float track. Switch: trio switch.

## Done when
- Menus are readable at 1080p and on Retina. Mouse and keyboard both work.
- Play → pick a scenario → countdown → run → summary. R restarts. Esc opens the in-game menu.
- Each run writes a record, and insights show it right away.
- `./scripts/smoke.sh` passes. No missing-asset warnings with the new modules.

## File claims (parallel sessions, see CLAUDE.md)
Session **skaar-07** owns these. Other sessions: read them, don't edit them. Message skaar-07 for changes.
- `game/ui/*`
- `game/shared/scenario.{h,cpp}`, `game/shared/runlog.{h,cpp}`, `game/shared/insights.{h,cpp}`
- `data/base/scenarios/*`, `data/base/benchmarks/*`
- `docs/plan-P2-P3.md`, `docs/insights.md`, `docs/scenarios.md`

skaar-47 owns P1 (`game/sgame`, `game/cgame`, `game/shared/q3.h`, `draw2d.*`, `theme.h`, `cmake/skaar/modules.cmake`).
The trainer (`game/cgame/trainer*.cpp`) is agreed with skaar-47 before it's written.
- skaar-31: `README.md`, `tools/textart.py`
