// What the server tells the client about arena entities (P6): items and
// practice dummies travel as ET_ITEM / ET_GENERAL entities, and these values
// say which is which. Shared by sgame (mode, items, dummies) and cgame
// (weapons: drawing).
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <cstring>

namespace skaar::arena {

// entityState_t.generic1 on arena entities
enum EntityKind { kKindNone = 0, kKindItem = 1, kKindDummy = 2 };

enum class ItemType { kWeapon, kAmmo, kHealth, kArmor };

struct ItemDef {
    const char* classname;  // map entity class (Q3 names, so Q3-style maps work)
    ItemType type;
    int slot;               // weapon / ammo: weapon slot (WP_*)
    int amount;             // health / armor points, or ammo (0: the weapon's ammoPickup)
    int max;                // health / armor: the cap this pickup can fill to
    int respawnMs;
};

// entityState_t.modelindex on items = index into this table
inline constexpr ItemDef kItems[] = {
    {"weapon_shotgun", ItemType::kWeapon, 3, 0, 0, 5000},
    {"weapon_rocketlauncher", ItemType::kWeapon, 5, 0, 0, 5000},
    {"weapon_lightning", ItemType::kWeapon, 6, 0, 0, 5000},
    {"weapon_railgun", ItemType::kWeapon, 7, 0, 0, 5000},
    {"ammo_shells", ItemType::kAmmo, 3, 0, 0, 40000},
    {"ammo_rockets", ItemType::kAmmo, 5, 0, 0, 40000},
    {"ammo_lightning", ItemType::kAmmo, 6, 0, 0, 40000},
    {"ammo_slugs", ItemType::kAmmo, 7, 0, 0, 40000},
    {"item_health_small", ItemType::kHealth, 0, 5, 200, 35000},
    {"item_health", ItemType::kHealth, 0, 25, 100, 35000},
    {"item_health_large", ItemType::kHealth, 0, 50, 100, 35000},
    {"item_health_mega", ItemType::kHealth, 0, 100, 200, 35000},
    {"item_armor_shard", ItemType::kArmor, 0, 5, 200, 25000},
    {"item_armor_combat", ItemType::kArmor, 0, 50, 200, 25000},
    {"item_armor_body", ItemType::kArmor, 0, 100, 200, 25000},
};
inline constexpr int kNumItems = static_cast<int>(sizeof(kItems) / sizeof(kItems[0]));

inline int FindItem(const char* classname) {
    for (int i = 0; i < kNumItems; i++) {
        if (!std::strcmp(kItems[i].classname, classname)) {
            return i;
        }
    }
    return -1;
}

// Practice dummies: entityState_t.modelindex = pattern, time2 = health
enum class DummyPattern { kStand = 0, kStrafe = 1, kStrafeJump = 2, kCircle = 3 };

constexpr int kMaxHealth = 100;   // spawn health; health and armor pickups go to 200
constexpr int kRespawnMs = 1500;  // after a death

}  // namespace skaar::arena
