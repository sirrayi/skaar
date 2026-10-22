// Weapons as data: data/base/weapons/<id>.json, shared by the server (firing,
// damage) and the client (effects, HUD). Format: docs/weapons.md.
//
// Each weapon sits in one of the engine's weapon slots (playerState weapon
// numbers, WP_* in bg_public.h), which the pmove weapon state machine and the
// network protocol use. The built-in four keep Q3's slots.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

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
};

bool ParseWeapon(std::string_view id, std::string_view json, WeaponDef& out, std::string& error);

// Registry, filled by each module from data/base/weapons (built-in defaults if a file is missing)
void ResetWeapons();              // the four built-ins: shotgun, rocket, lightning, rail
void AddWeapon(const WeaponDef& weapon);  // replaces one with the same id or slot
const WeaponDef* WeaponBySlot(int slot);
const WeaponDef* WeaponById(std::string_view id);
const std::vector<WeaponDef>& Weapons();

}  // namespace skaar
