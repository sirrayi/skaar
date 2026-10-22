// Aim-trainer hooks: empty in P1 (see trainer.h).
//
// This file is part of Skaar. GPLv2 or later.
#include "trainer.h"

namespace cg::trainer {

void Init() {}
void Shutdown() {}
void OnCommand(const usercmd_t& /*cmd*/, const playerState_t& /*ps*/) {}
void AddEntities() {}
void DrawHud() {}
bool ConsoleCommand(const char* /*cmd*/) { return false; }
bool Active() { return false; }

}  // namespace cg::trainer
