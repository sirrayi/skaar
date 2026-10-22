# Weapons

Weapons are JSON files in `data/base/weapons/<id>.json` (or `weapons/` in your home dir). A file for a built-in
(`shotgun`, `rocket`, `lightning`, `rail`) only needs the values it changes. A new weapon needs a `slot`.

```jsonc
{
  "name": "Rail",            // HUD name
  "slot": 7,                 // engine weapon slot 1..15 (WP_* in code/game/bg_public.h); one weapon per slot
  "type": "hitscan",         // hitscan | pellets | beam | projectile
  "damage": 100,             // per hit, pellet, beam tick or direct missile hit
  "splashDamage": 0,         // projectile: at the centre of the explosion, falling off to 0 at splashRadius
  "splashRadius": 0,
  "range": 8192,             // hitscan / beam reach
  "speed": 0,                // projectile, units per second
  "pellets": 1, "spread": 0, // pellets: count and Q3-style spread (units at 8192 out; the shotgun's is 700)
  "pierce": 4,               // hitscan: bodies one shot can pass through
  "refireMs": 1500,
  "knockback": 1,            // scale on the damage-based push
  "ammo": { "start": 10, "max": 200, "pickup": 10 }
}
```
Defaults are Q3's (GPL, `code/game/g_weapon.c`): rail 100 every 1.5 s, rocket 100 + 100 splash in 120 u at 900 u/s
every 0.8 s, lightning 8 every 50 ms out to 768 u, shotgun 11 pellets of 10 every second.
The parser and registry are in `game/shared/weapons.*`. The plan is in `docs/plan-P6.md`.

## Tac weapons (P7)
Extra fields, all off by default. `rifle.json` and `handcannon.json` use them:
```jsonc
{
  "semiAuto": true,                  // one shot per press
  "tracer": true,                    // thin travelling tracer instead of a rail core
  "firstShotSpread": 0.05,           // degrees, the first shot of a spray
  "inaccuracy": 0.9,                 // degrees, every later shot, standing
  "moveInaccuracy": 5,               // degrees added at moveSpeedRef horizontal speed (linear, up to 1.5x)
  "moveSpeedRef": 250,
  "airInaccuracy": 10,               // degrees added in the air
  "crouchScale": 0.7,                // times the total while crouched
  "headMultiplier": 2.6,             // a hit in the top 12 units of a body
  "falloff": [600, 1600, 0.7],       // damage scale from 1 at 600 u down to 0.7 at 1600 u
  "magazine": 7, "reloadMs": 2200,   // with a magazine, `ammo` is the reserve; R (reload) or auto when empty
  "recoil": {
    "pattern": [[2.2, 0.1], [2.0, -0.35]],  // per shot [up, right] kick, degrees; shot n aims at the sum of the first n
    "recoverMs": 360,                // a pause this long starts the spray over
    "cameraFollow": 0.6              // share of the kick the view shows (the rest moves only the bullets)
  }
}
```
Shots are deterministic, from the command time and the shot index (`game/shared/weapons.h`: `SprayOffset`,
`Inaccuracy`, `SpreadSample`, `FalloffScale`). Server hits and client effects agree, and the trainer uses the same
math. A scenario with `"weapon": "rifle"` fires that gun (docs/scenarios.md).
