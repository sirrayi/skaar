// This file is part of Skaar. GPLv2 or later.
#include "weapons.h"

#include <algorithm>
#include <stdexcept>

#include "json.hpp"

namespace skaar {
namespace {

using json = nlohmann::json;

// Q3's values (code/game/g_weapon.c, bg_pmove.c PM_Weapon; GPL), slots as WP_* in bg_public.h
std::vector<WeaponDef> BuiltIns() {
    WeaponDef shotgun;
    shotgun.id = "shotgun";
    shotgun.name = "Shotgun";
    shotgun.slot = 3;
    shotgun.type = FireType::kPellets;
    shotgun.damage = 10;
    shotgun.pellets = 11;
    shotgun.spread = 700;
    shotgun.refireMs = 1000;
    shotgun.ammoStart = 10;
    shotgun.ammoPickup = 10;

    WeaponDef rocket;
    rocket.id = "rocket";
    rocket.name = "Rocket";
    rocket.slot = 5;
    rocket.type = FireType::kProjectile;
    rocket.damage = 100;
    rocket.splashDamage = 100;
    rocket.splashRadius = 120;
    rocket.speed = 900;
    rocket.refireMs = 800;
    rocket.ammoStart = 10;
    rocket.ammoPickup = 5;

    WeaponDef lightning;
    lightning.id = "lightning";
    lightning.name = "Lightning";
    lightning.slot = 6;
    lightning.type = FireType::kBeam;
    lightning.damage = 8;
    lightning.range = 768;
    lightning.refireMs = 50;
    lightning.ammoStart = 100;
    lightning.ammoPickup = 60;

    WeaponDef rail;
    rail.id = "rail";
    rail.name = "Rail";
    rail.slot = 7;
    rail.type = FireType::kHitscan;
    rail.damage = 100;
    rail.pierce = 4;
    rail.refireMs = 1500;
    rail.ammoStart = 10;
    rail.ammoPickup = 10;
    return {shotgun, rocket, lightning, rail};
}

std::vector<WeaponDef>& Registry() {
    static std::vector<WeaponDef> weapons = BuiltIns();
    return weapons;
}

template <typename T>
T Get(const json& j, const char* key, T fallback) {
    auto it = j.find(key);
    return it != j.end() && !it->is_null() ? it->get<T>() : fallback;
}

}  // namespace

bool ParseWeapon(std::string_view id, std::string_view text, WeaponDef& out, std::string& error) {
    try {
        const json j = json::parse(text.begin(), text.end(), nullptr, true, true);
        // start from the built-in with this id, so a file only needs what it changes
        WeaponDef w;
        for (const WeaponDef& b : BuiltIns()) {
            if (b.id == id) {
                w = b;
            }
        }
        w.id = id;
        w.name = Get<std::string>(j, "name", w.name.empty() ? w.id : w.name);
        w.slot = Get<int>(j, "slot", w.slot);
        if (j.contains("type")) {
            const std::string t = j.at("type").get<std::string>();
            if (t == "hitscan") {
                w.type = FireType::kHitscan;
            } else if (t == "pellets") {
                w.type = FireType::kPellets;
            } else if (t == "beam") {
                w.type = FireType::kBeam;
            } else if (t == "projectile") {
                w.type = FireType::kProjectile;
            } else {
                throw std::runtime_error("type must be hitscan, pellets, beam or projectile");
            }
        }
        w.damage = Get<float>(j, "damage", w.damage);
        w.splashDamage = Get<float>(j, "splashDamage", w.splashDamage);
        w.splashRadius = Get<float>(j, "splashRadius", w.splashRadius);
        w.range = Get<float>(j, "range", w.range);
        w.speed = Get<float>(j, "speed", w.speed);
        w.pellets = Get<int>(j, "pellets", w.pellets);
        w.spread = Get<float>(j, "spread", w.spread);
        w.pierce = Get<int>(j, "pierce", w.pierce);
        w.refireMs = Get<int>(j, "refireMs", w.refireMs);
        w.knockback = Get<float>(j, "knockback", w.knockback);
        if (j.contains("ammo")) {
            const json& a = j.at("ammo");
            w.ammoStart = Get<int>(a, "start", w.ammoStart);
            w.ammoMax = Get<int>(a, "max", w.ammoMax);
            w.ammoPickup = Get<int>(a, "pickup", w.ammoPickup);
        }

        if (w.slot < 1 || w.slot > 15) {
            throw std::runtime_error("slot must be 1..15 (a WP_* number)");
        }
        if (w.damage < 0 || w.splashDamage < 0 || w.splashRadius < 0) {
            throw std::runtime_error("damage values must be >= 0");
        }
        if (w.refireMs < 10 || w.refireMs > 10000) {
            throw std::runtime_error("refireMs must be 10..10000");
        }
        if (w.type == FireType::kProjectile && (w.speed < 50 || w.speed > 20000)) {
            throw std::runtime_error("a projectile needs speed 50..20000");
        }
        if (w.pellets < 1 || w.pellets > 64 || w.pierce < 1 || w.pierce > 16) {
            throw std::runtime_error("pellets must be 1..64 and pierce 1..16");
        }
        if (w.range < 16 || w.range > 65536) {
            throw std::runtime_error("range must be 16..65536");
        }
        if (w.ammoStart < 0 || w.ammoMax < 1 || w.ammoPickup < 0) {
            throw std::runtime_error("bad ammo values");
        }
        out = std::move(w);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

void ResetWeapons() { Registry() = BuiltIns(); }

void AddWeapon(const WeaponDef& weapon) {
    auto& r = Registry();
    r.erase(std::remove_if(r.begin(), r.end(),
                           [&](const WeaponDef& w) { return w.id == weapon.id || w.slot == weapon.slot; }),
            r.end());
    r.push_back(weapon);
    std::sort(r.begin(), r.end(), [](const WeaponDef& a, const WeaponDef& b) { return a.slot < b.slot; });
}

const WeaponDef* WeaponBySlot(int slot) {
    for (const WeaponDef& w : Registry()) {
        if (w.slot == slot) {
            return &w;
        }
    }
    return nullptr;
}

const WeaponDef* WeaponById(std::string_view id) {
    for (const WeaponDef& w : Registry()) {
        if (w.id == id) {
            return &w;
        }
    }
    return nullptr;
}

const std::vector<WeaponDef>& Weapons() { return Registry(); }

}  // namespace skaar
