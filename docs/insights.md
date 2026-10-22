# Insights

Every finished run is saved, and the Insights screen turns the runs into answers: how today went, whether you're
improving, where your shots miss, and what to train next. The idea comes from RefleK's (a GPLv3 KovaaK's
tracker). Skaar shares no code with it. It's built into the game, so it needs no screen capture or log parsing.

Open it from the main menu (Insights), the pause menu, `I` on the end-of-run summary, or `skaar_ui insights`.

## Screens
| Tab | Shows |
|---|---|
| Overview | Streak (days in a row with a run), today, this session (runs with gaps under 30 min), all time, personal bests in the last 7 days. The latest run with a verdict against your best and average. Benchmark standing and the scenario to focus on next. Recent runs. |
| Scenarios | Per scenario: best, average of the last 10, trend (last 10 vs the 10 before), runs, time played. Score per run (hover a point for its date, click to open the run), accuracy and mean TTK over runs. |
| Run | One run: score and verdict, accuracy, kills, mean TTK, hits, cm/360 and FOV at the time. Time to kill over the run (with a rolling mean, and whether you slowed down), shot dispersion around the target in target radii (mean point of impact, drift direction), the aim path (view angles over the whole run, kills marked), view speed. Replay plays it back in game. |
| Benchmarks | Each scenario's best, tier and progress on the tier ladder. The focus pick (your lowest standing, ties go to the least recently played) and your weakest category. |

## Replays
`sk_replay <stats/runs/file.json>` (or Replay on the Run tab) plays a run back in the game: the camera follows the
recorded view at 60 Hz, and the targets re-simulate from the run's seed and recorded kills, so they spawn and move
as they did. When it ends, the Run tab opens.

## Files
In the home dir's `base/` (`~/Library/Application Support/Skaar/base/` on macOS):
- `stats/history.jsonl`: one line per run (scenario, time, score, accuracy, kills, TTK, cm/360, FOV, record file).
  The overview, trends and benchmarks read only this.
- `stats/runs/<scenario>__<YYYYMMDD-HHMMSS>.json`: the full record. Every shot with its aim error, every kill
  with its TTK, view samples at 60 Hz, the seed. Format: `game/shared/runlog.h`.

Both are plain JSON. Back them up or analyse them elsewhere as you like. Deleting a record keeps the run in the
history. Its Run tab then says the file is missing.

## Code
| Piece | Where |
|---|---|
| Records, history | `game/shared/runlog.*` |
| Streak, sessions, trends, tiers, focus pick | `game/shared/insights.*` (pure functions) |
| Charts | `game/shared/charts.*` |
| Screens | `game/ui/screen_insights.cpp` |
| Recording, replay | `game/cgame/trainer.cpp` |
