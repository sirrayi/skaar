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
// Sliding and stepping against the world: a port of bg_slidemove.c.
// in: origin, velocity, bounds, ground plane, trace function
// out: origin, velocity, impacts, stepped up or not
//
// Ported to C++ for Skaar. GPLv2 or later.
#include "move_local.h"

namespace skaar::move {

namespace {
constexpr int kMaxClipPlanes = 5;
}

// Returns true if the velocity was clipped in some way.
bool Move::SlideMove(bool gravity) {
    constexpr int numbumps = 4;
    vec3_t planes[kMaxClipPlanes];
    vec3_t primalVelocity;
    vec3_t endVelocity{};

    VectorCopy(ps_->velocity, primalVelocity);

    if (gravity) {
        VectorCopy(ps_->velocity, endVelocity);
        endVelocity[2] -= static_cast<float>(ps_->gravity) * pml_.frametime;
        ps_->velocity[2] = static_cast<float>((ps_->velocity[2] + endVelocity[2]) * 0.5);
        primalVelocity[2] = endVelocity[2];
        if (pml_.groundPlane) {
            // slide along the ground plane
            ClipVelocity(ps_->velocity, pml_.groundTrace.plane.normal, ps_->velocity, kOverclip);
        }
    }

    float timeLeft = pml_.frametime;

    // never turn against the ground plane
    int numplanes = 0;
    if (pml_.groundPlane) {
        VectorCopy(pml_.groundTrace.plane.normal, planes[0]);
        numplanes = 1;
    }

    // never turn against the original velocity
    VectorNormalize2(ps_->velocity, planes[numplanes]);
    numplanes++;

    int bumpcount;
    for (bumpcount = 0; bumpcount < numbumps; bumpcount++) {
        // where we're trying to move to, and can we make it there
        vec3_t end;
        VectorMA(ps_->origin, timeLeft, ps_->velocity, end);
        trace_t trace;
        pm_->trace(&trace, ps_->origin, pm_->mins, pm_->maxs, end, ps_->clientNum, pm_->tracemask);

        if (trace.allsolid) {
            // completely trapped in another solid: no falling damage build-up,
            // but allow sideways acceleration
            ps_->velocity[2] = 0;
            return true;
        }

        if (trace.fraction > 0) {
            VectorCopy(trace.endpos, ps_->origin);  // covered some distance
        }

        if (trace.fraction == 1) {
            break;  // moved the entire distance
        }

        // entity for contact
        AddTouchEnt(trace.entityNum);

        timeLeft -= timeLeft * trace.fraction;

        if (numplanes >= kMaxClipPlanes) {
            // shouldn't really happen
            VectorClear(ps_->velocity);
            return true;
        }

        // the same plane we hit before: nudge the velocity out along it, which
        // fixes some epsilon issues with non-axial planes
        int i;
        for (i = 0; i < numplanes; i++) {
            if (DotProduct(trace.plane.normal, planes[i]) > 0.99) {
                VectorAdd(trace.plane.normal, ps_->velocity, ps_->velocity);
                break;
            }
        }
        if (i < numplanes) {
            continue;
        }
        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;

        // make the velocity parallel to all of the clip planes:
        // find a plane it enters
        for (i = 0; i < numplanes; i++) {
            const float into = DotProduct(ps_->velocity, planes[i]);
            if (into >= 0.1) {
                continue;  // doesn't interact with the plane
            }

            // how hard we're hitting things
            if (-into > pml_.impactSpeed) {
                pml_.impactSpeed = -into;
            }

            // slide along the plane
            vec3_t clipVelocity;
            vec3_t endClipVelocity{};
            ClipVelocity(ps_->velocity, planes[i], clipVelocity, kOverclip);
            if (gravity) {
                ClipVelocity(endVelocity, planes[i], endClipVelocity, kOverclip);
            }

            // a second plane the new move enters
            for (int j = 0; j < numplanes; j++) {
                if (j == i) {
                    continue;
                }
                if (DotProduct(clipVelocity, planes[j]) >= 0.1) {
                    continue;  // doesn't interact with the plane
                }

                // try clipping the move to the plane
                ClipVelocity(clipVelocity, planes[j], clipVelocity, kOverclip);
                if (gravity) {
                    ClipVelocity(endClipVelocity, planes[j], endClipVelocity, kOverclip);
                }

                // does it go back into the first clip plane
                if (DotProduct(clipVelocity, planes[i]) >= 0) {
                    continue;
                }

                // slide the original velocity along the crease
                vec3_t dir;
                CrossProduct(planes[i], planes[j], dir);
                VectorNormalize(dir);
                float d = DotProduct(dir, ps_->velocity);
                VectorScale(dir, d, clipVelocity);

                if (gravity) {
                    CrossProduct(planes[i], planes[j], dir);
                    VectorNormalize(dir);
                    d = DotProduct(dir, endVelocity);
                    VectorScale(dir, d, endClipVelocity);
                }

                // a third plane the new move enters
                for (int k = 0; k < numplanes; k++) {
                    if (k == i || k == j) {
                        continue;
                    }
                    if (DotProduct(clipVelocity, planes[k]) >= 0.1) {
                        continue;  // doesn't interact with the plane
                    }
                    // stop dead at a triple plane interaction
                    VectorClear(ps_->velocity);
                    return true;
                }
            }

            // fixed all interactions: try another move
            VectorCopy(clipVelocity, ps_->velocity);
            if (gravity) {
                VectorCopy(endClipVelocity, endVelocity);
            }
            break;
        }
    }

    if (gravity) {
        VectorCopy(endVelocity, ps_->velocity);
    }

    // don't change the velocity while in a timer
    if (ps_->pm_time) {
        VectorCopy(primalVelocity, ps_->velocity);
    }

    return bumpcount != 0;
}

void Move::StepSlideMove(bool gravity) {
    vec3_t startO, startV;
    VectorCopy(ps_->origin, startO);
    VectorCopy(ps_->velocity, startV);

    if (!SlideMove(gravity)) {
        return;  // got exactly where we wanted to go first try
    }

    vec3_t down;
    VectorCopy(startO, down);
    down[2] -= kStepSize;
    trace_t trace;
    pm_->trace(&trace, startO, pm_->mins, pm_->maxs, down, ps_->clientNum, pm_->tracemask);

    // never step up while still moving up
    vec3_t up = {0, 0, 1};
    if (ps_->velocity[2] > 0 && (trace.fraction == 1.0f || DotProduct(trace.plane.normal, up) < 0.7)) {
        return;
    }

    VectorCopy(startO, up);
    up[2] += kStepSize;

    // the player position a step higher
    pm_->trace(&trace, startO, pm_->mins, pm_->maxs, up, ps_->clientNum, pm_->tracemask);
    if (trace.allsolid) {
        if (pm_->debugLevel) {
            Com_Printf("bend can't step\n");
        }
        return;  // can't step up
    }

    const float stepSize = trace.endpos[2] - startO[2];

    // slide move from there
    VectorCopy(trace.endpos, ps_->origin);
    VectorCopy(startV, ps_->velocity);
    SlideMove(gravity);

    // push down the final amount
    VectorCopy(ps_->origin, down);
    down[2] -= stepSize;
    pm_->trace(&trace, ps_->origin, pm_->mins, pm_->maxs, down, ps_->clientNum, pm_->tracemask);
    if (!trace.allsolid) {
        VectorCopy(trace.endpos, ps_->origin);
    }
    if (trace.fraction < 1.0f) {
        ClipVelocity(ps_->velocity, trace.plane.normal, ps_->velocity, kOverclip);
    }

    // step events
    const float delta = ps_->origin[2] - startO[2];
    if (delta > 2) {
        if (delta < 7) {
            AddEvent(EV_STEP_4);
        } else if (delta < 11) {
            AddEvent(EV_STEP_8);
        } else if (delta < 15) {
            AddEvent(EV_STEP_12);
        } else {
            AddEvent(EV_STEP_16);
        }
    }
    if (pm_->debugLevel) {
        Com_Printf("stepped\n");
    }
}

}  // namespace skaar::move
