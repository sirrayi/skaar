// This file is part of Skaar. GPLv2 or later.
#include <filesystem>
#include <fstream>
#include <sstream>

#include "check.h"
#include "shared/weapons.h"

using namespace skaar;

namespace {

std::string Slurp(const std::filesystem::path& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST("shipped weapons parse and match the built-in defaults") {
    ResetWeapons();
    const auto dir = std::filesystem::path(SKAAR_SOURCE_DIR) / "data" / "base" / "weapons";
    int n = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        WeaponDef w;
        std::string error;
        const std::string id = e.path().stem().string();
        if (!ParseWeapon(id, Slurp(e.path()), w, error)) {
            check::Fail(__FILE__, __LINE__, id + ": " + error);
            continue;
        }
        const WeaponDef* builtin = WeaponById(id);
        CHECK(builtin != nullptr);
        if (builtin) {
            CHECK(w.slot == builtin->slot && w.type == builtin->type && w.refireMs == builtin->refireMs);
            CHECK_NEAR(w.damage, builtin->damage, 1e-6);
        }
        n++;
    }
    CHECK(n == 4);
}

TEST("weapon files only need what they change") {
    WeaponDef w;
    std::string error;
    CHECK(ParseWeapon("rail", R"({"damage": 80})", w, error));
    CHECK(w.slot == 7 && w.pierce == 4 && w.refireMs == 1500);
    CHECK_NEAR(w.damage, 80, 1e-6);
    // a new weapon needs a slot
    CHECK(!ParseWeapon("pistol", R"({"type": "hitscan", "damage": 40})", w, error));
    CHECK(ParseWeapon("pistol", R"({"slot": 2, "type": "hitscan", "damage": 40, "refireMs": 250})", w, error));
    CHECK(!ParseWeapon("bad", R"({"slot": 9, "type": "projectile", "speed": 0})", w, error));
    CHECK(!ParseWeapon("bad", R"({"slot": 9, "type": "laser"})", w, error));
}

TEST("weapon registry by slot and id") {
    ResetWeapons();
    CHECK(WeaponBySlot(7) && WeaponBySlot(7)->id == "rail");
    CHECK(WeaponBySlot(5) && WeaponBySlot(5)->type == FireType::kProjectile);
    WeaponDef pistol;
    std::string error;
    CHECK(ParseWeapon("pistol", R"({"slot": 2, "damage": 40})", pistol, error));
    AddWeapon(pistol);
    CHECK(Weapons().size() == 5 && Weapons().front().id == "pistol");  // sorted by slot
    WeaponDef rail2 = *WeaponById("rail");
    rail2.damage = 70;
    AddWeapon(rail2);
    CHECK(Weapons().size() == 5);
    CHECK_NEAR(WeaponById("rail")->damage, 70, 1e-6);
    ResetWeapons();
}
