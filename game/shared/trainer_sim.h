// The aim-trainer simulation: targets, their motion, hit tests, scoring and
// the run record. No engine calls, so the cgame drives it and tests can too.
//
// Determinism: given the scenario, seed and the same input calls (time, eye,
// angles, buttons), a run reproduces exactly at any frame rate. Target motion
// is piecewise linear in angle space with exact event times; every target has
// its own RNG stream, and spawns draw from a separate one in kill order.
//
// Angles are degrees. yaw grows to the left (q3), pitch grows UP here (the
// cgame negates q3's pitch, which grows down).
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <cstdint>
#include <vector>

#include <optional>

#include "runlog.h"
#include "scenario.h"
#include "weapons.h"

namespace skaar {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

class Rng {
public:
    explicit Rng(uint32_t seed = 1) : state_(seed ? seed : 0x9e3779b9u) {}
    uint32_t Next();
    float Unit();                       // [0, 1)
    float In(Range r) { return r.min + (r.max - r.min) * Unit(); }
    float Sign() { return Next() & 1 ? 1.0f : -1.0f; }

private:
    uint32_t state_;
};

struct Target {
    int serial = 0;          // spawn order, from 0
    bool alive = false;
    float spawnT = 0;        // seconds into the run
    float yaw = 0, pitch = 0, dist = 0;  // relative to the anchor and base yaw
    float vyaw = 0, vpitch = 0;          // degrees per second
    float nextChange = 0;    // time of the next direction change
    float health = 1;
    float lastHitT = -1;     // for the hit flash
    int cell = -1;           // grid cell, if any
    float stateT = 0;        // time yaw/pitch are valid for
    Rng rng;
};

struct TrainerEvents {
    int hits = 0, kills = 0, misses = 0;  // since the last Input call
};

class TrainerSim {
public:
    // eye: where angles are measured from (the spawn eye); baseYaw: the spawn facing
    // weapon: the gun for scenarios with "weapon" set (nullptr: look it up in the weapon registry)
    void Start(const Scenario& scenario, uint32_t seed, Vec3 eye, float baseYaw, const WeaponDef* weapon = nullptr);

    // With a weapon: the player's movement for its inaccuracy, before each Input
    void SetMovement(float horizontalSpeed, bool onGround, bool crouched);

    // With a weapon: the view kick at time t (degrees up / right), cameraFollow share, easing back
    void Punch(float t, float& up, float& right) const;
    bool HasWeapon() const { return weapon_.has_value(); }

    // Moves targets to time t (seconds since the run started). Times must not go back.
    void Advance(float t);

    // One usercmd: view at time t. pressed = attack went down in this command,
    // held = attack is down, dt = seconds since the previous command.
    TrainerEvents Input(float t, Vec3 eye, float yaw, float pitch, bool pressed, bool held, float dt);

    // Replays: kill the target with this serial at time t, as the recorded run did.
    bool KillSerial(int serial, float t);
    // Replays: mark a recorded hit for the flash.
    void FlashSerial(int serial, float t);

    bool Finished() const { return finished_; }
    float Time() const { return time_; }
    float Remaining() const;
    const Scenario& GetScenario() const { return scenario_; }
    const std::vector<Target>& Targets() const { return targets_; }
    Vec3 TargetPosition(const Target& t) const;
    float TargetRadius() const { return scenario_.targets.radius; }

    // Live numbers for the HUD
    float Score() const;
    float Accuracy() const;    // 0..1
    float MeanTtk() const;     // ms
    int Kills() const { return kills_; }
    int Shots() const { return shots_; }
    int Hits() const { return hits_; }
    int Expired() const { return expired_; }

    // The run so far as a record (summary fields the sim knows; the caller adds
    // time, sensitivity, fov and the view samples).
    RunRecord Record() const;

private:
    void Spawn(float t);
    void Kill(Target& target, float t);
    void MoveTo(Target& target, float t);
    void NewVelocity(Target& target);
    int FreeCell();

    Scenario scenario_;
    uint32_t seed_ = 0;
    Vec3 anchor_;
    float baseYaw_ = 0;
    float time_ = 0;
    bool finished_ = false;

    std::vector<Target> targets_;
    Rng spawnRng_;
    int nextSerial_ = 0;
    int lastCell_ = -1;

    int shots_ = 0, hits_ = 0, kills_ = 0, expired_ = 0;
    float firing_ = 0, onTarget_ = 0;  // hold mode, seconds
    double ttkSum_ = 0;
    float nextTrackSample_ = 0;

    std::optional<WeaponDef> weapon_;
    float moveSpeed_ = 0;
    bool onGround_ = true, crouched_ = false;
    int spray_ = 0;              // shots in the current spray
    float lastShotT_ = -1000;
    float nextShotT_ = 0;

    std::vector<ShotEvent> shotLog_;
    std::vector<KillEvent> killLog_;
};

// Helpers shared with the cgame
Vec3 AnglesToDir(float yaw, float pitch);  // yaw left+, pitch up+
float AngleDelta(float a, float b);        // a - b wrapped to (-180, 180]

}  // namespace skaar
