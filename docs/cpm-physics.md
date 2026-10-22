# CPM-style movement: research notes

Goal (BRIEF §9, P5): a "CPM-style" movement ruleset next to the stock Q3 one ("vq3"), selectable per mode.
CPMA itself is closed source. Nothing here comes from its code. Everything below is **documented behaviour**
from public write-ups and published configs, re-implemented by us in a C++20 port of the GPL Q3 `bg_pmove.c`
(`game/shared/move_*`, see `docs/plan-P5.md`).
**Licenses:** Xonotic (S2) is GPLv3+. Skaar is GPLv2-or-later, so Xonotic is a behaviour reference only and none of its
code is copied. Same for anything derived from the CPMA dev docs (S5: read the README only, not its source).
"CPM", "CPMA" and "Quake" are internal reference names only. Ruleset ids are `vq3` and `cpm` internally. UI labels are proposed as **classic** / **promode**, pending the user's OK
("promode" is close to "Challenge ProMode"; a neutral label may be safer).

## Sources
| # | Source | What it gives us |
|---|---|---|
| S1 | `code/game/bg_pmove.c`, `bg_local.h` (id Software, GPLv2), in this repo | The vq3 baseline: every constant and the air/walk/jump flow |
| S2 | Xonotic `physicsCPMA.cfg` (GPL game data): https://gitlab.com/xonotic/xonotic-data.pk3dir/-/raw/master/physicsCPMA.cfg | CPM-approximating values, several annotated as taken from the "CPMA 1 dev docs" |
| S3 | ZdrytchX, *Challenge ProMode (CPM) Physics Tutorial*: https://sites.google.com/site/zdrytchx/how-2s-tutorials-and-information/challenge-promode-cpm-physics-tutorial-information | Which keys trigger which air behaviour; double jump 400 ms, +100 on 270 |
| S4 | Nexuiz commits r6686 / r8594 (GPL), describing the air move and the CPM-style air control: https://icculus.org/pipermail/nexuiz-commits/2009-May/004971.html, https://icculus.org/pipermail/nexuiz-commits/2010-January/006852.html | Order of the air-accel overrides; the air control formula (32 × aircontrol × dot² × frametime) |
| S5 | freepromode README (GPLv2 OpenArena mod): https://github.com/oitzujoey/freepromode | Independent list of the CPM features: air control, strafe accelerate, wishspeed 30, double jump within 400 ms, ramp boost |
| S6 | *More Than You Ever Wanted to Know About Air Strafing* (Steam guide) and the QuakeWorld wiki "QW physics air": https://steamcommunity.com/sharedfiles/filedetails/?id=184184420, https://quakeworld.nu/wiki/QW_physics_air | Why a low wishspeed cap with high accel gives QW-style air strafing |
| S7 | Play More Promode CPMA guide: https://playmorepromode.com/guides/cpma-tdm-gameplay | Player-facing description: double jump within 400 ms, ramp jumps give extra height |

## Constants
Q3 units: 1 unit ≈ 1 inch, speeds in units/s ("ups"), accelerations in the dimensionless Q3 form (see the accelerate formula below).

| Parameter | classic (vq3) | promode (cpm) | Source |
|---|---|---|---|
| Ground max speed | 320 | 320 | S1, S2 |
| Ground accelerate | 10 | **15** | S1; S2 ("CPMA 1 dev docs") |
| Ground friction | 6 | **6** (dev docs: 8; modern CPM: 6) | S1; S2 comment |
| Stop speed (friction floor) | 100 | 100 | S1, S2 |
| Air accelerate | 1 | 1 | S1, S2 |
| Air stop accelerate (when wish opposes velocity) | n/a (1) | **2.5** | S2 ("dev docs"), S4 |
| Air strafe accelerate (strafe keys only) | n/a | **70** | S2 ("strafeaccelerate" in dev docs), S3 ("1→70") |
| Air strafe wishspeed cap (strafe keys only) | n/a (no cap) | **30** | S2 ("wishspeed"), S3 ("10→1 m/s" in its scale), S5 |
| Air control | 0 | **150**, dot power 2, forward and backward | S2 ("dev docs"; flags: backwards on), S4 |
| Jump velocity | 270 | 270 | S1, S2, S3 |
| Double jump | none | **+100** if the jump comes within **400 ms** of the previous one | S3, S5, S7 (400 ms is well sourced; +100 is community-derived in S3, tunable) |
| Ramp jump | vertical velocity reset to 270 | **kept**: vz = max(vz, 0) + 270 | S2 (jumpspeedcap_min 0, no max, "CPMA has ramp jumping"), S5, S7 |
| Step height | 18 | 18 | S1, S2 |
| Gravity | 800 | 800 | S1 (server g_gravity) |

## Behaviour, precisely
**Accelerate** (Q3, S1): `add = wishspeed − dot(v, wishdir)`; if `add > 0`, then
`v += wishdir × min(add, accel × frametime × wishspeed)`. With a small wishspeed cap and a large accel (cpm strafe-only),
`add` stays positive at wide angles, which is what gives QW-style air strafing (S6).

**Air move** (S2, S4), in order:
1. `accel = airAccelerate` (1).
2. If `dot(v, wishdir) < 0` and airStopAccelerate > 0: `accel = airStopAccelerate` (2.5).
3. If only strafe keys are held (forward = 0, right ≠ 0): `wishspeed = min(wishspeed, 30)`, `accel = 70`.
4. Accelerate with (wishdir, wishspeed, accel).
5. Air control (below) with the uncapped wishspeed.
6. Clip to a steep ground plane and slide, as vq3.

Diagonal input (forward + strafe) gets neither override, so classic strafe jumping works the same in both rulesets (S3).

**Air control** (S3, S4): only with forward-only input (or back-only, since backward control is on), and wishspeed > 0.
- Save `zspeed = v.z`, set `v.z = 0`, `speed = |v|`, then normalize v.
- `dot = v · wishdir`, `k = 32 × airControl × dot^power × frametime` (power 2).
- If `dot > 0`: `v = normalize(v × speed + wishdir × k)`. The direction turns toward the view; nothing changes when moving away from it.
- `v = v × speed`, `v.z = zspeed`. Horizontal speed and vertical velocity are preserved.
S3: "the faster you move, the slower you turn", which follows from the fixed k against a larger `speed`.

**Jump** (S1 + S3/S5/S7):
- vq3: `v.z = 270`, and jump must be released between jumps (PMF_JUMP_HELD).
- cpm: `v.z = max(v.z, 0) + 270`, plus 100 if the previous jump was less than 400 ms ago. Running up a ramp, walking has already
  clipped velocity to the slope, so `v.z > 0` carries into the jump (ramp jump). Downhill, `v.z` is clamped to 0 first.
- The 400 ms window lives in the player state (`STAT_JUMPTIME`) so server and client prediction agree.
- Auto-hop (jumping again while jump is held) is a separate ruleset flag, off in both by default. CPMA servers vary.

## Not replicated (documented, deliberately left out of v1)
- CPM skipping the wall velocity clip during the double-jump timer (S3). Exotic, and needs care to avoid tunnelling. Revisit after play-tests.
- Weapon switch speed, item and damage rules: not movement.
- Crouch-slide variants and Warsow-style dash: not CPM.

## How we verify (P5 tests)
`tools/movetest` (headless), against a plane/ramp trace stub with the C++ pmove, both rulesets:
1. Ground speed caps at 320 in both rulesets.
2. Classic: optimal-angle strafe jumping gains speed past 320 and keeps gaining over successive jumps.
3. Promode: strafe-only air input turns the velocity several times faster than classic without losing speed.
   (It does not *gain* speed faster: per frame vq3 adds 2·2.56·317.4 to v², cpm 2·16.8·13.2.)
4. Promode air control turns velocity toward the view and keeps horizontal speed within 1%.
5. Promode double jump: 370 inside 400 ms, 270 outside. Ramp jump adds the upward velocity.
6. Determinism: the same inputs give a byte-identical playerState.
