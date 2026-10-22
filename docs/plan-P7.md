# Plan P7: tac-shooter mode (rifle + hand cannon)

CLAUDE.md product direction: tac-shooter feel, starting minimal with two weapons (inspired by the AK-47 and the
Deagle; in the product they're "Rifle" and "Hand cannon"). Weapons stay data-driven: recoil patterns, spread,
movement inaccuracy, counter-strafing. Status: proposed 2026-10-22.

## Mechanics (all in the weapon JSON, docs/weapons.md)
| Field | Meaning |
|---|---|
| `spread` (deg) | Base inaccuracy, standing still. `firstShotSpread` for the first shot after a pause. |
| `moveSpread` (deg) | Added at full run speed, scaled by horizontal speed / `moveSpeedRef`. This is what makes counter-strafing (stopping before you shoot) matter. |
| `airSpread` (deg) | Added while airborne. `crouchScale` multiplies the total while crouched. |
| `recoil` | Per-shot `[pitch, yaw]` offsets in degrees, cumulative (the spray pattern). `recoilRecoverMs` resets the shot index after a pause. `cameraFollow` (0..1) is how much of the offset moves the view, the rest only moves the bullets. |
| `damage`, `headMultiplier`, `falloff` | Body damage, head multiplier (head = top 12 u of the box), damage falloff `[start, end, minScale]` over distance. |
| `magazine`, `reloadMs` | Magazine and reload. `reload` command, plus auto-reload when empty. |

Starting values (first guesses, tuned by play-testing):
- **Rifle**: 600 rpm (100 ms), 36 body / ×4 head, magazine 30, reload 2.5 s. First shot 0.1°, standing 0.25°,
  moving +4°, air +8°. A 30-shot pattern that climbs ~7° over the first 10 shots, then swings left and right.
- **Hand cannon**: semi-auto, 260 ms, 55 body / ×2.6 head, magazine 7, reload 2.2 s. First shot 0.05°,
  moving +5°. Each shot kicks 2.5° up, and it recovers in 350 ms.

## Pieces
1. WeaponDef fields + parsing + tests (`game/shared/weapons.*`).
2. Server: hitscan with spread, pattern offsets and head zones, mag/reload (`game/sgame/weapons_fire.cpp`, mode).
   Shot index and recoil state per client.
3. Client: view recoil (cameraFollow) applied in prediction-safe form (the server mirrors the same pattern from the
   shot index), tracers and impact puffs, ammo/mag HUD, reload.
4. A **tac ruleset** for movement (skaar-31: `data/base/rulesets/tac.json`, ~250 u/s run, high ground friction
   and accel, no strafe-jump gains, small jump).
5. **Range mode**: map `tacrange` with dummies at 10/20/40 m, standing and strafing (dummy patterns exist).
6. Trainer scenarios: "Spray control" (hold fire on a far wall target; score = hits within the head zone) and
   "Counter-strafe" (strafing player, click targets: only first shots after a stop are accurate).

## File claims (skaar-07, pending OK from skaar-47 / skaar-31)
`docs/plan-P7.md`, `data/base/weapons/rifle.json`, `handcannon.json`, `data/maps-src/tacrange.json`, tac scenario
JSONs. The weapons, mode and cgame weapons files are already mine (P6). The tac ruleset file is requested from skaar-31.
