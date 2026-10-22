# Plan P5: CPM-style movement v1 (ruleset layer)

Goal: movement becomes a swappable **ruleset** (data + code). Two rulesets ship: `vq3` (stock Q3 behavior)
and `cpm` (strafe jumping with higher air control, double jump, ramp jump, CPM-style accel/friction).
The ruleset is chosen per mode/scenario and is identical on the server and in client prediction.

Status: research first (BRIEF §9 P5). Constants and sources go in `docs/cpm-physics.md` before code.

## File claims (parallel sessions, see CLAUDE.md)
- **skaar-31** (this plan, confirmed with skaar-47 2026-10-22): `docs/plan-P5.md`, `docs/cpm-physics.md`,
  `game/shared/move_*.{h,cpp}`, `data/base/rulesets/*`, `data/maps-src/movement.json`, `tools/movetest/*`,
  plus `README.md`, `tools/textart.py`.
- Wiring into `game/sgame/clients.cpp`, `game/cgame/predict.cpp`, `cmake/skaar/modules.cmake` and a smoke step in
  `scripts/smoke.sh` is applied by skaar-47 from a diff skaar-31 sends.
- Scenario key `"player": {"ruleset": ...}` is parsed by skaar-07 in `scenario.{h,cpp}`.
