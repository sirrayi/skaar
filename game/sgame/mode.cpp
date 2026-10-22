// Game-mode hooks: empty until P6 (see mode.h).
//
// This file is part of Skaar. GPLv2 or later.
#include "mode.h"

namespace sg::mode {

void Init() {}
void Shutdown() {}
void RunFrame(int /*levelTime*/) {}
void ClientSpawn(int /*clientNum*/, playerState_t& /*ps*/) {}
void ClientThink(int /*clientNum*/, const usercmd_t& /*cmd*/, playerState_t& /*ps*/, int /*oldEventSequence*/) {}
bool ClientCommand(int /*clientNum*/, const char* /*cmd*/) { return false; }
bool SpawnEntity(const char* /*classname*/, const EntityKeys& /*keys*/) { return false; }
void Touch(gentity_t* /*trigger*/, gentity_t* /*player*/) {}
void Damage(gentity_t* /*target*/, gentity_t* /*inflictor*/, gentity_t* /*attacker*/, const vec3_t /*dir*/,
            const vec3_t /*point*/, int /*damage*/, int /*dflags*/, int /*mod*/) {}

}  // namespace sg::mode
