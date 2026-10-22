// Player movement, shared by the server game and client prediction.
// A C++ port of Q3's bg_pmove.c / bg_slidemove.c with the tunables moved into
// rulesets (move_ruleset.h). Same pmove_t / playerState_t as the C version.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include "q3.h"

namespace skaar::move {

// Runs the command in pm->cmd from ps->commandTime up to cmd.serverTime, in
// steps of pm->pmove_msec (pmove_fixed) or at most 66 ms. The ruleset is
// ps->stats[kStatRuleset].
void Pmove(pmove_t* pm);

// The view-angle part of a move only (clamps pitch, applies delta_angles).
void UpdateViewAngles(playerState_t* ps, const usercmd_t* cmd);

}  // namespace skaar::move
