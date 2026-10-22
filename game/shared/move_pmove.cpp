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
// Player movement: a port of bg_pmove.c. Takes a playerstate and a usercmd and
// returns a modified playerstate. Changes from Q3, all ruleset-driven
// (docs/cpm-physics.md): AirMove's accel overrides, AirControl, and CheckJump's
// ramp and double jumps. With the vq3 ruleset it moves exactly like Q3.
//
// Ported to C++ for Skaar. GPLv2 or later.
#include "move_local.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace skaar::move {
namespace {

int c_pmove = 0;  // lets a debugger break on a given step

// Rounds each component to the nearest integer to save network bandwidth.
// Done here rather than through the engine's snapvector syscall so the server
// and client prediction share one implementation on every platform.
void SnapToInt(vec3_t v) {
    v[0] = std::round(v[0]);
    v[1] = std::round(v[1]);
    v[2] = std::round(v[2]);
}

}  // namespace

void ClipVelocity(const vec3_t in, const vec3_t normal, vec3_t out, float overbounce) {
    float backoff = DotProduct(in, normal);
    if (backoff < 0) {
        backoff *= overbounce;
    } else {
        backoff /= overbounce;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = in[i] - normal[i] * backoff;
    }
}

void Move::AddEvent(int event) {
    BG_AddPredictableEventToPlayerstate(event, 0, ps_);
}

void Move::AddTouchEnt(int entityNum) {
    if (entityNum == ENTITYNUM_WORLD || pm_->numtouch == MAXTOUCH) {
        return;
    }
    for (int i = 0; i < pm_->numtouch; i++) {
        if (pm_->touchents[i] == entityNum) {
            return;
        }
    }
    pm_->touchents[pm_->numtouch++] = entityNum;
}

void Move::StartTorsoAnim(int anim) {
    if (ps_->pm_type >= PM_DEAD) {
        return;
    }
    ps_->torsoAnim = ((ps_->torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
}

void Move::StartLegsAnim(int anim) {
    if (ps_->pm_type >= PM_DEAD) {
        return;
    }
    if (ps_->legsTimer > 0) {
        return;  // a high priority animation is running
    }
    ps_->legsAnim = ((ps_->legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
}

void Move::ContinueLegsAnim(int anim) {
    if ((ps_->legsAnim & ~ANIM_TOGGLEBIT) == anim || ps_->legsTimer > 0) {
        return;
    }
    StartLegsAnim(anim);
}

void Move::ContinueTorsoAnim(int anim) {
    if ((ps_->torsoAnim & ~ANIM_TOGGLEBIT) == anim || ps_->torsoTimer > 0) {
        return;
    }
    StartTorsoAnim(anim);
}

void Move::ForceLegsAnim(int anim) {
    ps_->legsTimer = 0;
    StartLegsAnim(anim);
}

void Move::ForceJumpAnim() {
    if (pm_->cmd.forwardmove >= 0) {
        ForceLegsAnim(LEGS_JUMP);
        ps_->pm_flags &= ~PMF_BACKWARDS_JUMP;
    } else {
        ForceLegsAnim(LEGS_JUMPB);
        ps_->pm_flags |= PMF_BACKWARDS_JUMP;
    }
}

// Ground friction and water friction.
void Move::Friction() {
    float* vel = ps_->velocity;
    vec3_t vec;
    VectorCopy(vel, vec);
    if (pml_.walking) {
        vec[2] = 0;  // ignore slope movement
    }

    const float speed = VectorLength(vec);
    if (speed < 1) {
        vel[0] = 0;
        vel[1] = 0;  // allow sinking underwater
        return;
    }

    float drop = 0;

    // ground friction
    if (pm_->waterlevel <= 1 && pml_.walking && !(pml_.groundTrace.surfaceFlags & SURF_SLICK) &&
        !(ps_->pm_flags & PMF_TIME_KNOCKBACK)) {  // no friction while knocked back
        const float control = speed < rs_.stopSpeed ? rs_.stopSpeed : speed;
        drop += control * rs_.friction * pml_.frametime;
    }

    // water friction, even if just wading
    if (pm_->waterlevel) {
        drop += speed * rs_.waterFriction * static_cast<float>(pm_->waterlevel) * pml_.frametime;
    }

    if (ps_->powerups[PW_FLIGHT]) {
        drop += speed * rs_.flightFriction * pml_.frametime;
    }

    if (ps_->pm_type == PM_SPECTATOR) {
        drop += speed * rs_.spectatorFriction * pml_.frametime;
    }

    float newspeed = speed - drop;
    if (newspeed < 0) {
        newspeed = 0;
    }
    newspeed /= speed;
    VectorScale(vel, newspeed, vel);
}

// Q2-style acceleration. Only the velocity component along wishdir is capped,
// which is what makes strafe jumping work.
void Move::Accelerate(const vec3_t wishdir, float wishspeed, float accel) {
    const float currentspeed = DotProduct(ps_->velocity, wishdir);
    const float addspeed = wishspeed - currentspeed;
    if (addspeed <= 0) {
        return;
    }
    const float accelspeed = std::min(accel * pml_.frametime * wishspeed, addspeed);
    VectorMA(ps_->velocity, accelspeed, wishdir, ps_->velocity);
}

// Turns the horizontal velocity toward wishdir without changing its length,
// with forward (or back) held alone. Stronger the better the two line up, and
// nothing while wishing against the velocity. Written from the behaviour
// described in docs/cpm-physics.md.
void Move::AirControl(const vec3_t wishdir, float wishspeed) {
    const usercmd_t& cmd = pm_->cmd;
    if (cmd.forwardmove == 0 || cmd.rightmove != 0 || wishspeed <= 0) {
        return;
    }
    if (cmd.forwardmove < 0 && !rs_.airControlBackward) {
        return;
    }

    vec3_t dir = {ps_->velocity[0], ps_->velocity[1], 0};
    const float speed = VectorNormalize(dir);
    if (speed <= 0) {
        return;
    }
    const float alignment = DotProduct(dir, wishdir);
    if (alignment <= 0) {
        return;
    }

    const float turn = 32 * rs_.airControl * std::pow(alignment, rs_.airControlPower) * pml_.frametime;
    vec3_t steered;
    VectorScale(dir, speed, steered);
    VectorMA(steered, turn, wishdir, steered);
    VectorNormalize(steered);

    ps_->velocity[0] = steered[0] * speed;
    ps_->velocity[1] = steered[1] * speed;
}

// Scale factor for the cmd's -127..127 axial moves, so diagonals aren't sqrt(2)
// faster.
float Move::CmdScale(const usercmd_t& cmd) const {
    int max = std::abs(cmd.forwardmove);
    max = std::max(max, std::abs(static_cast<int>(cmd.rightmove)));
    max = std::max(max, std::abs(static_cast<int>(cmd.upmove)));
    if (!max) {
        return 0;
    }
    // the double math matches the C original bit for bit (sqrt of an int, 127.0)
    const float total = static_cast<float>(
        std::sqrt(static_cast<double>(cmd.forwardmove * cmd.forwardmove + cmd.rightmove * cmd.rightmove +
                                      cmd.upmove * cmd.upmove)));
    return static_cast<float>(static_cast<float>(ps_->speed) * static_cast<float>(max) / (127.0 * total));
}

// Rotation of the legs relative to the facing direction.
void Move::SetMovementDir() {
    const int f = pm_->cmd.forwardmove;
    const int r = pm_->cmd.rightmove;
    if (f || r) {
        if (r == 0 && f > 0) {
            ps_->movementDir = 0;
        } else if (r < 0 && f > 0) {
            ps_->movementDir = 1;
        } else if (r < 0 && f == 0) {
            ps_->movementDir = 2;
        } else if (r < 0 && f < 0) {
            ps_->movementDir = 3;
        } else if (r == 0 && f < 0) {
            ps_->movementDir = 4;
        } else if (r > 0 && f < 0) {
            ps_->movementDir = 5;
        } else if (r > 0 && f == 0) {
            ps_->movementDir = 6;
        } else if (r > 0 && f > 0) {
            ps_->movementDir = 7;
        }
    } else {
        // not going directly sideways: turn the animation to the diagonal so
        // they don't stop too crooked
        if (ps_->movementDir == 2) {
            ps_->movementDir = 1;
        } else if (ps_->movementDir == 6) {
            ps_->movementDir = 7;
        }
    }
}

bool Move::CheckJump() {
    if (ps_->pm_flags & PMF_RESPAWNED) {
        return false;  // no jump until all buttons are up
    }
    if (pm_->cmd.upmove < 10) {
        return false;  // not holding jump
    }
    if ((ps_->pm_flags & PMF_JUMP_HELD) && !rs_.autoHop) {
        // must release jump first; clear upmove so cmdscale doesn't lower running speed
        pm_->cmd.upmove = 0;
        return false;
    }

    pml_.groundPlane = false;  // jumping away
    pml_.walking = false;
    ps_->pm_flags |= PMF_JUMP_HELD;
    ps_->groundEntityNum = ENTITYNUM_NONE;

    float jump = rs_.jumpVelocity;
    if (rs_.rampJump) {
        // walking has clipped the velocity to the slope, so running up a ramp
        // carries upward speed into the jump; downhill it's dropped
        jump += std::max(ps_->velocity[2], 0.0f);
    }
    if (rs_.doubleJumpWindowMs > 0) {
        if (ps_->stats[kStatJumpTime] > 0) {
            jump += rs_.doubleJumpBoost;
        }
        ps_->stats[kStatJumpTime] = rs_.doubleJumpWindowMs;
    }
    ps_->velocity[2] = jump;
    AddEvent(EV_JUMP);
    ForceJumpAnim();
    return true;
}

bool Move::CheckWaterJump() {
    if (ps_->pm_time || pm_->waterlevel != 2) {
        return false;
    }

    vec3_t flatforward = {pml_.forward[0], pml_.forward[1], 0};
    VectorNormalize(flatforward);

    vec3_t spot;
    VectorMA(ps_->origin, 30, flatforward, spot);
    spot[2] += 4;
    int cont = pm_->pointcontents(spot, ps_->clientNum);
    if (!(cont & CONTENTS_SOLID)) {
        return false;
    }

    spot[2] += 16;
    cont = pm_->pointcontents(spot, ps_->clientNum);
    if (cont & (CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY)) {
        return false;
    }

    // jump out of the water
    VectorScale(pml_.forward, 200, ps_->velocity);
    ps_->velocity[2] = 350;
    ps_->pm_flags |= PMF_TIME_WATERJUMP;
    ps_->pm_time = 2000;
    return true;
}

// Flying out of the water: no control, but falls.
void Move::WaterJumpMove() {
    StepSlideMove(true);
    ps_->velocity[2] -= static_cast<float>(ps_->gravity) * pml_.frametime;
    if (ps_->velocity[2] < 0) {
        // cancel as soon as we're falling down again
        ps_->pm_flags &= ~PMF_ALL_TIMES;
        ps_->pm_time = 0;
    }
}

void Move::WaterMove() {
    if (CheckWaterJump()) {
        WaterJumpMove();
        return;
    }
    Friction();

    const float scale = CmdScale(pm_->cmd);
    vec3_t wishvel;
    if (!scale) {
        VectorSet(wishvel, 0, 0, -60);  // sink towards the bottom
    } else {
        for (int i = 0; i < 3; i++) {
            wishvel[i] = scale * pml_.forward[i] * pm_->cmd.forwardmove + scale * pml_.right[i] * pm_->cmd.rightmove;
        }
        wishvel[2] += scale * pm_->cmd.upmove;
    }

    vec3_t wishdir;
    VectorCopy(wishvel, wishdir);
    float wishspeed = VectorNormalize(wishdir);
    wishspeed = std::min(wishspeed, static_cast<float>(ps_->speed) * rs_.swimScale);

    Accelerate(wishdir, wishspeed, rs_.waterAccelerate);

    // make sure we can go up slopes easily under water
    if (pml_.groundPlane && DotProduct(ps_->velocity, pml_.groundTrace.plane.normal) < 0) {
        const float vel = VectorLength(ps_->velocity);
        ClipVelocity(ps_->velocity, pml_.groundTrace.plane.normal, ps_->velocity, kOverclip);
        VectorNormalize(ps_->velocity);
        VectorScale(ps_->velocity, vel, ps_->velocity);
    }

    SlideMove(false);
}

// Flight powerup and spectators.
void Move::FlyMove() {
    Friction();

    const float scale = CmdScale(pm_->cmd);
    vec3_t wishvel;
    if (!scale) {
        VectorClear(wishvel);
    } else {
        for (int i = 0; i < 3; i++) {
            wishvel[i] = scale * pml_.forward[i] * pm_->cmd.forwardmove + scale * pml_.right[i] * pm_->cmd.rightmove;
        }
        wishvel[2] += scale * pm_->cmd.upmove;
    }

    vec3_t wishdir;
    VectorCopy(wishvel, wishdir);
    const float wishspeed = VectorNormalize(wishdir);

    Accelerate(wishdir, wishspeed, rs_.flyAccelerate);
    StepSlideMove(false);
}

void Move::AirMove() {
    Friction();

    const float fmove = pm_->cmd.forwardmove;
    const float smove = pm_->cmd.rightmove;
    const float scale = CmdScale(pm_->cmd);

    // so clients can rotate the legs for strafing
    SetMovementDir();

    // project moves down to the flat plane
    pml_.forward[2] = 0;
    pml_.right[2] = 0;
    VectorNormalize(pml_.forward);
    VectorNormalize(pml_.right);

    vec3_t wishvel;
    for (int i = 0; i < 2; i++) {
        wishvel[i] = pml_.forward[i] * fmove + pml_.right[i] * smove;
    }
    wishvel[2] = 0;

    vec3_t wishdir;
    VectorCopy(wishvel, wishdir);
    float wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    // vq3: one small air accel. cpm overrides it when wishing against the
    // velocity, and with only strafe keys held (low cap, high accel).
    float accel = rs_.airAccelerate;
    float accelWishspeed = wishspeed;
    if (rs_.airStopAccelerate > 0 && DotProduct(ps_->velocity, wishdir) < 0) {
        accel = rs_.airStopAccelerate;
    }
    if (rs_.strafeAccelerate > 0 && fmove == 0 && smove != 0) {
        if (rs_.strafeWishSpeed > 0) {
            accelWishspeed = std::min(accelWishspeed, rs_.strafeWishSpeed);
        }
        accel = rs_.strafeAccelerate;
    }
    Accelerate(wishdir, accelWishspeed, accel);

    if (rs_.airControl > 0) {
        AirControl(wishdir, wishspeed);
    }

    // we may have a very steep ground plane without a ground entity: slide along it
    if (pml_.groundPlane) {
        ClipVelocity(ps_->velocity, pml_.groundTrace.plane.normal, ps_->velocity, kOverclip);
    }

    StepSlideMove(true);
}

void Move::GrappleMove() {
    vec3_t v;
    VectorScale(pml_.forward, -16, v);
    VectorAdd(ps_->grapplePoint, v, v);

    vec3_t vel;
    VectorSubtract(v, ps_->origin, vel);
    const float vlen = VectorLength(vel);
    VectorNormalize(vel);

    if (vlen <= 100) {
        VectorScale(vel, 10 * vlen, vel);
    } else {
        VectorScale(vel, 800, vel);
    }
    VectorCopy(vel, ps_->velocity);
    pml_.groundPlane = false;
}

void Move::WalkMove() {
    if (pm_->waterlevel > 2 && DotProduct(pml_.forward, pml_.groundTrace.plane.normal) > 0) {
        WaterMove();  // begin swimming
        return;
    }

    if (CheckJump()) {
        // jumped away
        if (pm_->waterlevel > 1) {
            WaterMove();
        } else {
            AirMove();
        }
        return;
    }

    Friction();

    const float fmove = pm_->cmd.forwardmove;
    const float smove = pm_->cmd.rightmove;
    const float scale = CmdScale(pm_->cmd);

    SetMovementDir();

    // project the forward and right directions onto the ground plane
    pml_.forward[2] = 0;
    pml_.right[2] = 0;
    ClipVelocity(pml_.forward, pml_.groundTrace.plane.normal, pml_.forward, kOverclip);
    ClipVelocity(pml_.right, pml_.groundTrace.plane.normal, pml_.right, kOverclip);
    VectorNormalize(pml_.forward);
    VectorNormalize(pml_.right);

    // going up or down slopes, the wish velocity shouldn't have a zero z
    vec3_t wishvel;
    for (int i = 0; i < 3; i++) {
        wishvel[i] = pml_.forward[i] * fmove + pml_.right[i] * smove;
    }

    vec3_t wishdir;
    VectorCopy(wishvel, wishdir);
    float wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    // slower when ducking
    if (ps_->pm_flags & PMF_DUCKED) {
        wishspeed = std::min(wishspeed, static_cast<float>(ps_->speed) * rs_.duckScale);
    }

    // slower when wading or walking on the bottom
    if (pm_->waterlevel) {
        float waterScale = static_cast<float>(pm_->waterlevel / 3.0);
        waterScale = static_cast<float>(1.0 - (1.0 - rs_.swimScale) * waterScale);
        wishspeed = std::min(wishspeed, static_cast<float>(ps_->speed) * waterScale);
    }

    // when hit, the player temporarily loses full control
    const bool slick = (pml_.groundTrace.surfaceFlags & SURF_SLICK) || (ps_->pm_flags & PMF_TIME_KNOCKBACK);
    Accelerate(wishdir, wishspeed, slick ? rs_.airAccelerate : rs_.accelerate);

    if (slick) {
        ps_->velocity[2] -= static_cast<float>(ps_->gravity) * pml_.frametime;
    }

    const float vel = VectorLength(ps_->velocity);

    // slide along the ground plane, without losing speed up or down slopes
    ClipVelocity(ps_->velocity, pml_.groundTrace.plane.normal, ps_->velocity, kOverclip);
    VectorNormalize(ps_->velocity);
    VectorScale(ps_->velocity, vel, ps_->velocity);

    if (!ps_->velocity[0] && !ps_->velocity[1]) {
        return;  // standing still
    }

    StepSlideMove(false);
}

void Move::DeadMove() {
    if (!pml_.walking) {
        return;
    }
    // extra friction
    const float forward = VectorLength(ps_->velocity) - 20;
    if (forward <= 0) {
        VectorClear(ps_->velocity);
    } else {
        VectorNormalize(ps_->velocity);
        VectorScale(ps_->velocity, forward, ps_->velocity);
    }
}

void Move::NoclipMove() {
    ps_->viewheight = DEFAULT_VIEWHEIGHT;

    // friction
    const float speed = VectorLength(ps_->velocity);
    if (speed < 1) {
        VectorCopy(vec3_origin, ps_->velocity);
    } else {
        const float friction = rs_.friction * 1.5f;  // extra friction
        const float control = speed < rs_.stopSpeed ? rs_.stopSpeed : speed;
        const float drop = control * friction * pml_.frametime;
        const float newspeed = std::max(speed - drop, 0.0f) / speed;
        VectorScale(ps_->velocity, newspeed, ps_->velocity);
    }

    // accelerate
    const float scale = CmdScale(pm_->cmd);
    const float fmove = pm_->cmd.forwardmove;
    const float smove = pm_->cmd.rightmove;

    vec3_t wishvel;
    for (int i = 0; i < 3; i++) {
        wishvel[i] = pml_.forward[i] * fmove + pml_.right[i] * smove;
    }
    wishvel[2] += pm_->cmd.upmove;

    vec3_t wishdir;
    VectorCopy(wishvel, wishdir);
    float wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    Accelerate(wishdir, wishspeed, rs_.accelerate);

    VectorMA(ps_->origin, pml_.frametime, ps_->velocity, ps_->origin);
}

int Move::FootstepForSurface() const {
    if (pml_.groundTrace.surfaceFlags & SURF_NOSTEPS) {
        return 0;
    }
    if (pml_.groundTrace.surfaceFlags & SURF_METALSTEPS) {
        return EV_FOOTSTEP_METAL;
    }
    return EV_FOOTSTEP;
}

// Hard landings: animation and sound events.
void Move::CrashLand() {
    ForceLegsAnim((ps_->pm_flags & PMF_BACKWARDS_JUMP) ? LEGS_LANDB : LEGS_LAND);
    ps_->legsTimer = kTimerLand;

    // the exact velocity on landing
    const float dist = ps_->origin[2] - pml_.previousOrigin[2];
    const float vel = pml_.previousVelocity[2];
    const float acc = -static_cast<float>(ps_->gravity);

    const float a = acc / 2;
    const float b = vel;
    const float c = -dist;

    const float den = b * b - 4 * a * c;
    if (den < 0) {
        return;
    }
    // double math as in the C original
    const float t = static_cast<float>((-b - std::sqrt(static_cast<double>(den))) / (2 * a));

    float delta = vel + t * acc;
    delta = static_cast<float>(delta * delta * 0.0001);

    // ducking while falling doubles damage
    if (ps_->pm_flags & PMF_DUCKED) {
        delta *= 2;
    }

    // no falling damage when completely underwater, less in standing water
    if (pm_->waterlevel == 3) {
        return;
    }
    if (pm_->waterlevel == 2) {
        delta *= 0.25f;
    }
    if (pm_->waterlevel == 1) {
        delta *= 0.5f;
    }

    if (delta < 1) {
        return;
    }

    // SURF_NODAMAGE is for bounce pads: no damage and no crunch sound
    if (!(pml_.groundTrace.surfaceFlags & SURF_NODAMAGE)) {
        if (delta > 60) {
            AddEvent(EV_FALL_FAR);
        } else if (delta > 40) {
            // a pain grunt, so not when dead
            if (ps_->stats[STAT_HEALTH] > 0) {
                AddEvent(EV_FALL_MEDIUM);
            }
        } else if (delta > 7) {
            AddEvent(EV_FALL_SHORT);
        } else {
            AddEvent(FootstepForSurface());
        }
    }

    // start the footstep cycle over
    ps_->bobCycle = 0;
}

bool Move::CorrectAllSolid(trace_t& trace) {
    if (pm_->debugLevel) {
        Com_Printf("%i:allsolid\n", c_pmove);
    }

    // jitter around
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            for (int k = -1; k <= 1; k++) {
                vec3_t point;
                VectorCopy(ps_->origin, point);
                point[0] += static_cast<float>(i);
                point[1] += static_cast<float>(j);
                point[2] += static_cast<float>(k);
                pm_->trace(&trace, point, pm_->mins, pm_->maxs, point, ps_->clientNum, pm_->tracemask);
                if (!trace.allsolid) {
                    VectorSet(point, ps_->origin[0], ps_->origin[1], ps_->origin[2] - 0.25f);
                    pm_->trace(&trace, ps_->origin, pm_->mins, pm_->maxs, point, ps_->clientNum, pm_->tracemask);
                    pml_.groundTrace = trace;
                    return true;
                }
            }
        }
    }

    ps_->groundEntityNum = ENTITYNUM_NONE;
    pml_.groundPlane = false;
    pml_.walking = false;
    return false;
}

// The ground trace didn't hit a surface: free fall.
void Move::GroundTraceMissed() {
    if (ps_->groundEntityNum != ENTITYNUM_NONE) {
        // just went into free fall
        if (pm_->debugLevel) {
            Com_Printf("%i:lift\n", c_pmove);
        }

        // not in a jumping animation and the ground is a ways away: force it,
        // or the player would be backflipping down staircases
        vec3_t point;
        VectorCopy(ps_->origin, point);
        point[2] -= 64;

        trace_t trace;
        pm_->trace(&trace, ps_->origin, pm_->mins, pm_->maxs, point, ps_->clientNum, pm_->tracemask);
        if (trace.fraction == 1.0f) {
            ForceJumpAnim();
        }
    }

    ps_->groundEntityNum = ENTITYNUM_NONE;
    pml_.groundPlane = false;
    pml_.walking = false;
}

void Move::GroundTrace() {
    vec3_t point = {ps_->origin[0], ps_->origin[1], ps_->origin[2] - 0.25f};

    trace_t trace;
    pm_->trace(&trace, ps_->origin, pm_->mins, pm_->maxs, point, ps_->clientNum, pm_->tracemask);
    pml_.groundTrace = trace;

    // starting in a solid: try to correct
    if (trace.allsolid && !CorrectAllSolid(trace)) {
        return;
    }

    // didn't hit anything: free fall
    if (trace.fraction == 1.0f) {
        GroundTraceMissed();
        pml_.groundPlane = false;
        pml_.walking = false;
        return;
    }

    // thrown off the ground
    if (ps_->velocity[2] > 0 && DotProduct(ps_->velocity, trace.plane.normal) > 10) {
        if (pm_->debugLevel) {
            Com_Printf("%i:kickoff\n", c_pmove);
        }
        ForceJumpAnim();
        ps_->groundEntityNum = ENTITYNUM_NONE;
        pml_.groundPlane = false;
        pml_.walking = false;
        return;
    }

    // slopes that are too steep aren't ground
    if (trace.plane.normal[2] < kMinWalkNormal) {
        if (pm_->debugLevel) {
            Com_Printf("%i:steep\n", c_pmove);
        }
        ps_->groundEntityNum = ENTITYNUM_NONE;
        pml_.groundPlane = true;
        pml_.walking = false;
        return;
    }

    pml_.groundPlane = true;
    pml_.walking = true;

    // hitting solid ground ends a waterjump
    if (ps_->pm_flags & PMF_TIME_WATERJUMP) {
        ps_->pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND);
        ps_->pm_time = 0;
    }

    if (ps_->groundEntityNum == ENTITYNUM_NONE) {
        // just hit the ground
        if (pm_->debugLevel) {
            Com_Printf("%i:Land\n", c_pmove);
        }
        CrashLand();

        // no landing time when just going down a slope
        if (pml_.previousVelocity[2] < -200) {
            // no jumping for a little while
            ps_->pm_flags |= PMF_TIME_LAND;
            ps_->pm_time = 250;
        }
    }

    ps_->groundEntityNum = trace.entityNum;
    AddTouchEnt(trace.entityNum);
}

void Move::SetWaterLevel() {
    // waterlevel, accounting for ducking
    pm_->waterlevel = 0;
    pm_->watertype = 0;

    vec3_t point = {ps_->origin[0], ps_->origin[1], ps_->origin[2] + MINS_Z + 1};
    int cont = pm_->pointcontents(point, ps_->clientNum);

    if (cont & MASK_WATER) {
        const int sample2 = ps_->viewheight - MINS_Z;
        const int sample1 = sample2 / 2;

        pm_->watertype = cont;
        pm_->waterlevel = 1;
        point[2] = ps_->origin[2] + MINS_Z + static_cast<float>(sample1);
        cont = pm_->pointcontents(point, ps_->clientNum);
        if (cont & MASK_WATER) {
            pm_->waterlevel = 2;
            point[2] = ps_->origin[2] + MINS_Z + static_cast<float>(sample2);
            cont = pm_->pointcontents(point, ps_->clientNum);
            if (cont & MASK_WATER) {
                pm_->waterlevel = 3;
            }
        }
    }
}

// Sets mins, maxs and the viewheight.
void Move::CheckDuck() {
    if (ps_->powerups[PW_INVULNERABILITY]) {
        if (ps_->pm_flags & PMF_INVULEXPAND) {
            // the invulnerability sphere has a 42 unit radius
            VectorSet(pm_->mins, -INVUL_RADIUS, -INVUL_RADIUS, -INVUL_RADIUS);
            VectorSet(pm_->maxs, INVUL_RADIUS, INVUL_RADIUS, INVUL_RADIUS);
        } else {
            VectorSet(pm_->mins, -PLAYER_WIDTH, -PLAYER_WIDTH, MINS_Z);
            VectorSet(pm_->maxs, PLAYER_WIDTH, PLAYER_WIDTH, 16);
        }
        ps_->pm_flags |= PMF_DUCKED;
        ps_->viewheight = CROUCH_VIEWHEIGHT;
        return;
    }
    ps_->pm_flags &= ~PMF_INVULEXPAND;

    pm_->mins[0] = -PLAYER_WIDTH;
    pm_->mins[1] = -PLAYER_WIDTH;
    pm_->maxs[0] = PLAYER_WIDTH;
    pm_->maxs[1] = PLAYER_WIDTH;
    pm_->mins[2] = MINS_Z;

    if (ps_->pm_type == PM_DEAD) {
        pm_->maxs[2] = DEAD_HEIGHT;
        ps_->viewheight = DEAD_VIEWHEIGHT;
        return;
    }

    if (pm_->cmd.upmove < 0) {
        ps_->pm_flags |= PMF_DUCKED;
    } else if (ps_->pm_flags & PMF_DUCKED) {
        // stand up if possible
        pm_->maxs[2] = DEFAULT_HEIGHT;
        trace_t trace;
        pm_->trace(&trace, ps_->origin, pm_->mins, pm_->maxs, ps_->origin, ps_->clientNum, pm_->tracemask);
        if (!trace.allsolid) {
            ps_->pm_flags &= ~PMF_DUCKED;
        }
    }

    if (ps_->pm_flags & PMF_DUCKED) {
        pm_->maxs[2] = CROUCH_HEIGHT;
        ps_->viewheight = CROUCH_VIEWHEIGHT;
    } else {
        pm_->maxs[2] = DEFAULT_HEIGHT;
        ps_->viewheight = DEFAULT_VIEWHEIGHT;
    }
}

void Move::Footsteps() {
    // speed and cycle for all cyclic walking effects
    pm_->xyspeed = std::sqrt(ps_->velocity[0] * ps_->velocity[0] + ps_->velocity[1] * ps_->velocity[1]);

    if (ps_->groundEntityNum == ENTITYNUM_NONE) {
        if (ps_->powerups[PW_INVULNERABILITY]) {
            ContinueLegsAnim(LEGS_IDLECR);
        }
        // airborne leaves the position in the cycle intact, but doesn't advance it
        if (pm_->waterlevel > 1) {
            ContinueLegsAnim(LEGS_SWIM);
        }
        return;
    }

    // not trying to move
    if (!pm_->cmd.forwardmove && !pm_->cmd.rightmove) {
        if (pm_->xyspeed < 5) {
            ps_->bobCycle = 0;  // start at the beginning of the cycle again
            ContinueLegsAnim((ps_->pm_flags & PMF_DUCKED) ? LEGS_IDLECR : LEGS_IDLE);
        }
        return;
    }

    bool footstep = false;
    float bobmove;
    const bool backwards = ps_->pm_flags & PMF_BACKWARDS_RUN;

    if (ps_->pm_flags & PMF_DUCKED) {
        bobmove = 0.5f;  // ducked characters bob much faster, and never play footsteps
        ContinueLegsAnim(backwards ? LEGS_BACKCR : LEGS_WALKCR);
    } else if (!(pm_->cmd.buttons & BUTTON_WALKING)) {
        bobmove = 0.4f;  // faster speeds bob faster
        ContinueLegsAnim(backwards ? LEGS_BACK : LEGS_RUN);
        footstep = true;
    } else {
        bobmove = 0.3f;  // walking bobs slow
        ContinueLegsAnim(backwards ? LEGS_BACKWALK : LEGS_WALK);
    }

    // footstep / splash sounds
    const int old = ps_->bobCycle;
    ps_->bobCycle = static_cast<int>(static_cast<float>(old) + bobmove * static_cast<float>(pml_.msec)) & 255;

    // just crossed a cycle boundary: play a footstep event
    if (((old + 64) ^ (ps_->bobCycle + 64)) & 128) {
        if (pm_->waterlevel == 0) {
            // on the ground only running plays sounds
            if (footstep && !pm_->noFootsteps) {
                AddEvent(FootstepForSurface());
            }
        } else if (pm_->waterlevel == 1) {
            AddEvent(EV_FOOTSPLASH);
        } else if (pm_->waterlevel == 2) {
            AddEvent(EV_SWIM);  // wading / swimming at the surface
        }
        // no sound when completely underwater
    }
}

// Sound events for entering and leaving water.
void Move::WaterEvents() {
    if (!pml_.previousWaterLevel && pm_->waterlevel) {
        AddEvent(EV_WATER_TOUCH);
    }
    if (pml_.previousWaterLevel && !pm_->waterlevel) {
        AddEvent(EV_WATER_LEAVE);
    }
    if (pml_.previousWaterLevel != 3 && pm_->waterlevel == 3) {
        AddEvent(EV_WATER_UNDER);  // head just went under
    }
    if (pml_.previousWaterLevel == 3 && pm_->waterlevel != 3) {
        AddEvent(EV_WATER_CLEAR);  // head just came out
    }
}

void Move::BeginWeaponChange(int weapon) {
    if (weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS) {
        return;
    }
    if (!(ps_->stats[STAT_WEAPONS] & (1 << weapon))) {
        return;
    }
    if (ps_->weaponstate == WEAPON_DROPPING) {
        return;
    }
    AddEvent(EV_CHANGE_WEAPON);
    ps_->weaponstate = WEAPON_DROPPING;
    ps_->weaponTime += 200;
    StartTorsoAnim(TORSO_DROP);
}

void Move::FinishWeaponChange() {
    int weapon = pm_->cmd.weapon;
    if (weapon < WP_NONE || weapon >= WP_NUM_WEAPONS) {
        weapon = WP_NONE;
    }
    if (!(ps_->stats[STAT_WEAPONS] & (1 << weapon))) {
        weapon = WP_NONE;
    }
    ps_->weapon = weapon;
    ps_->weaponstate = WEAPON_RAISING;
    ps_->weaponTime += 250;
    StartTorsoAnim(TORSO_RAISE);
}

void Move::TorsoAnimation() {
    if (ps_->weaponstate == WEAPON_READY) {
        ContinueTorsoAnim(ps_->weapon == WP_GAUNTLET ? TORSO_STAND2 : TORSO_STAND);
    }
}

// Weapon events and the weapon counter.
void Move::Weapon() {
    // no attack until all buttons are up
    if (ps_->pm_flags & PMF_RESPAWNED) {
        return;
    }
    if (ps_->persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return;
    }
    if (ps_->stats[STAT_HEALTH] <= 0) {
        ps_->weapon = WP_NONE;
        return;
    }

    // item use
    if (pm_->cmd.buttons & BUTTON_USE_HOLDABLE) {
        if (!(ps_->pm_flags & PMF_USE_ITEM_HELD)) {
            const gitem_t& item = bg_itemlist[ps_->stats[STAT_HOLDABLE_ITEM]];
            if (item.giTag == HI_MEDKIT && ps_->stats[STAT_HEALTH] >= ps_->stats[STAT_MAX_HEALTH] + 25) {
                // don't use a medkit at max health
            } else {
                ps_->pm_flags |= PMF_USE_ITEM_HELD;
                AddEvent(EV_USE_ITEM0 + item.giTag);
                ps_->stats[STAT_HOLDABLE_ITEM] = 0;
            }
            return;
        }
    } else {
        ps_->pm_flags &= ~PMF_USE_ITEM_HELD;
    }

    if (ps_->weaponTime > 0) {
        ps_->weaponTime -= pml_.msec;
    }

    // can't change while firing, but can change again while lowering or raising
    if ((ps_->weaponTime <= 0 || ps_->weaponstate != WEAPON_FIRING) && ps_->weapon != pm_->cmd.weapon) {
        BeginWeaponChange(pm_->cmd.weapon);
    }

    if (ps_->weaponTime > 0) {
        return;
    }

    if (ps_->weaponstate == WEAPON_DROPPING) {
        FinishWeaponChange();
        return;
    }

    if (ps_->weaponstate == WEAPON_RAISING) {
        ps_->weaponstate = WEAPON_READY;
        StartTorsoAnim(ps_->weapon == WP_GAUNTLET ? TORSO_STAND2 : TORSO_STAND);
        return;
    }

    if (!(pm_->cmd.buttons & BUTTON_ATTACK)) {
        ps_->weaponTime = 0;
        ps_->weaponstate = WEAPON_READY;
        return;
    }

    // start the animation even when out of ammo
    if (ps_->weapon == WP_GAUNTLET) {
        // the gauntlet only "fires" when it actually hits something
        if (!pm_->gauntletHit) {
            ps_->weaponTime = 0;
            ps_->weaponstate = WEAPON_READY;
            return;
        }
        StartTorsoAnim(TORSO_ATTACK2);
    } else {
        StartTorsoAnim(TORSO_ATTACK);
    }

    ps_->weaponstate = WEAPON_FIRING;

    if (!ps_->ammo[ps_->weapon]) {
        AddEvent(EV_NOAMMO);
        ps_->weaponTime += 500;
        return;
    }

    // -1 is infinite
    if (ps_->ammo[ps_->weapon] != -1) {
        ps_->ammo[ps_->weapon]--;
    }

    AddEvent(EV_FIRE_WEAPON);

    int addTime;
    switch (ps_->weapon) {
        case WP_LIGHTNING: addTime = 50; break;
        case WP_SHOTGUN: addTime = 1000; break;
        case WP_MACHINEGUN: addTime = 100; break;
        case WP_GRENADE_LAUNCHER: addTime = 800; break;
        case WP_ROCKET_LAUNCHER: addTime = 800; break;
        case WP_PLASMAGUN: addTime = 100; break;
        case WP_RAILGUN: addTime = 1500; break;
        case WP_BFG: addTime = 200; break;
        case WP_GRAPPLING_HOOK: addTime = 400; break;
        case WP_GAUNTLET:
        default: addTime = 400; break;
    }

    if (ps_->powerups[PW_HASTE]) {
        addTime = static_cast<int>(static_cast<float>(addTime) / 1.3f);
    }

    ps_->weaponTime += addTime;
}

void Move::Animate() {
    if ((pm_->cmd.buttons & BUTTON_GESTURE) && ps_->torsoTimer == 0) {
        StartTorsoAnim(TORSO_GESTURE);
        ps_->torsoTimer = kTimerGesture;
        AddEvent(EV_TAUNT);
    }
}

void Move::DropTimers() {
    // misc timing counter
    if (ps_->pm_time) {
        if (pml_.msec >= ps_->pm_time) {
            ps_->pm_flags &= ~PMF_ALL_TIMES;
            ps_->pm_time = 0;
        } else {
            ps_->pm_time -= pml_.msec;
        }
    }

    // animation counters
    if (ps_->legsTimer > 0) {
        ps_->legsTimer = std::max(ps_->legsTimer - pml_.msec, 0);
    }
    if (ps_->torsoTimer > 0) {
        ps_->torsoTimer = std::max(ps_->torsoTimer - pml_.msec, 0);
    }

    // double-jump window
    if (ps_->stats[kStatJumpTime] > 0) {
        ps_->stats[kStatJumpTime] = std::max(ps_->stats[kStatJumpTime] - pml_.msec, 0);
    }
}

void Move::Single() {
    pmove_t* pmove = pm_;

    // lets us debug movement problems with a journal, by breaking on the previous step
    c_pmove++;

    // clear results
    pm_->numtouch = 0;
    pm_->watertype = 0;
    pm_->waterlevel = 0;

    if (ps_->stats[STAT_HEALTH] <= 0) {
        pm_->tracemask &= ~CONTENTS_BODY;  // corpses can fly through bodies
    }

    // walking button clear while running, against proxy no-footsteps cheats
    if (std::abs(pm_->cmd.forwardmove) > 64 || std::abs(pm_->cmd.rightmove) > 64) {
        pm_->cmd.buttons &= ~BUTTON_WALKING;
    }

    // talk balloon flag
    if (pm_->cmd.buttons & BUTTON_TALK) {
        ps_->eFlags |= EF_TALK;
    } else {
        ps_->eFlags &= ~EF_TALK;
    }

    // firing flag for continuous beam weapons
    if (!(ps_->pm_flags & PMF_RESPAWNED) && ps_->pm_type != PM_INTERMISSION && ps_->pm_type != PM_NOCLIP &&
        (pm_->cmd.buttons & BUTTON_ATTACK) && ps_->ammo[ps_->weapon]) {
        ps_->eFlags |= EF_FIRING;
    } else {
        ps_->eFlags &= ~EF_FIRING;
    }

    // clear the respawned flag once attack and use are released
    if (ps_->stats[STAT_HEALTH] > 0 && !(pm_->cmd.buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE))) {
        ps_->pm_flags &= ~PMF_RESPAWNED;
    }

    // talk button down: no other input, so an intercept proxy can't add fake
    // talk balloons. Keep the talk button for when the cmd is reused.
    if (pmove->cmd.buttons & BUTTON_TALK) {
        pmove->cmd.buttons = BUTTON_TALK;
        pmove->cmd.forwardmove = 0;
        pmove->cmd.rightmove = 0;
        pmove->cmd.upmove = 0;
    }

    pml_ = Locals{};

    // the time
    pml_.msec = std::clamp(pmove->cmd.serverTime - ps_->commandTime, 1, 200);
    ps_->commandTime = pmove->cmd.serverTime;

    // old origin in case we get stuck, old velocity for crash landing
    VectorCopy(ps_->origin, pml_.previousOrigin);
    VectorCopy(ps_->velocity, pml_.previousVelocity);

    pml_.frametime = static_cast<float>(pml_.msec * 0.001);  // double, as in the C original

    UpdateViewAngles(ps_, &pm_->cmd);
    AngleVectors(ps_->viewangles, pml_.forward, pml_.right, pml_.up);

    if (pm_->cmd.upmove < 10) {
        ps_->pm_flags &= ~PMF_JUMP_HELD;  // not holding jump
    }

    // backpedaling animations
    if (pm_->cmd.forwardmove < 0) {
        ps_->pm_flags |= PMF_BACKWARDS_RUN;
    } else if (pm_->cmd.forwardmove > 0 || (pm_->cmd.forwardmove == 0 && pm_->cmd.rightmove)) {
        ps_->pm_flags &= ~PMF_BACKWARDS_RUN;
    }

    if (ps_->pm_type >= PM_DEAD) {
        pm_->cmd.forwardmove = 0;
        pm_->cmd.rightmove = 0;
        pm_->cmd.upmove = 0;
    }

    if (ps_->pm_type == PM_SPECTATOR) {
        CheckDuck();
        FlyMove();
        DropTimers();
        return;
    }

    if (ps_->pm_type == PM_NOCLIP) {
        NoclipMove();
        DropTimers();
        return;
    }

    if (ps_->pm_type == PM_FREEZE || ps_->pm_type == PM_INTERMISSION || ps_->pm_type == PM_SPINTERMISSION) {
        return;  // no movement at all
    }

    SetWaterLevel();
    pml_.previousWaterLevel = pmove->waterlevel;

    CheckDuck();
    GroundTrace();

    if (ps_->pm_type == PM_DEAD) {
        DeadMove();
    }

    DropTimers();

    if (ps_->powerups[PW_FLIGHT]) {
        FlyMove();  // no jumping, different friction
    } else if (ps_->pm_flags & PMF_GRAPPLE_PULL) {
        GrappleMove();
        AirMove();  // can wiggle a bit
    } else if (ps_->pm_flags & PMF_TIME_WATERJUMP) {
        WaterJumpMove();
    } else if (pm_->waterlevel > 1) {
        WaterMove();
    } else if (pml_.walking) {
        WalkMove();
    } else {
        AirMove();
    }

    Animate();

    GroundTrace();
    SetWaterLevel();

    Weapon();
    TorsoAnimation();
    Footsteps();
    WaterEvents();

    SnapToInt(ps_->velocity);
}

void UpdateViewAngles(playerState_t* ps, const usercmd_t* cmd) {
    if (ps->pm_type == PM_INTERMISSION || ps->pm_type == PM_SPINTERMISSION) {
        return;  // no view changes at all
    }
    if (ps->pm_type != PM_SPECTATOR && ps->stats[STAT_HEALTH] <= 0) {
        return;  // no view changes at all
    }

    // circularly clamp the angles with deltas
    for (int i = 0; i < 3; i++) {
        short temp = static_cast<short>(cmd->angles[i] + ps->delta_angles[i]);
        if (i == PITCH) {
            // no looking up or down more than 90 degrees
            if (temp > 16000) {
                ps->delta_angles[i] = 16000 - cmd->angles[i];
                temp = 16000;
            } else if (temp < -16000) {
                ps->delta_angles[i] = -16000 - cmd->angles[i];
                temp = -16000;
            }
        }
        ps->viewangles[i] = SHORT2ANGLE(temp);
    }
}

void Pmove(pmove_t* pmove) {
    const int finalTime = pmove->cmd.serverTime;
    if (finalTime < pmove->ps->commandTime) {
        return;  // shouldn't happen
    }
    if (finalTime > pmove->ps->commandTime + 1000) {
        pmove->ps->commandTime = finalTime - 1000;
    }

    pmove->ps->pmove_framecount = (pmove->ps->pmove_framecount + 1) & ((1 << PS_PMOVEFRAMECOUNTBITS) - 1);

    const Ruleset& rs = RulesetAt(pmove->ps->stats[kStatRuleset]);

    // chop up long moves, against framerate-dependent behaviour
    while (pmove->ps->commandTime != finalTime) {
        int msec = finalTime - pmove->ps->commandTime;
        if (pmove->pmove_fixed) {
            msec = std::min(msec, pmove->pmove_msec);
        } else {
            msec = std::min(msec, 66);
        }
        pmove->cmd.serverTime = pmove->ps->commandTime + msec;
        Move(pmove, rs).Single();

        if (pmove->ps->pm_flags & PMF_JUMP_HELD) {
            pmove->cmd.upmove = 20;
        }
    }
}

}  // namespace skaar::move
