// Small vector / matrix / quaternion library. Matrices use the row-vector convention (v * M),
// the same as System.Numerics, so transform order reads left to right.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cw {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg = kPi / 180.f;

struct Vec2 { float x = 0, y = 0; };

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit constexpr Vec3(float s) : x(s), y(s), z(s) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline Vec3 normalize(const Vec3& v) { float l = v.length(); return l > 1e-12f ? v / l : Vec3(0, 0, 0); }
inline Vec3 vmin(const Vec3& a, const Vec3& b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline Vec3 vmax(const Vec3& a, const Vec3& b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline float distance(const Vec3& a, const Vec3& b) { return (a - b).length(); }
inline Vec3 reflect(const Vec3& v, const Vec3& n) { return v - n * (2.f * dot(v, n)); }

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    bool operator==(const Vec4& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
};

/// Unity-compatible quaternion (x, y, z, w), Hamilton product: (a * b) applies b first.
struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
    static Quat axisAngle(const Vec3& axis, float rad) {
        Vec3 a = normalize(axis);
        float s = std::sin(rad * .5f);
        return {a.x * s, a.y * s, a.z * s, std::cos(rad * .5f)};
    }
    Quat operator*(const Quat& b) const {
        return {w * b.x + x * b.w + y * b.z - z * b.y,
                w * b.y - x * b.z + y * b.w + z * b.x,
                w * b.z + x * b.y - y * b.x + z * b.w,
                w * b.w - x * b.x - y * b.y - z * b.z};
    }
    Vec3 rotate(const Vec3& v) const {
        Vec3 u{x, y, z};
        Vec3 t = cross(u, v) * 2.f;
        return v + t * w + cross(u, t);
    }
};

struct Mat4 {
    float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

    static Mat4 identity() { return {}; }
    static Mat4 translation(const Vec3& t) { Mat4 r; r.m[3][0] = t.x; r.m[3][1] = t.y; r.m[3][2] = t.z; return r; }
    static Mat4 scale(const Vec3& s) { Mat4 r; r.m[0][0] = s.x; r.m[1][1] = s.y; r.m[2][2] = s.z; return r; }
    static Mat4 scale(float s) { return scale(Vec3(s)); }
    static Mat4 rotX(float a) { Mat4 r; float c = std::cos(a), s = std::sin(a); r.m[1][1] = c; r.m[1][2] = s; r.m[2][1] = -s; r.m[2][2] = c; return r; }
    static Mat4 rotY(float a) { Mat4 r; float c = std::cos(a), s = std::sin(a); r.m[0][0] = c; r.m[0][2] = -s; r.m[2][0] = s; r.m[2][2] = c; return r; }
    static Mat4 rotZ(float a) { Mat4 r; float c = std::cos(a), s = std::sin(a); r.m[0][0] = c; r.m[0][1] = s; r.m[1][0] = -s; r.m[1][1] = c; return r; }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += m[i][k] * b.m[k][j];
                r.m[i][j] = s;
            }
        return r;
    }
    Vec3 point(const Vec3& v) const {
        return {v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0],
                v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1],
                v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2]};
    }
    Vec3 vector(const Vec3& v) const {
        return {v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0],
                v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1],
                v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2]};
    }
    Mat4 transposed() const { Mat4 r; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) r.m[i][j] = m[j][i]; return r; }

    /// General inverse (Gauss-Jordan). Returns identity if singular.
    Mat4 inverted() const {
        double a[4][8];
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 8; j++) a[i][j] = j < 4 ? m[i][j] : (j - 4 == i ? 1.0 : 0.0);
        for (int c = 0; c < 4; c++) {
            int p = c;
            for (int r = c + 1; r < 4; r++) if (std::fabs(a[r][c]) > std::fabs(a[p][c])) p = r;
            if (std::fabs(a[p][c]) < 1e-12) return {};
            if (p != c) for (int j = 0; j < 8; j++) std::swap(a[p][j], a[c][j]);
            double d = a[c][c];
            for (int j = 0; j < 8; j++) a[c][j] /= d;
            for (int r = 0; r < 4; r++) {
                if (r == c) continue;
                double f = a[r][c];
                if (f != 0) for (int j = 0; j < 8; j++) a[r][j] -= f * a[c][j];
            }
        }
        Mat4 r;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) r.m[i][j] = (float)a[i][j + 4];
        return r;
    }
};

} // namespace cw
