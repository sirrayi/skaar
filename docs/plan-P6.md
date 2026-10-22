# Plan P6: duel practice (arena, weapons, dummies)

BRIEF §9 P6: "One test arena map, dummy bots, basic weapons (rail, rocket, lightning, shotgun) using Q3's existing
weapon code." Movement comes from P5 (`g_movement`, vq3 | cpm).

Status 2026-10-22 evening: A–G done and playable (main menu › Arena, or `map duel`). Left: weapon refire times still
come from pmove's table, not the JSON `refireMs` (needs a hook in skaar-31's move_pmove); arena sessions aren't saved
to insights yet (needs a server-side file write); dummies don't shoot back.

## Scope
1. **Weapons, data-driven.** `data/base/weapons/<id>.json` (rail, rocket, lightning, shotgun): damage, splash,
   projectile speed, range, pellets/spread, ammo, refire. The defaults match Q3 (GPL code we may reuse):
   rail 100 / 1500 ms, rocket 100 (+120 splash, r 120) at 900 u/s / 800 ms, lightning 8 per 50 ms to 768 u,
   shotgun 11 × 10 spread 700 / 1000 ms.
   Firing follows the pmove weapon state machine (`EV_FIRE_WEAPON`). The server does hitscan traces, rocket missiles,
   splash damage and knockback. The client draws the rail trail, the lightning beam, rockets, explosions and impacts
   (generated sprites and sounds, no Q3 assets).
2. **Damage, health, armor, death, respawn.** One `mode::Damage` entry point (Q3 armor rules: armor absorbs 2/3),
   self-damage and rocket jumps, respawn after 1.5 s at the spawn farthest from the attacker.
3. **Items.** Weapons, ammo, health (+25, +50, mega +100 over max that ticks down), armor (+50, +100). Respawn timers.
   Pickup by box overlap.
4. **Dummies (not botlib: mapc makes no .aas).** Server entities with a player box, health and armor. They run on
   patterns: stand, strafe (A/D with random reversals), strafe-jump, circle. They respawn and can hold weapons as
   visuals. Practice targets, not opponents. Botlib bots are a later decision.
5. **Arena map** `data/maps-src/duel.json`: two-level boxes, stairs, jump pads (P5/skaar-47), item spawns.
6. **Mode + HUD.** "Duel practice" in the Play screen: health / armor / ammo / weapon bar, per-weapon accuracy and
   damage. Each session is saved to `stats/` like trainer runs, so Insights can show weapon accuracy over time.

## Order
A weapons JSON + parsing + tests → B server firing (rail, shotgun, lightning) + damage → C rockets + splash →
D client effects + sounds → E items → F dummies → G arena map → H HUD + mode entry + stats.

## File claims (skaar-07)
- `docs/plan-P6.md`, `docs/weapons.md`
- `data/base/weapons/*`, `data/maps-src/duel.json`, `data/base/scripts/weapons.shader`
- `game/shared/weapons.{h,cpp}` (definitions, parsing)
- `game/sgame/mode.{h,cpp}`, `game/sgame/weapons*.cpp`, `game/sgame/dummies.cpp`, `game/sgame/items.cpp`
  (`mode.*` stubs are wired into the sgame loop by skaar-47, then handed over)
- `game/cgame/weapons*.cpp` (effects, HUD; hooks to be agreed with skaar-47)
- new functions in `tools/gen_assets.py` for weapon sprites, sounds and the dummy model

Hooks in other sessions' files (skaar-47: sgame clients/main, cgame loop) are requested, not edited.
