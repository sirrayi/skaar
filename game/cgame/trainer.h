// Aim trainer in the cgame: runs scenarios (game/shared/trainer_sim.h) on the
// player's own predicted commands, draws targets and the trainer HUD, saves
// run records for the insights screens, and plays recorded runs back.
//
// Console commands:
//   sk_start <scenario>   load a scenario (if it's on this map) and start a run
//   sk_restart            new run of the current scenario (R); respawn otherwise
//   sk_stop               leave the scenario, back to free roam
//   sk_replay <file>      play back stats/runs/<file>.json (same map)
//   sk_insights           open the ui insights on the last run
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

// Camera override for replays: no-op unless a replay is playing.
void OverrideView(vec3_t origin, vec3_t angles);

}  // namespace cg::trainer
