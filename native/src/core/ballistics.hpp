// Engine-independent projectile maths (unit tested).
#pragma once
#include <random>
#include "math.hpp"

namespace cw::ballistics {

constexpr float kGravity = 9.81f;

/// Advances velocity by gravity and returns this step's displacement (exact for constant acceleration).
inline Vec3 step(Vec3& vel, float drop, float dt) {
    Vec3 v0 = vel;
    vel.y -= kGravity * drop * dt;
    return (v0 + vel) * (0.5f * dt);
}

inline Vec3 bounce(const Vec3& v, Vec3 n, float bounciness) {
    if (n.lengthSq() < 1e-8f) return -v * bounciness;
    return reflect(v, normalize(n)) * bounciness;
}

/// Random direction inside a cone of spreadDeg degrees around forward.
inline Vec3 spread(Vec3 fwd, float spreadDeg, std::mt19937& rng) {
    fwd = normalize(fwd);
    if (spreadDeg <= 0.0001f) return fwd;
    std::uniform_real_distribution<float> u(0.f, 1.f);
    Vec3 helper = std::fabs(fwd.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 right = normalize(cross(helper, fwd));
    Vec3 up = cross(fwd, right);
    float cosMax = std::cos(spreadDeg * kDeg);
    float cosT = 1.f - u(rng) * (1.f - cosMax);
    float sinT = std::sqrt(std::max(0.f, 1.f - cosT * cosT));
    float phi = u(rng) * 2.f * kPi;
    return normalize(fwd * cosT + (right * std::cos(phi) + up * std::sin(phi)) * sinT);
}

inline float areaDamage(float damage, float radius, float dist, bool falloff) {
    if (radius <= 0 || dist > radius) return 0;
    if (!falloff) return damage;
    float t = 1.f - dist / radius;
    return damage * (0.25f + 0.75f * t);
}

/// Unity's Quaternion.Euler: Z, then X, then Y (radians).
inline Quat eulerYXZ(float x, float y, float z) {
    return Quat::axisAngle({0, 1, 0}, y) * Quat::axisAngle({1, 0, 0}, x) * Quat::axisAngle({0, 0, 1}, z);
}
inline Quat eulerDeg(const Vec3& d) { return eulerYXZ(d.x * kDeg, d.y * kDeg, d.z * kDeg); }

/// Rotation that points +Z along dir.
inline Quat lookRotation(Vec3 dir) {
    if (dir.lengthSq() < 1e-8f) return {};
    dir = normalize(dir);
    return eulerYXZ(-std::asin(std::clamp(dir.y, -1.f, 1.f)), std::atan2(dir.x, dir.z), 0);
}

} // namespace cw::ballistics
