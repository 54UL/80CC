#pragma once

// =============================================================================
// Math constants -- single source of truth for all mathematical literals.
//
// Use these instead of M_PI, 3.14159..., or any inline constant.
// All values are constexpr float for consistent precision and SIMD-friendly
// codegen (no double→float truncation warnings, no runtime conversion).
// =============================================================================

namespace ettycc::math
{
    constexpr float kPi       = 3.14159265358979323846f;
    constexpr float kTwoPi    = 2.f * kPi;                // τ (full circle)
    constexpr float kHalfPi   = kPi * 0.5f;               // π/2 (quarter turn)
    constexpr float kInvPi    = 1.f / kPi;
    constexpr float kInvTwoPi = 1.f / kTwoPi;             // 1/τ (angle → [0,1])

    constexpr float kDegToRad = kPi / 180.f;
    constexpr float kRadToDeg = 180.f / kPi;

    // Floating-point tolerance thresholds.
    // Use kEpsilon for general near-zero checks.
    // Use kEpsilonSq for squared-distance comparisons (avoids sqrt).
    constexpr float kEpsilon   = 1e-6f;
    constexpr float kEpsilonSq = kEpsilon * kEpsilon;

    // Common geometric constants.
    constexpr float kSqrt2    = 1.41421356237309504880f;
    constexpr float kInvSqrt2 = 0.70710678118654752440f;

} // namespace ettycc::math
