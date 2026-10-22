// Game-mode hooks in the client loop (P6 arena: weapons, missiles, items,
// dummies). Stubs here; the implementation lives in weapons*.cpp (owned by
// the P6 plan, docs/plan-P6.md).
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include "local.h"

namespace cg::weapons {

void Init();      // after the map and HUD are loaded
void Shutdown();

// Every entity in the current snapshot except the local player, each frame
// between R_ClearScene and R_RenderScene. lerpOrigin/lerpAngles are already
// interpolated (or extrapolated along the trajectory) for this frame.
void AddPacketEntity(const CEntity& cent);

// An entity event (es.event, or a temp ET_EVENTS entity), once each.
void EntityEvent(const entityState_t& es, int event);

// A local player event (predicted ps.events, or a server external event), once each.
void PlayerEvent(int event, int eventParm, const playerState_t& ps);

// Each frame after the world, next to trainer::DrawHud.
void DrawHud();

}  // namespace cg::weapons
