#pragma once

#include <Math/Constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

// =============================================================================
// Math utilities -- common mathematical operations centralized for consistency,
// performance, and SIMD-friendly codegen via GLM.
//
// All functions are inline constexpr where possible.
// Prefer these over raw std:: calls to ensure uniform precision and enable
// compiler vectorization hints.
// =============================================================================

namespace ettycc::math
{
    // =========================================================================
    // Scalar utilities
    // =========================================================================

    // Safe division: returns fallback when denominator is near-zero.
    inline float SafeDiv(float num, float den, float fallback = 0.f)
    {
        return glm::abs(den) > kEpsilon ? num / den : fallback;
    }

    // Remap value from [inMin, inMax] to [outMin, outMax].
    inline float Remap(float value, float inMin, float inMax,
                       float outMin, float outMax)
    {
        float t = SafeDiv(value - inMin, inMax - inMin);
        return outMin + t * (outMax - outMin);
    }

    // Linear interpolation. Equivalent to glm::mix for scalars.
    inline constexpr float Lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    // Snap value to nearest grid step.
    inline float SnapToGrid(float val, float grid)
    {
        return glm::round(val / grid) * grid;
    }

    // =========================================================================
    // Angle utilities (all in radians)
    // =========================================================================

    // Wrap angle to [0, 2π).
    inline float WrapAngle(float radians)
    {
        radians = std::fmod(radians, kTwoPi);
        return radians < 0.f ? radians + kTwoPi : radians;
    }

    // Wrap angle to [-π, π).
    inline float WrapAngleSigned(float radians)
    {
        radians = std::fmod(radians + kPi, kTwoPi);
        return radians < 0.f ? radians + kPi : radians - kPi;
    }

    // Angle of 2D vector (atan2). Returns [0, 2π).
    inline float Angle(glm::vec2 v)
    {
        return WrapAngle(glm::atan(v.y, v.x));
    }

    // Angle of 2D vector (atan2). Returns [-π, π).
    inline float AngleSigned(glm::vec2 v)
    {
        return glm::atan(v.y, v.x);
    }

    // Angle from point `from` to point `to`. Returns [0, 2π).
    inline float AngleBetween(glm::vec2 from, glm::vec2 to)
    {
        return Angle(to - from);
    }

    // Convert angle to normalized [0, 1) range (for radial UV encoding).
    inline float AngleToNormalized(float radians)
    {
        return WrapAngle(radians) * kInvTwoPi;
    }

    // =========================================================================
    // Quaternion ↔ 2D angle (Z-axis rotation)
    // =========================================================================

    // Extract Z-rotation angle from quaternion (for 2D physics).
    inline float QuatToAngle2D(const glm::quat& q)
    {
        return 2.f * glm::atan(q.z, q.w);
    }

    // Create quaternion from Z-rotation angle (for 2D physics).
    inline glm::quat AngleToQuat2D(float radians)
    {
        return glm::angleAxis(radians, glm::vec3(0.f, 0.f, 1.f));
    }

    // =========================================================================
    // 2D geometry
    // =========================================================================

    // 2D cross product (signed area of parallelogram OAB).
    inline float Cross2D(glm::vec2 a, glm::vec2 b)
    {
        return a.x * b.y - a.y * b.x;
    }

    // 2D cross product relative to origin point O.
    inline float Cross2D(glm::vec2 O, glm::vec2 A, glm::vec2 B)
    {
        return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
    }

    // Signed area of a triangle (positive = CCW, negative = CW).
    inline float TriangleAreaSigned(glm::vec2 a, glm::vec2 b, glm::vec2 c)
    {
        return 0.5f * ((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
    }

    // Unsigned area of a triangle.
    inline float TriangleArea(glm::vec2 a, glm::vec2 b, glm::vec2 c)
    {
        return glm::abs(TriangleAreaSigned(a, b, c));
    }

    // Signed polygon area via shoelace formula.
    // Positive for CCW winding, negative for CW.
    inline float PolygonAreaSigned(const glm::vec2* verts, size_t count)
    {
        float area = 0.f;
        for (size_t i = 0; i < count; ++i)
        {
            size_t j = (i + 1) % count;
            area += verts[i].x * verts[j].y;
            area -= verts[j].x * verts[i].y;
        }
        return area * 0.5f;
    }

    // Unsigned polygon area.
    inline float PolygonArea(const glm::vec2* verts, size_t count)
    {
        return glm::abs(PolygonAreaSigned(verts, count));
    }

    // Centroid of a polygon (uniform density).
    inline glm::vec2 PolygonCentroid(const glm::vec2* verts, size_t count)
    {
        glm::vec2 sum(0.f);
        for (size_t i = 0; i < count; ++i) sum += verts[i];
        return count > 0 ? sum / static_cast<float>(count) : glm::vec2(0.f);
    }

    // =========================================================================
    // Distance helpers (prefer glm::length over manual sqrt(x²+y²))
    // =========================================================================

    // Squared distance between two 2D points (avoids sqrt).
    inline float DistanceSq(glm::vec2 a, glm::vec2 b)
    {
        glm::vec2 d = a - b;
        return glm::dot(d, d);
    }

    // Distance between two 2D points.
    inline float Distance(glm::vec2 a, glm::vec2 b)
    {
        return glm::length(a - b);
    }

    // Squared distance between two 3D points (avoids sqrt).
    inline float DistanceSq(glm::vec3 a, glm::vec3 b)
    {
        glm::vec3 d = a - b;
        return glm::dot(d, d);
    }

    // Distance between two 3D points.
    inline float Distance(glm::vec3 a, glm::vec3 b)
    {
        return glm::length(a - b);
    }

    // =========================================================================
    // Safe normalize (returns fallback when vector is near-zero)
    // =========================================================================

    inline glm::vec2 SafeNormalize(glm::vec2 v, glm::vec2 fallback = {1.f, 0.f})
    {
        float len = glm::length(v);
        return len > kEpsilon ? v / len : fallback;
    }

    inline glm::vec3 SafeNormalize(glm::vec3 v, glm::vec3 fallback = {1.f, 0.f, 0.f})
    {
        float len = glm::length(v);
        return len > kEpsilon ? v / len : fallback;
    }

    // =========================================================================
    // Scale / mass helpers (used in physics collision processing)
    // =========================================================================

    // Safe scale component: returns fallback when near-zero (prevents div-by-zero).
    inline float SafeScale(float s, float fallback = 1.f)
    {
        return glm::abs(s) > kEpsilon ? s : fallback;
    }

    // Cube root (for volume→radius or mass→scale conversions).
    inline float CubeRoot(float x)
    {
        return std::cbrt(x);
    }

    // Scale factor from mass ratio (preserving volume: scale ∝ ∛(mass_ratio)).
    inline float MassScaleFactor(float newMass, float oldMass)
    {
        return (oldMass > kEpsilon) ? std::cbrt(newMass / oldMass) : 1.f;
    }

    // =========================================================================
    // Comparison helpers
    // =========================================================================

    // Near-equal for floats.
    inline bool NearEqual(float a, float b, float eps = kEpsilon)
    {
        return glm::abs(a - b) < eps;
    }

    // Near-equal for 2D vectors.
    inline bool NearEqual(glm::vec2 a, glm::vec2 b, float eps = kEpsilon)
    {
        return glm::abs(a.x - b.x) < eps && glm::abs(a.y - b.y) < eps;
    }

    // Near-equal for 3D vectors.
    inline bool NearEqual(glm::vec3 a, glm::vec3 b, float eps = kEpsilon)
    {
        return glm::abs(a.x - b.x) < eps
            && glm::abs(a.y - b.y) < eps
            && glm::abs(a.z - b.z) < eps;
    }

    // Near-zero for float.
    inline bool NearZero(float x, float eps = kEpsilon)
    {
        return glm::abs(x) < eps;
    }

} // namespace ettycc::math
