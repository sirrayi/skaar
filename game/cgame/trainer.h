// Aim-trainer hook points in the cgame frame loop. Stubs in P1; the trainer
// (targets, hitscan, run recording) fills them in from P3 on.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include "syscalls.h"

namespace cg::trainer {

void Init();      // after the map and HUD are loaded
void Shutdown();

// Once per new user command, right after prediction ran it. `ps` is the
// predicted player state after the command (view angles, origin, velocity).
// Commands are delivered once each, in order, even though prediction replays
// unacknowledged commands every frame.
void OnCommand(const usercmd_t& cmd, const playerState_t& ps);

// Each frame between R_ClearScene and R_RenderScene: add refEntities.
void AddEntities();

// Each frame after the world is rendered, before the base HUD.
void DrawHud();

// Console commands, offered before the cgame's own; true if handled.
bool ConsoleCommand(const char* cmd);

// While true the base HUD hides its crosshair, speed and map line (the trainer draws its own).
bool Active();

}  // namespace cg::trainer
