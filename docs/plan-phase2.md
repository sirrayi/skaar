# Phase 2: the robust version of Phase 1

Decided by the human 2026-10-22. The phases are now:
- **Phase 1** (P0–P6): solo lobby, aim trainer, CPM movement, duel practice. Built, playable, not hardened.
- **Phase 2** (this plan): the same scope, made robust. No new modes, weapons or physics.
- **Phase 3**: branch out: multiplayer (ioq3 netcode), tac ruleset (AK-47 + Deagle), surf, real bots, and more.

Phase 2 is done when a stranger can download it, set their sensitivity, play for an hour across the trainer,
movement and arena without a crash, a wrong number or a confusing screen, and their stats are still there next week.

## Workstreams

### A. Correct numbers (the trainer is useless if these are wrong)
- cm/360 verified by the human with a ruler on raw input. Swipe speed must not matter. Same check with the fallback path.
- Hit registration: shot-vs-target tests at high fps, low fps and with frame spikes; hits at the target's edge.
- Sim determinism: the same seed and inputs give the same score. Replays reproduce the run exactly (test on recorded runs).
- Stats: accuracy, TTK, score formulas documented in `docs/scenarios.md` and covered by tests. Arena stats included.

### B. Feel, tuned from playtests
- A playtest log in `docs/playtest.md`: what felt off, the fix, the commit.
- Scenario defaults (sizes, speeds, durations), benchmark tiers calibrated against real runs.
- Movement: the human's feel check of cpm (strafe turns, double jump, ramps, jump pads). Weapon feel in the arena.
- Hit/kill sounds and flashes, crosshair, HUD readability on every map.

### C. Finish what Phase 1 left open
- Lobby: pick vq3/cpm per mode. Weapon refire times from the weapon JSON. Arena sessions in Insights.
- In-game pass over every screen: run summary, replay, insights pages, settings, arena HUD, loading. One visual
  language (`docs/plan-visual.md`), no screen left on the old style.
- Sensitivity converter checked against the published formulas for each listed game.

### D. Robustness
- **Config versioning.** A `sk_cfgVersion` cvar and a migration step for values we change on purpose. Two bugs today
  came from archived cvars outliving their defaults (crosshair colour, tonemap).
- Bad input never crashes or silently misbehaves: malformed scenario/weapon/ruleset JSON shows a clear message in the UI;
  a corrupt or partial `stats/history.jsonl` line is skipped and reported, never loses the file.
- Soak test: a scripted multi-hour session that cycles scenarios, maps and the arena with UBSan (and ASan once the
  toolchain allows), watching memory and frame times.
- Unit tests for every shared module (sim, scenarios, run log, insights, weapons, rulesets); `smoke.sh` runs them.
- A CI workflow for our build (macOS arm64, Linux, Windows) running smoke and the unit tests. The current
  `.github/workflows/build.yml` is upstream ioq3's.
- Performance budget: a fixed frame-time target at 240 Hz with the 2D UI on, measured in `dev_overlay`.
- Dev noise stays out of the player's way: toasts only for real problems, nothing on screen in a release build.

### E. First run and shipping
- First-run flow: set DPI and sensitivity (or convert from another game), FOV, crosshair, then a first scenario.
- Release build: signed and notarized macOS app/DMG (`docs/macos-codesign-notarization.txt`), Linux verified by the
  human, Windows buildable. Versioned, with a changelog.
- Scenario packs: a documented folder where people drop their own JSON, loaded and validated at start.
- Final name and identity. All placeholder sounds/models replaced with own or properly licensed assets (`docs/assets.md`).

## Order
D (config versioning, CI) first, so everything after is protected → A → C → B (needs the human's playtests) → E.
Each item gets its own `docs/plan-*.md` section or file with claims before coding, as in Phase 1.

## Out of scope (Phase 3)
Multiplayer and netcode, tac weapons and movement, surf, bots that fight back, leaderboards, a map editor.

## Claims (parallel sessions)
- **skaar-07**: A (hit registration tests at any input rate and through spikes, edge hits, replay reproduction from
  saved records, score/accuracy/TTK formulas documented and tested, arena stats); C (in-game pass over the run
  summary, replay, insights, settings, arena HUD, loading; lobby pick of vq3/cpm per mode; sensitivity converter
  checked against published formulas); D (bad-input UX: malformed scenario/weapon/benchmark JSON shown in the UI,
  corrupt history lines skipped and reported; unit tests for game/shared modules I own); E (first-run flow,
  scenario packs). Files: game/ui/*, game/cgame/trainer*, game/cgame/weapons*, game/sgame/mode*/weapons_fire/items/
  dummies, game/shared/{scenario,runlog,insights,trainer_sim,charts,weapons,arena,arenalog}.*, tests/test_*.cpp
  (except movement), data/base/{scenarios,benchmarks,weapons}.
- **skaar-47**: D config versioning (sk_cfgVersion + migrations), CI workflow, soak test, performance budget,
  release-build dev noise; E signing / notarized release and versioning.
- Note: P7 tac (rifle, hand cannon, tac scenarios, tacrange) was finished before this plan was read. It stays as
  is, untouched in Phase 2, pending the human's call.
- **skaar-47**: D config versioning (`code/qcommon/cfg_migrate.c`, `docs/config-migrations.md`): done 2026-10-22.
  D CI workflow (`.github/workflows/skaar.yml`), soak test (`scripts/soak.sh`), performance budget (`docs/feel.md` + dev_overlay),
  release-build dev noise (`code/qcommon/dev.c`, `code/client/cl_dev.c`); E signing/notarized release, versioning, changelog.
