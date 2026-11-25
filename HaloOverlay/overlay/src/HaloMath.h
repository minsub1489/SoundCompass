#pragma once

#include <algorithm>
#include <cmath>

namespace halo
{
constexpr double kPi = 3.14159265358979323846;

struct Vec2
{
    double x = 0.0, y = 0.0;
};

struct Vec3
{
    double x = 0.0, y = 0.0, z = 0.0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator-(Vec2 a) { return {-a.x, -a.y}; }
inline Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.y * s}; }
inline Vec2& operator+=(Vec2& a, Vec2 b) { a = a + b; return a; }
inline Vec2& operator-=(Vec2& a, Vec2 b) { a = a - b; return a; }
inline double Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline double Length(Vec2 a) { return std::sqrt(Dot(a, a)); }

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline Vec3 Normalized(Vec3 a)
{
    const double length = Length(a);
    return length > 1e-12 ? a * (1.0 / length) : Vec3{};
}

inline double DegToRad(double degrees) { return degrees * kPi / 180.0; }

/** Signed difference a - b wrapped to [-180, 180). */
inline double AngleDiff(double a, double b)
{
    double d = std::fmod(a - b + 180.0, 360.0);
    if (d < 0.0) { d += 360.0; }
    return d - 180.0;
}
}
