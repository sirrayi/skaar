# Plan P5: CPM-style movement v1 (ruleset layer)

Goal: movement becomes a swappable **ruleset** (data + code). Two rulesets ship: `vq3` (stock Q3 behavior, label
"Classic") and `cpm` (strafe-only air turning, air control, ramp and double jumps, ground accel 15; label "Advanced").
The ruleset is chosen per server/mode and is identical in server movement and client prediction.
Constants, behavior and sources: `docs/cpm-physics.md`.

## Design
- `game/shared/move_pmove.cpp`, `move_slide.cpp`: C++20 port of Q3's `bg_pmove.c` / `bg_slidemove.c` (GPL, id headers kept),
  `skaar::move::Pmove(pmove_t*)`. Q3's globals `pm`/`pml` became one `Move` object per step. The ruleset hooks are
  `AirMove` (accel overrides), `AirControl` and `CheckJump` (ramp and double jump). Everything else is a straight port,
  down to the C version's double-precision spots.
- `game/shared/move_ruleset.{h,cpp}`: the `Ruleset` struct, built-ins, JSON parsing, the registry.
  - `ps.stats[14]` = active ruleset index, `ps.stats[15]` = ms left in the double-jump window. Both travel in snapshots,
    so prediction uses the server's values.
  - Server: loads `rulesets/*.json` (name order), picks `g_movement`, and publishes that ruleset's values in configstring
    26 as an info string. JSON's quotes wouldn't survive the quoted `cs` server command. Client: mirrors it at the same
    index. That's netplay-safe, and the client never needs the files.
- `data/base/rulesets/{vq3,cpm}.json`: every value, commented. A new `rulesets/<id>.json` adds a ruleset
  (`/set g_movement <id>`). Keys left out keep vq3's values (or cpm's for `cpm.json`).
- Velocity snapping is a local `std::round`, not the engine syscall, so server and client share one implementation.

## Status
- [x] Research doc with sources (`docs/cpm-physics.md`).
- [x] C++ port + rulesets + JSON + configstring sync, compiled into all modules (inert until wired).
- [x] Tests: `skaar_movetest` in ctest (`cmake/skaar/tests.cmake`, so smoke runs it), or `tools/movetest/run.sh`. 34 checks:
  - **Parity:** vq3 port vs C `bg_pmove.c`, byte-identical playerState over 2120 steps (flat ground, strafe jumps,
    ramps, walls, stairs, crouch).
  - **Physics:** speed cap 320, strafe-jump gain, cpm strafe-only turning 6× vq3, air control (turns, keeps speed ±1%),
    jump 270, double jump 370 inside 400 ms, ceiling-bonk double jump, ramp jump `max(vz, 0) + 270`, full downhill jump.
  - **Determinism**, JSON parsing and validation, registry, configstring round-trip, shipped files == built-ins.
  - Clean under UBSan (safe build).
- [x] `data/maps-src/movement.json`: straight, stairs, 40/80 ledges, low-ceiling tunnel (boxes only).
- [ ] Wiring (skaar-47 applies the patch): sgame/cgame call `skaar::move::Pmove`, `g_movement` cvar, configstring sync,
  `bg_pmove.c`/`bg_slidemove.c` dropped from `modules.cmake`. Built, smoke-passed, and run with `g_movement cpm` on the
  movement map in an isolated copy.
- [ ] Scenario key `"player": {"ruleset": "cpm"}` (skaar-07, `scenario.{h,cpp}`).
- [ ] Ramps (~20°, ~35°) and a jump pad in the movement map: needs sloped brushes and trigger brush entities in
  `tools/mapc` (skaar-47 OK'd skaar-31 editing mapc for this), plus trigger_push handling in sgame.
- [ ] UI: pick the ruleset in the lobby (P2 screens, skaar-07). Final labels need the user's OK.

## How to run
- Tests: `ctest --test-dir build-fast -R skaar_movetest`, or `tools/movetest/run.sh` (standalone).
- In game (after wiring): `/set g_movement cpm`, then `/devmap movement`.

## File claims (parallel sessions, see CLAUDE.md)
- **skaar-31** (this plan, confirmed with skaar-47 2026-10-22): `docs/plan-P5.md`, `docs/cpm-physics.md`,
  `game/shared/move_*.{h,cpp}`, `data/base/rulesets/*`, `data/maps-src/movement.json`, `tools/movetest/*`,
  the `skaar_movetest` block at the end of `cmake/skaar/tests.cmake`, plus `README.md`, `tools/textart.py`.
- Wiring into `game/sgame/*`, `game/cgame/*` and `cmake/skaar/modules.cmake` is applied by skaar-47 from a patch skaar-31 sends.
- Scenario key `"player": {"ruleset": ...}` is parsed by skaar-07 in `scenario.{h,cpp}`.
