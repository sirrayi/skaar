// Weapon fire on the server: hitscan (rail, pierces), pellets (shotgun),
// beam (lightning), projectiles (rocket) with splash. Shapes and constants
// follow Q3's g_weapon.c / g_missile.c (GPL); values come from the weapon
// definitions (game/shared/weapons.h).
//
// This file is part of Skaar. GPLv2 or later.
#include <cmath>
#include <vector>

#include "mode_local.h"

namespace sg::mode {
namespace {

constexpr int kMissilePrestepMs = 50;  // Q3 MISSILE_PRESTEP: rockets start a little way out
constexpr int kMissileLifeMs = 15000;

int ModFor(const skaar::WeaponDef& w, bool splash) {
    switch (w.slot) {
        case WP_SHOTGUN:
            return MOD_SHOTGUN;
        case WP_ROCKET_LAUNCHER:
            return splash ? MOD_ROCKET_SPLASH : MOD_ROCKET;
        case WP_LIGHTNING:
            return MOD_LIGHTNING;
        case WP_RAILGUN:
            return MOD_RAILGUN;
        default:
            return MOD_UNKNOWN;
    }
}

// Q3 CalcMuzzlePoint: from the eye, a little forward
void Muzzle(const playerState_t& ps, vec3_t muzzle, vec3_t forward, vec3_t right, vec3_t up) {
    AngleVectors(ps.viewangles, forward, right, up);
    VectorCopy(ps.origin, muzzle);
    muzzle[2] += ps.viewheight;
    VectorMA(muzzle, 14, forward, muzzle);
}

void CountHit(gentity_t* player, int slot, gentity_t* hit) {
    if (player->client && hit != player && Damageable(hit)) {
        clients[player->s.number].hits[slot]++;
    }
}

// Rail: one line through up to `pierce` bodies, then the world
void FireHitscan(gentity_t* player, const skaar::WeaponDef& w, const vec3_t muzzle, const vec3_t forward) {
    vec3_t end;
    VectorMA(muzzle, w.range, forward, end);
    std::vector<gentity_t*> unlinked;
    trace_t tr;
    bool counted = false;
    for (int i = 0; i < w.pierce; i++) {
        trap::Trace(&tr, muzzle, nullptr, nullptr, end, player->s.number, MASK_SHOT);
        if (tr.entityNum >= ENTITYNUM_MAX_NORMAL) {
            break;
        }
        gentity_t* hit = &g_entities[tr.entityNum];
        if (!counted) {
            CountHit(player, w.slot, hit);
            counted = Damageable(hit);
        }
        if (Damageable(hit)) {
            Damage(hit, player, player, forward, tr.endpos, static_cast<int>(w.damage), 0, ModFor(w, false));
        }
        if (!(hit->r.contents & CONTENTS_BODY)) {
            break;
        }
        // pass through it: take it out of the world for the next trace (Q3 does the same)
        trap::UnlinkEntity(hit);
        unlinked.push_back(hit);
    }
    trap::Trace(&tr, muzzle, nullptr, nullptr, end, player->s.number, MASK_SOLID);
    for (gentity_t* e : unlinked) {
        if (e->inuse) {
            trap::LinkEntity(e);
        }
    }
    gentity_t* tent = TempEvent(muzzle, EV_RAILTRAIL, DirToByte(tr.plane.normal));
    VectorCopy(tr.endpos, tent->s.origin2);
    tent->s.clientNum = player->s.number;
    tent->s.weapon = w.slot;
}

// Shotgun: Q3's pattern. The client rebuilds the pellets from the seed for impacts.
void FirePellets(gentity_t* player, const skaar::WeaponDef& w, const vec3_t muzzle, const vec3_t aim) {
    int seed = rand() & 255;
    gentity_t* tent = TempEvent(muzzle, EV_SHOTGUN, seed);
    tent->s.otherEntityNum = player->s.number;
    tent->s.weapon = w.slot;
    tent->s.angles2[0] = w.spread;
    tent->s.angles2[1] = static_cast<float>(w.pellets);
    VectorScale(aim, 4096, tent->s.origin2);
    SnapVector(tent->s.origin2);

    // Q3 ShotgunPattern: the frame comes from the snapped direction in origin2,
    // so the client rebuilds exactly these pellets from (origin2, seed)
    vec3_t forward, right, up;
    VectorNormalize2(tent->s.origin2, forward);
    PerpendicularVector(right, forward);
    CrossProduct(forward, right, up);
    bool counted = false;
    for (int i = 0; i < w.pellets; i++) {
        const float r = Q_crandom(&seed) * w.spread * 16;
        const float u = Q_crandom(&seed) * w.spread * 16;
        vec3_t end;
        VectorMA(muzzle, 8192 * 16, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);
        trace_t tr;
        trap::Trace(&tr, muzzle, nullptr, nullptr, end, player->s.number, MASK_SHOT);
        if (tr.entityNum >= ENTITYNUM_MAX_NORMAL) {
            continue;
        }
        gentity_t* hit = &g_entities[tr.entityNum];
        if (!counted && Damageable(hit)) {
            CountHit(player, w.slot, hit);
            counted = true;
        }
        Damage(hit, player, player, forward, tr.endpos, static_cast<int>(w.damage), 0, ModFor(w, false));
    }
}

// Lightning: a short trace each refire. The client draws the beam from its own view.
void FireBeam(gentity_t* player, const skaar::WeaponDef& w, const vec3_t muzzle, const vec3_t forward) {
    vec3_t end;
    VectorMA(muzzle, w.range, forward, end);
    trace_t tr;
    trap::Trace(&tr, muzzle, nullptr, nullptr, end, player->s.number, MASK_SHOT);
    if (tr.entityNum >= ENTITYNUM_MAX_NORMAL) {
        return;
    }
    gentity_t* hit = &g_entities[tr.entityNum];
    CountHit(player, w.slot, hit);
    Damage(hit, player, player, forward, tr.endpos, static_cast<int>(w.damage), 0, ModFor(w, false));
}

void FireProjectile(gentity_t* player, const skaar::WeaponDef& w, const vec3_t muzzle, const vec3_t forward) {
    gentity_t* m = Spawn();
    m->classname = "missile";
    m->s.eType = ET_MISSILE;
    m->s.weapon = w.slot;
    m->s.otherEntityNum = player->s.number;
    m->r.ownerNum = player->s.number;
    m->r.svFlags = SVF_USE_CURRENT_ORIGIN;
    m->s.pos.trType = TR_LINEAR;
    m->s.pos.trTime = level.time - kMissilePrestepMs;
    VectorCopy(muzzle, m->s.pos.trBase);
    VectorScale(forward, w.speed, m->s.pos.trDelta);
    SnapVector(m->s.pos.trDelta);  // save net bandwidth (Q3)
    VectorCopy(muzzle, m->r.currentOrigin);

    EntState& st = StateOf(m);
    st = EntState{};
    st.kind = EntState::Kind::kMissile;
    st.owner = player->s.number;
    st.slot = w.slot;
    st.dieAt = level.time + kMissileLifeMs;
    trap::LinkEntity(m);
}

void Explode(gentity_t* m, const trace_t* tr) {
    EntState& st = StateOf(m);
    const skaar::WeaponDef* w = skaar::WeaponBySlot(st.slot);
    gentity_t* owner = st.owner < ENTITYNUM_MAX_NORMAL ? &g_entities[st.owner] : nullptr;
    vec3_t origin;
    VectorCopy(tr ? tr->endpos : m->r.currentOrigin, origin);

    gentity_t* direct = nullptr;
    if (w && tr && tr->entityNum < ENTITYNUM_MAX_NORMAL) {
        gentity_t* hit = &g_entities[tr->entityNum];
        if (Damageable(hit)) {
            direct = hit;
            if (owner) {
                CountHit(owner, w->slot, hit);
            }
            vec3_t dir;
            VectorNormalize2(m->s.pos.trDelta, dir);
            Damage(hit, m, owner, dir, origin, static_cast<int>(w->damage), 0, ModFor(*w, false));
        }
    }
    if (w && w->splashDamage > 0) {
        // nudge off the surface so the splash test starts in open space
        vec3_t at;
        VectorCopy(origin, at);
        if (tr) {
            VectorMA(at, 1, tr->plane.normal, at);
        }
        RadiusDamage(at, owner, w->splashDamage, w->splashRadius, direct, ModFor(*w, true));
    }

    vec3_t normal = {0, 0, 1};
    if (tr) {
        VectorCopy(tr->plane.normal, normal);
    }
    const int dirByte = DirToByte(normal);
    gentity_t* tent = TempEvent(origin, direct ? EV_MISSILE_HIT : EV_MISSILE_MISS, dirByte);
    tent->s.weapon = st.slot;
    tent->s.otherEntityNum = direct ? direct->s.number : ENTITYNUM_NONE;

    st = EntState{};
    FreeEntity(m);
}

}  // namespace

void FireWeapon(gentity_t* player, playerState_t& ps) {
    const skaar::WeaponDef* w = skaar::WeaponBySlot(ps.weapon);
    if (!w) {
        return;
    }
    clients[player->s.number].shots[w->slot]++;
    vec3_t muzzle, forward, right, up;
    Muzzle(ps, muzzle, forward, right, up);
    switch (w->type) {
        case skaar::FireType::kHitscan:
            FireHitscan(player, *w, muzzle, forward);
            break;
        case skaar::FireType::kPellets:
            FirePellets(player, *w, muzzle, forward);
            break;
        case skaar::FireType::kBeam:
            FireBeam(player, *w, muzzle, forward);
            break;
        case skaar::FireType::kProjectile:
            FireProjectile(player, *w, muzzle, forward);
            break;
    }
}

void RunMissile(gentity_t* m) {
    EntState& st = StateOf(m);
    if (level.time >= st.dieAt) {
        Explode(m, nullptr);
        return;
    }
    vec3_t origin;
    BG_EvaluateTrajectory(&m->s.pos, level.time, origin);
    trace_t tr;
    // the owner can't hit their own rocket on the way out
    trap::Trace(&tr, m->r.currentOrigin, nullptr, nullptr, origin, st.owner, MASK_SHOT);
    if (tr.startsolid || tr.allsolid) {
        tr.fraction = 0;
        VectorCopy(m->r.currentOrigin, tr.endpos);
    }
    VectorCopy(tr.endpos, m->r.currentOrigin);
    if (tr.fraction < 1.0f) {
        if (tr.surfaceFlags & SURF_NOIMPACT) {
            st = EntState{};
            FreeEntity(m);  // into the sky
            return;
        }
        Explode(m, &tr);
        return;
    }
    trap::LinkEntity(m);
}

}  // namespace sg::mode
