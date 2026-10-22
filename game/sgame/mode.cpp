// Arena mode (P6): the hooks the server loop calls, damage, events, loadouts.
// Weapons fire in weapons_fire.cpp, items in items.cpp, dummies in dummies.cpp.
// Damage and knockback follow Q3's g_combat.c (GPL).
//
// This file is part of Skaar. GPLv2 or later.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "mode_local.h"

namespace sg::mode {

std::array<EntState, MAX_GENTITIES> ents;
std::array<ClientState, MAX_CLIENTS> clients;
Cvars mcv;

namespace {

constexpr int kDamageRadius = 0x1;       // Q3 DAMAGE_* flags (g_local.h)
constexpr int kDamageNoKnockback = 0x4;
constexpr float kArmorProtection = 0.66f;

void LoadWeapons() {
    skaar::ResetWeapons();
    for (const std::string& file : trap::FileList("weapons", ".json")) {
        const std::string id = file.substr(0, file.size() - 5);
        std::string text, error;
        skaar::WeaponDef w;
        if (!trap::FileRead(skaar::Va("weapons/%s", file.c_str()), text)) {
            continue;
        }
        if (skaar::ParseWeapon(id, text, w, error)) {
            skaar::AddWeapon(w);
        } else {
            Com_Printf(S_COLOR_YELLOW "WARNING: weapons/%s: %s\n", file.c_str(), error.c_str());
        }
    }
}

// Free temp events, clear stale entity events, run missiles, items and dummies
void RunEntities() {
    for (int i = MAX_CLIENTS; i < level.numEntities; i++) {
        gentity_t* ent = &g_entities[i];
        EntState& st = ents[i];
        if (!ent->inuse || st.kind == EntState::Kind::kNone) {
            continue;
        }
        if (st.eventTime && level.time - st.eventTime > EVENT_VALID_MSEC) {
            ent->s.event = 0;
            ent->s.eventParm = 0;
            st.eventTime = 0;
        }
        switch (st.kind) {
            case EntState::Kind::kTemp:
                if (level.time >= st.freeAt) {
                    st = EntState{};
                    FreeEntity(ent);
                }
                break;
            case EntState::Kind::kMissile:
                RunMissile(ent);
                break;
            case EntState::Kind::kItem:
                RunItem(ent);
                break;
            case EntState::Kind::kDummy:
                RunDummy(ent);
                break;
            default:
                break;
        }
    }
}

// Health and armor above 100 tick down once a second (Q3's mega/armor decay)
void Decay() {
    for (int c = 0; c < MAX_CLIENTS; c++) {
        gclient_t& cl = g_clients[c];
        if (!cl.spawned || cl.dead) {
            continue;
        }
        ClientState& cs = clients[c];
        if (level.time - cs.lastDecay < 1000) {
            continue;
        }
        cs.lastDecay = level.time;
        if (cl.ps.stats[STAT_HEALTH] > skaar::arena::kMaxHealth) {
            cl.ps.stats[STAT_HEALTH]--;
        }
        if (cl.ps.stats[STAT_ARMOR] > skaar::arena::kMaxHealth) {
            cl.ps.stats[STAT_ARMOR]--;
        }
        // Q3 clears a client's external event once it's old
        if (cl.ps.externalEvent && level.time - cl.ps.externalEventTime > EVENT_VALID_MSEC) {
            cl.ps.externalEvent = 0;
            cl.ps.externalEventParm = 0;
        }
    }
}

}  // namespace

// ---- helpers ------------------------------------------------------------------------

bool KeyVector(const EntityKeys& keys, const char* key, vec3_t out) {
    auto it = keys.find(key);
    if (it == keys.end()) {
        return false;
    }
    std::istringstream in(it->second);
    return static_cast<bool>(in >> out[0] >> out[1] >> out[2]);
}

gentity_t* TempEvent(const vec3_t origin, int event, int eventParm) {
    gentity_t* e = Spawn();
    e->classname = "tempEntity";
    e->s.eType = ET_EVENTS + event;
    e->s.eventParm = eventParm;
    e->s.pos.trType = TR_STATIONARY;
    VectorCopy(origin, e->s.pos.trBase);
    VectorCopy(origin, e->r.currentOrigin);
    EntState& st = StateOf(e);
    st = EntState{};
    st.kind = EntState::Kind::kTemp;
    st.freeAt = level.time + EVENT_VALID_MSEC;
    trap::LinkEntity(e);
    return e;
}

void AddEvent(gentity_t* ent, int event, int eventParm) {
    // the two event bits make a repeat of the same event look new (Q3 G_AddEvent)
    if (ent->client) {
        playerState_t& ps = ent->client->ps;
        const int bits = ((ps.externalEvent & EV_EVENT_BITS) + EV_EVENT_BIT1) & EV_EVENT_BITS;
        ps.externalEvent = event | bits;
        ps.externalEventParm = eventParm;
        ps.externalEventTime = level.time;
    } else {
        const int bits = ((ent->s.event & EV_EVENT_BITS) + EV_EVENT_BIT1) & EV_EVENT_BITS;
        ent->s.event = event | bits;
        ent->s.eventParm = eventParm;
        StateOf(ent).eventTime = level.time;
    }
}

bool Damageable(const gentity_t* ent) {
    if (!ent || !ent->inuse) {
        return false;
    }
    if (ent->client) {
        return ent->client->spawned && !ent->client->dead;
    }
    const EntState& st = ents[ent->s.number];
    return st.kind == EntState::Kind::kDummy && st.deadUntil == 0;
}

void RadiusDamage(const vec3_t origin, gentity_t* attacker, float damage, float radius, gentity_t* ignore, int mod) {
    if (radius < 1) {
        return;
    }
    vec3_t mins, maxs;
    for (int i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }
    int list[MAX_GENTITIES];
    const int n = trap::EntitiesInBox(mins, maxs, list, MAX_GENTITIES);
    for (int k = 0; k < n; k++) {
        gentity_t* ent = &g_entities[list[k]];
        if (ent == ignore || !Damageable(ent)) {
            continue;
        }
        // distance from the edge of the target's box
        vec3_t v;
        for (int i = 0; i < 3; i++) {
            v[i] = origin[i] < ent->r.absmin[i]   ? ent->r.absmin[i] - origin[i]
                   : origin[i] > ent->r.absmax[i] ? origin[i] - ent->r.absmax[i]
                                                  : 0;
        }
        const float dist = VectorLength(v);
        if (dist >= radius) {
            continue;
        }
        // only through open space (Q3 CanDamage, centre test)
        vec3_t centre;
        VectorAdd(ent->r.absmin, ent->r.absmax, centre);
        VectorScale(centre, 0.5f, centre);
        trace_t tr;
        trap::Trace(&tr, origin, nullptr, nullptr, centre, ENTITYNUM_NONE, MASK_SOLID);
        if (tr.fraction < 1.0f) {
            continue;
        }
        vec3_t dir;
        VectorSubtract(ent->r.currentOrigin, origin, dir);
        dir[2] += 24;  // push the centre of mass higher, so players go up more
        Damage(ent, nullptr, attacker, dir, origin, static_cast<int>(damage * (1.0f - dist / radius)), kDamageRadius,
               mod);
    }
}

void GiveWeapon(playerState_t& ps, int slot, bool select) {
    const skaar::WeaponDef* w = skaar::WeaponBySlot(slot);
    if (!w) {
        return;
    }
    ps.stats[STAT_WEAPONS] |= 1 << slot;
    if (mcv.infiniteAmmo.integer) {
        ps.ammo[slot] = -1;  // pmove never counts down -1
    } else if (ps.ammo[slot] >= 0) {
        ps.ammo[slot] = std::min(std::max(ps.ammo[slot], w->ammoStart), w->ammoMax);
    }
    if (select || ps.weapon == WP_NONE) {
        ps.weapon = slot;
        ps.weaponstate = WEAPON_READY;
    }
}

// ---- hooks ----------------------------------------------------------------------------

void Init() {
    ents.fill(EntState{});
    clients.fill(ClientState{});
    trap::CvarRegister(&mcv.knockback, "g_knockback", "1000", 0);
    trap::CvarRegister(&mcv.infiniteAmmo, "g_infiniteAmmo", "0", CVAR_ARCHIVE);
    trap::CvarRegister(&mcv.loadout, "g_loadout", "1", CVAR_ARCHIVE);
    LoadWeapons();
}

void Shutdown() {}

void RunFrame(int /*levelTime*/) {
    for (vmCvar_t* c : {&mcv.knockback, &mcv.infiniteAmmo, &mcv.loadout}) {
        trap::CvarUpdate(c);
    }
    RunEntities();
    Decay();
}

void ClientSpawn(int clientNum, playerState_t& ps) {
    ps.stats[STAT_HEALTH] = skaar::arena::kMaxHealth;
    ps.stats[STAT_MAX_HEALTH] = skaar::arena::kMaxHealth;
    ps.stats[STAT_ARMOR] = 0;
    ps.stats[STAT_WEAPONS] = 0;
    for (int& a : ps.ammo) {
        a = 0;
    }
    // practice loadout only on maps with arena items: the aim trainer's maps stay weaponless
    bool arenaMap = false;
    for (int i = MAX_CLIENTS; i < level.numEntities && !arenaMap; i++) {
        arenaMap = ents[i].kind == EntState::Kind::kItem || ents[i].kind == EntState::Kind::kDummy;
    }
    if (arenaMap && mcv.loadout.integer) {
        for (const skaar::WeaponDef& w : skaar::Weapons()) {
            GiveWeapon(ps, w.slot, false);
        }
        if (const skaar::WeaponDef* rail = skaar::WeaponById("rail")) {
            ps.weapon = rail->slot;
        }
    }
    clients[clientNum].lastDecay = level.time;
}

void ClientThink(int clientNum, const usercmd_t& /*cmd*/, playerState_t& ps, int oldEventSequence) {
    gentity_t* player = &g_entities[clientNum];
    if (oldEventSequence < ps.eventSequence - MAX_PS_EVENTS) {
        oldEventSequence = ps.eventSequence - MAX_PS_EVENTS;
    }
    for (int i = oldEventSequence; i < ps.eventSequence; i++) {
        const int event = ps.events[i & (MAX_PS_EVENTS - 1)] & ~EV_EVENT_BITS;
        if (event == EV_FIRE_WEAPON) {
            FireWeapon(player, ps);
        }
    }
}

bool ClientCommand(int clientNum, const char* cmd) {
    playerState_t& ps = g_clients[clientNum].ps;
    if (!Q_stricmp(cmd, "give")) {
        // give all | <weapon id>  (practice convenience)
        const std::string what = trap::Argv(1);
        for (const skaar::WeaponDef& w : skaar::Weapons()) {
            if (what == "all" || what == w.id) {
                GiveWeapon(ps, w.slot, what != "all");
            }
        }
        if (what == "all") {
            ps.stats[STAT_HEALTH] = 200;
            ps.stats[STAT_ARMOR] = 200;
        }
        return true;
    }
    if (!Q_stricmp(cmd, "arenastats")) {
        const ClientState& cs = clients[clientNum];
        std::string text = skaar::Va("damage dealt %d, taken %d, kills %d, deaths %d\n", cs.damageDealt,
                                     cs.damageTaken, cs.kills, cs.deaths);
        for (const skaar::WeaponDef& w : skaar::Weapons()) {
            if (cs.shots[w.slot]) {
                text += skaar::Va("%-10s %d/%d  %.0f pct\n", w.name.c_str(), cs.hits[w.slot], cs.shots[w.slot],
                                  100.0f * cs.hits[w.slot] / cs.shots[w.slot]);
            }
        }
        trap::SendServerCommand(clientNum, skaar::Va("print \"%s\"", text.c_str()));
        return true;
    }
    return false;
}

bool SpawnEntity(const char* classname, const EntityKeys& keys) {
    const int item = skaar::arena::FindItem(classname);
    if (item >= 0) {
        return SpawnItem(item, keys);
    }
    if (!Q_stricmp(classname, "info_dummy")) {
        return SpawnDummy(keys);
    }
    return false;
}

void Touch(gentity_t* trigger, gentity_t* player) {
    if (StateOf(trigger).kind == EntState::Kind::kItem) {
        TouchItem(trigger, player);
    }
}

void Damage(gentity_t* target, gentity_t* inflictor, gentity_t* attacker, const vec3_t dir, const vec3_t point,
            int damage, int dflags, int mod) {
    (void)inflictor;
    (void)point;
    (void)mod;
    if (!Damageable(target) || damage <= 0) {
        return;
    }
    if (!target->client) {
        DamageDummy(target, attacker, dir, damage, dflags);
        return;
    }

    gclient_t& cl = *target->client;
    playerState_t& ps = cl.ps;
    const int targetNum = target->s.number;

    // knockback first, so rocket jumps get the full push (Q3)
    vec3_t d = {0, 0, 1};
    if (dir) {
        VectorCopy(dir, d);
        VectorNormalize(d);
    }
    int knockback = std::min(damage, 200);
    if (dflags & kDamageNoKnockback) {
        knockback = 0;
    }
    if (knockback && ps.pm_type == PM_NORMAL) {
        vec3_t kvel;
        VectorScale(d, mcv.knockback.value * static_cast<float>(knockback) / 200.0f, kvel);
        VectorAdd(ps.velocity, kvel, ps.velocity);
        if (!ps.pm_time) {
            ps.pm_time = std::clamp(knockback * 2, 50, 200);
            ps.pm_flags |= PMF_TIME_KNOCKBACK;
        }
    }

    const bool selfHit = attacker == target;
    if (attacker && attacker->client && !selfHit) {
        attacker->client->ps.persistant[PERS_HITS]++;  // the cgame beeps on change
        clients[attacker->s.number].damageDealt += damage;
    }
    if (selfHit) {
        damage = static_cast<int>(damage * 0.5f);  // always half on yourself
    }
    damage = std::max(damage, 1);

    int save = static_cast<int>(std::ceil(damage * kArmorProtection));
    save = std::min(save, ps.stats[STAT_ARMOR]);
    ps.stats[STAT_ARMOR] -= save;
    const int take = damage - save;
    ps.stats[STAT_HEALTH] -= take;
    clients[targetNum].damageTaken += take;
    AddEvent(target, EV_PAIN, std::max(0, ps.stats[STAT_HEALTH]));

    if (ps.stats[STAT_HEALTH] <= 0) {
        clients[targetNum].deaths++;
        if (attacker && attacker->client && !selfHit) {
            clients[attacker->s.number].kills++;
        }
        Kill(targetNum);
    }
}

}  // namespace sg::mode
