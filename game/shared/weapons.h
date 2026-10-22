// Weapons as data: data/base/weapons/<id>.json, shared by the server (firing,
// damage) and the client (effects, HUD). Format: docs/weapons.md.
//
// Each weapon sits in one of the engine's weapon slots (playerState weapon
// numbers, WP_* in bg_public.h), which the pmove weapon state machine and the
// network protocol use. The built-in four keep Q3's slots.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace skaar {

enum class FireType {
    kHitscan,     // one trace; may pierce (rail)
    kPellets,     // several spread traces (shotgun)
    kBeam,        // short-range trace every refire (lightning)
    kProjectile,  // a missile that flies and explodes (rocket)
};

struct WeaponDef {
    std::string id;          // file name
    std::string name;        // shown in the HUD
    int slot = 0;            // WP_* number, 1..15
    FireType type = FireType::kHitscan;

    float damage = 0;        // per hit / pellet / beam tick / direct missile hit
    float splashDamage = 0;  // projectile: at the centre of the explosion
    float splashRadius = 0;
    float range = 8192;      // hitscan and beam reach
    float speed = 0;         // projectile units per second
    int pellets = 1;         // kPellets
    float spread = 0;        // kPellets: Q3 units of spread at 8192 (DEFAULT_SHOTGUN_SPREAD = 700)
    int pierce = 1;          // kHitscan: bodies one trace can pass through (rail: 4)
    int refireMs = 1000;     // time between shots
    float knockback = 1;     // scale on damage-derived knockback

    int ammoStart = 0;       // given with the weapon
    int ammoMax = 200;
    int ammoPickup = 10;     // from an ammo box

    // ---- tac weapons (P7, docs/plan-P7.md); all off by default ----
    bool semiAuto = false;       // one shot per press
    bool tracer = false;         // the client draws a thin tracer instead of a rail core
    float firstShotSpread = 0;   // degrees, the first shot after the spray reset
    float inaccuracy = 0;        // degrees, standing, after the first shot
    float moveInaccuracy = 0;    // degrees added at moveSpeedRef horizontal speed (scales linearly)
    float moveSpeedRef = 250;
    float airInaccuracy = 0;     // degrees added while airborne
    float crouchScale = 1;       // multiplies the total while crouched
    float headMultiplier = 1;    // a hit in the top 12 units of a body
    float falloffStart = 0, falloffEnd = 0, falloffMin = 1;  // damage scale over distance (end 0: none)
    int magazine = 0;            // rounds per magazine (0: no magazine, ammo is the reserve)
    int reloadMs = 0;
    // spray pattern: per-shot [up, right] kick in degrees; shot i aims at the sum of kicks 0..i-1
    std::vector<std::array<float, 2>> recoil;
    int recoilRecoverMs = 0;     // a pause this long resets the spray to shot 0
    float cameraFollow = 0.5f;   // share of the spray offset that also moves the view (the rest is bullets only)
};

// ---- tac math (deterministic: same inputs on server and client) ----

// Aim offset of shot `index` in the spray, degrees: up (+) and right (+).
void SprayOffset(const WeaponDef& w, int index, float& up, float& right);

// Cone half-angle in degrees for a shot.
float Inaccuracy(const WeaponDef& w, bool firstShot, float horizontalSpeed, bool onGround, bool crouched);

// A point in the cone, from an integer seed: up / right in degrees, uniform over the disc.
void SpreadSample(float coneDeg, int seed, float& up, float& right);

// Damage scale at a distance (falloff), 1 when no falloff is set.
float FalloffScale(const WeaponDef& w, float distance);

bool ParseWeapon(std::string_view id, std::string_view json, WeaponDef& out, std::string& error);

// Registry, filled by each module from data/base/weapons (built-in defaults if a file is missing)
void ResetWeapons();              // the four built-ins: shotgun, rocket, lightning, rail
void AddWeapon(const WeaponDef& weapon);  // replaces one with the same id or slot
const WeaponDef* WeaponBySlot(int slot);
const WeaponDef* WeaponById(std::string_view id);
const std::vector<WeaponDef>& Weapons();

}  // namespace skaar
