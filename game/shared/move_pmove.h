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

// Weapon refire time in ms, overriding Q3's table for that weapon (<= 0: Q3's
// value again). Set the same table in sgame and cgame so prediction agrees.
void SetWeaponRefire(int weapon, int ms);
void ClearWeaponRefire();  // back to Q3's table for every weapon

// Semi-automatic: fires once per press, attack must be released before the next
// shot. Same table in sgame and cgame. ClearWeaponRefire doesn't touch it.
void SetWeaponSemiAuto(int weapon, bool semiAuto);
void ClearWeaponSemiAuto();

// pm_flags bit (free in Q3, inside the 16 networked bits): attack held since
// the last semi-automatic shot
constexpr int kPmfAttackHeld = 128;

}  // namespace skaar::move
