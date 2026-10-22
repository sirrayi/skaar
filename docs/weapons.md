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
