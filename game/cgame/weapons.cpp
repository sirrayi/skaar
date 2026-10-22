// Game-mode hooks: empty until P6 (see weapons.h).
//
// This file is part of Skaar. GPLv2 or later.
#include "weapons.h"

namespace cg::weapons {

void Init() {}
void Shutdown() {}
void AddPacketEntity(const CEntity& /*cent*/) {}
void EntityEvent(const entityState_t& /*es*/, int /*event*/) {}
void PlayerEvent(int /*event*/, int /*eventParm*/, const playerState_t& /*ps*/) {}
void DrawHud() {}
int SelectedWeapon() { return 0; }
bool ConsoleCommand(const char* /*cmd*/) { return false; }

}  // namespace cg::weapons
