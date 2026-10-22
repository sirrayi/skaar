/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// Internals of the movement port (bg_local.h). Q3's file-level globals pm/pml
// become one Move object per pmove step, so nothing leaks between calls.
//
// Ported to C++ for Skaar. GPLv2 or later.
#pragma once

#include "move_pmove.h"
#include "move_ruleset.h"

namespace skaar::move {

constexpr float kMinWalkNormal = 0.7f;  // can't walk on very steep slopes
constexpr float kStepSize = 18;
constexpr int kTimerLand = 130;
constexpr int kTimerGesture = 34 * 66 + 50;
constexpr float kOverclip = 1.001f;

// All of these are zeroed before each step, so client and server can't differ.
struct Locals {
    vec3_t forward, right, up;
    float frametime;
    int msec;

    bool walking;
    bool groundPlane;
    trace_t groundTrace;

    float impactSpeed;

    vec3_t previousOrigin;
    vec3_t previousVelocity;
    int previousWaterLevel;
};

class Move {
public:
    Move(pmove_t* pm, const Ruleset& rs) : pm_(pm), ps_(pm->ps), rs_(rs) {}
    void Single();

private:
    // move_pmove.cpp
    void AddEvent(int event);
    void AddTouchEnt(int entityNum);
    void StartTorsoAnim(int anim);
    void StartLegsAnim(int anim);
    void ContinueLegsAnim(int anim);
    void ContinueTorsoAnim(int anim);
    void ForceLegsAnim(int anim);
    void ForceJumpAnim();

    void Friction();
    void Accelerate(const vec3_t wishdir, float wishspeed, float accel);
    void AirControl(const vec3_t wishdir, float wishspeed);
    float CmdScale(const usercmd_t& cmd) const;
    void SetMovementDir();
    bool CheckJump();
    bool CheckWaterJump();
    void WaterJumpMove();
    void WaterMove();
    void FlyMove();
    void AirMove();
    void GrappleMove();
    void WalkMove();
    void DeadMove();
    void NoclipMove();
    int FootstepForSurface() const;
    void CrashLand();
    bool CorrectAllSolid(trace_t& trace);
    void GroundTraceMissed();
    void GroundTrace();
    void SetWaterLevel();
    void CheckDuck();
    void Footsteps();
    void WaterEvents();
    void BeginWeaponChange(int weapon);
    void FinishWeaponChange();
    void TorsoAnimation();
    void Weapon();
    void Animate();
    void DropTimers();

    // move_slide.cpp
    bool SlideMove(bool gravity);
    void StepSlideMove(bool gravity);

    pmove_t* pm_;
    playerState_t* ps_;
    const Ruleset& rs_;
    Locals pml_{};
};

void ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out, float overbounce);

}  // namespace skaar::move
