// This file is part of Skaar. GPLv2 or later.
#include "trainer_sim.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace skaar {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = 180.0f / kPi;
constexpr float kNever = std::numeric_limits<float>::infinity();
constexpr float kTrackSampleInterval = 0.05f;  // hold mode: aim-error samples at 20 Hz

uint32_t Mix(uint32_t x) {
    // lowbias32 hash, so neighbouring serials get unrelated streams
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

}  // namespace

uint32_t Rng::Next() {
    // xorshift32
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
}

float Rng::Unit() { return static_cast<float>(Next() >> 8) / 16777216.0f; }

Vec3 AnglesToDir(float yaw, float pitch) {
    const float y = yaw / kDeg, p = pitch / kDeg;
    return {std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), std::sin(p)};
}

float AngleDelta(float a, float b) {
    float d = std::fmod(a - b, 360.0f);
    if (d > 180) {
        d -= 360;
    } else if (d <= -180) {
        d += 360;
    }
    return d;
}

void TrainerSim::Start(const Scenario& scenario, uint32_t seed, Vec3 eye, float baseYaw) {
    *this = TrainerSim();
    scenario_ = scenario;
    seed_ = seed;
    anchor_ = eye;
    baseYaw_ = baseYaw;
    spawnRng_ = Rng(Mix(seed ^ 0xa5a5a5a5u));
    targets_.resize(scenario.targets.alive);
    for (int i = 0; i < scenario.targets.alive; i++) {
        Spawn(0);
    }
}

float TrainerSim::Remaining() const { return std::max(0.0f, scenario_.duration - time_); }

Vec3 TrainerSim::TargetPosition(const Target& t) const {
    const Vec3 d = AnglesToDir(baseYaw_ + t.yaw, t.pitch);
    return {anchor_.x + d.x * t.dist, anchor_.y + d.y * t.dist, anchor_.z + d.z * t.dist};
}

int TrainerSim::FreeCell() {
    const auto& o = scenario_.targets;
    const int cells = o.cols * o.rows;
    std::vector<int> free;
    for (int c = 0; c < cells; c++) {
        if (c == lastCell_) {
            continue;
        }
        bool used = false;
        for (const Target& t : targets_) {
            used = used || (t.alive && t.cell == c);
        }
        if (!used) {
            free.push_back(c);
        }
    }
    if (free.empty()) {
        return -1;  // can't happen: ParseScenario requires cells > alive
    }
    return free[spawnRng_.Next() % free.size()];
}

void TrainerSim::Spawn(float t) {
    // reuse the first dead slot
    Target* slot = nullptr;
    for (Target& target : targets_) {
        if (!target.alive) {
            slot = &target;
            break;
        }
    }
    if (!slot) {
        return;
    }

    const auto& o = scenario_.targets;
    Target n;
    n.serial = nextSerial_++;
    n.alive = true;
    n.spawnT = t;
    n.health = o.health;
    n.rng = Rng(Mix(seed_ + 0x9e3779b9u * static_cast<uint32_t>(n.serial + 1)));
    n.dist = spawnRng_.In(o.distance);

    if (o.shape == SpawnShape::kGrid) {
        n.cell = FreeCell();
        lastCell_ = n.cell;
        const int col = n.cell % o.cols, row = n.cell / o.cols;
        // column 0 is the leftmost, i.e. the largest yaw
        n.yaw = o.yaw.max - (col + 0.5f) / o.cols * (o.yaw.max - o.yaw.min);
        n.pitch = o.pitch.min + (row + 0.5f) / o.rows * (o.pitch.max - o.pitch.min);
    } else {
        // keep a little room from the others so two targets never overlap
        const float minSep = 3.0f * std::atan(o.radius / n.dist) * kDeg;
        for (int attempt = 0; attempt < 8; attempt++) {
            n.yaw = spawnRng_.In(o.yaw);
            n.pitch = spawnRng_.In(o.pitch);
            bool clear = true;
            for (const Target& other : targets_) {
                if (other.alive && std::hypot(AngleDelta(other.yaw, n.yaw), other.pitch - n.pitch) < minSep) {
                    clear = false;
                }
            }
            if (clear) {
                break;
            }
        }
    }

    n.nextChange = kNever;
    if (o.motion != MotionType::kStatic) {
        NewVelocity(n);
        n.nextChange = t + n.rng.In(o.changeEvery);
    }
    n.lastHitT = -1;
    n.yaw = std::clamp(n.yaw, o.yaw.min, o.yaw.max);
    n.pitch = std::clamp(n.pitch, o.pitch.min, o.pitch.max);
    n.stateT = t;
    *slot = n;
}

void TrainerSim::NewVelocity(Target& target) {
    const auto& o = scenario_.targets;
    const float speed = target.rng.In(o.speed) / target.dist * kDeg;  // world units/s -> degrees/s
    if (o.motion == MotionType::kStrafe) {
        target.vyaw = speed * target.rng.Sign();
        target.vpitch = 0;
    } else {
        const float a = target.rng.Unit() * 2 * kPi;
        target.vyaw = speed * std::cos(a);
        target.vpitch = speed * std::sin(a);
    }
}

void TrainerSim::MoveTo(Target& target, float t) {
    const auto& o = scenario_.targets;
    float& now = target.stateT;
    for (int guard = 0; guard < 1000 && now < t; guard++) {
        const float end = std::min(t, target.nextChange);
        // earliest wall of the spawn window hit before `end`
        float hit = end;
        int axis = -1;
        if (target.vyaw > 0 && now + (o.yaw.max - target.yaw) / target.vyaw < hit) {
            hit = now + (o.yaw.max - target.yaw) / target.vyaw;
            axis = 0;
        } else if (target.vyaw < 0 && now + (o.yaw.min - target.yaw) / target.vyaw < hit) {
            hit = now + (o.yaw.min - target.yaw) / target.vyaw;
            axis = 0;
        }
        if (target.vpitch > 0 && now + (o.pitch.max - target.pitch) / target.vpitch < hit) {
            hit = now + (o.pitch.max - target.pitch) / target.vpitch;
            axis = 1;
        } else if (target.vpitch < 0 && now + (o.pitch.min - target.pitch) / target.vpitch < hit) {
            hit = now + (o.pitch.min - target.pitch) / target.vpitch;
            axis = 1;
        }
        hit = std::max(hit, now);

        const float dt = hit - now;
        target.yaw = std::clamp(target.yaw + target.vyaw * dt, o.yaw.min, o.yaw.max);
        target.pitch = std::clamp(target.pitch + target.vpitch * dt, o.pitch.min, o.pitch.max);
        now = hit;

        if (axis == 0) {
            target.vyaw = -target.vyaw;
        } else if (axis == 1) {
            target.vpitch = -target.vpitch;
        } else if (now >= target.nextChange) {
            NewVelocity(target);
            target.nextChange = now + target.rng.In(o.changeEvery);
        }
    }
    now = std::max(now, t);
}

void TrainerSim::Advance(float t) {
    if (finished_) {
        return;
    }
    t = std::min(t, scenario_.duration);
    if (t < time_) {
        t = time_;
    }

    // timeouts, in time order (each respawn draws from the spawn RNG)
    const float lifetime = scenario_.targets.lifetime;
    while (lifetime > 0) {
        Target* first = nullptr;
        for (Target& target : targets_) {
            if (target.alive && target.spawnT + lifetime <= t &&
                (!first || target.spawnT < first->spawnT ||
                 (target.spawnT == first->spawnT && target.serial < first->serial))) {
                first = &target;
            }
        }
        if (!first) {
            break;
        }
        const float when = first->spawnT + lifetime;
        for (Target& target : targets_) {
            if (target.alive) {
                MoveTo(target, when);
            }
        }
        first->alive = false;
        expired_++;
        Spawn(when);
    }

    for (Target& target : targets_) {
        if (target.alive) {
            MoveTo(target, t);
        }
    }
    time_ = t;
    if (time_ >= scenario_.duration) {
        finished_ = true;
    }
}

void TrainerSim::Kill(Target& target, float t) {
    kills_++;
    const float ttk = (t - target.spawnT) * 1000.0f;
    ttkSum_ += ttk;
    killLog_.push_back({t, target.serial, ttk});
    target.alive = false;
    Spawn(t);
}

TrainerEvents TrainerSim::Input(float t, Vec3 eye, float yaw, float pitch, bool pressed, bool held, float dt) {
    TrainerEvents ev;
    if (finished_) {
        return ev;
    }
    Advance(t);
    if (finished_ && t > scenario_.duration) {
        return ev;
    }

    const Vec3 dir = AnglesToDir(yaw, pitch);
    const float r = scenario_.targets.radius;

    // ray vs spheres: nearest hit along the ray, and the target nearest the aim
    Target* hitTarget = nullptr;
    float hitDist = kNever;
    Target* nearest = nullptr;
    float nearestAngle = kNever;
    for (Target& target : targets_) {
        if (!target.alive) {
            continue;
        }
        const Vec3 d = Sub(TargetPosition(target), eye);
        const float along = Dot(d, dir);
        const float len = std::sqrt(Dot(d, d));
        const float angle = len > 0 ? std::acos(std::clamp(along / len, -1.0f, 1.0f)) : 0;
        if (angle < nearestAngle) {
            nearestAngle = angle;
            nearest = &target;
        }
        if (along > 0 && Dot(d, d) - along * along <= r * r && along < hitDist) {
            hitDist = along;
            hitTarget = &target;
        }
    }

    auto logShot = [&](bool hit) {
        ShotEvent s;
        s.t = t;
        s.hit = hit;
        if (nearest) {
            const Vec3 d = Sub(TargetPosition(*nearest), eye);
            const float tyaw = std::atan2(d.y, d.x) * kDeg;
            const float tpitch = std::atan2(d.z, std::hypot(d.x, d.y)) * kDeg;
            s.target = nearest->serial;
            s.dx = -AngleDelta(yaw, tyaw) * std::cos(tpitch / kDeg);  // + = aim right of the target
            s.dy = pitch - tpitch;                                     // + = aim above it
            s.radius = std::atan(r / std::sqrt(Dot(d, d))) * kDeg;
        }
        shotLog_.push_back(s);
    };

    if (scenario_.fire == FireMode::kClick) {
        if (pressed) {
            shots_++;
            logShot(hitTarget != nullptr);
            if (hitTarget) {
                hits_++;
                ev.hits++;
                hitTarget->lastHitT = t;
                hitTarget->health -= 1;
                if (hitTarget->health <= 0) {
                    ev.kills++;
                    Kill(*hitTarget, t);
                }
            } else {
                ev.misses++;
            }
        }
    } else if (held) {
        dt = std::clamp(dt, 0.0f, 0.05f);
        firing_ += dt;
        if (t >= nextTrackSample_) {
            shots_++;
            hits_ += hitTarget ? 1 : 0;
            logShot(hitTarget != nullptr);
            nextTrackSample_ = t + kTrackSampleInterval;
        }
        if (hitTarget) {
            ev.hits++;
            onTarget_ += dt;
            hitTarget->lastHitT = t;
            hitTarget->health -= dt;
            if (hitTarget->health <= 0) {
                ev.kills++;
                Kill(*hitTarget, t);
            }
        }
    }
    return ev;
}

float TrainerSim::Accuracy() const {
    if (scenario_.fire == FireMode::kHold) {
        return firing_ > 0 ? onTarget_ / firing_ : 0;
    }
    return shots_ > 0 ? static_cast<float>(hits_) / shots_ : 0;
}

float TrainerSim::MeanTtk() const { return kills_ > 0 ? static_cast<float>(ttkSum_ / kills_) : 0; }

float TrainerSim::Score() const {
    const auto& s = scenario_.score;
    if (scenario_.fire == FireMode::kHold) {
        return onTarget_ * s.perSecondOnTarget + kills_ * s.kill;
    }
    return kills_ * s.kill * ((1 - s.accuracyWeight) + s.accuracyWeight * Accuracy());
}

RunRecord TrainerSim::Record() const {
    RunRecord r;
    RunSummary& s = r.summary;
    s.scenario = scenario_.id;
    s.name = scenario_.name;
    s.category = scenario_.category;
    s.duration = std::min(time_, scenario_.duration);
    s.score = Score();
    s.shots = shots_;
    s.hits = hits_;
    s.kills = kills_;
    s.accuracy = Accuracy();
    s.ttk = MeanTtk();
    s.onTarget = onTarget_;
    r.seed = seed_;
    r.shots = shotLog_;
    r.kills = killLog_;
    return r;
}

}  // namespace skaar
