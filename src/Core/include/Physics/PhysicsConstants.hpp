#ifndef ETTYCC_PHYSICS_CONSTANTS_HPP
#define ETTYCC_PHYSICS_CONSTANTS_HPP

namespace ettycc::physics
{
    // =========================================================================
    // Tuning constants for all physics backends.
    // Centralised here so they can be adjusted in one place.
    // =========================================================================

    // -- Coordinate system ----------------------------------------------------
    // Both Bullet and Box2D use Y-up, matching OpenGL.
    // No axis swap is needed.  If a future backend uses a different convention,
    // add a coordinate transform in that backend's implementation.

    // -- World-to-physics scale -----------------------------------------------
    // The engine uses arbitrary "world units" with gravity ~9.81 (meter-like).
    // Box2D works best with objects sized 0.1–10 m.  Both backends now use
    // the same 1:1 mapping so that collider sizes match exactly.
    //
    // IMPORTANT: do NOT set kBox2DScale much below 1.0 — Box2D has an internal
    // polygon skin (b2_polygonRadius ≈ 0.01 m) that becomes proportionally
    // huge when shapes are tiny, causing invisible collision gaps.
    constexpr float kBulletScale  = 1.0f;
    constexpr float kBox2DScale   = 1.0f;

    // -- Timestep -------------------------------------------------------------
    constexpr float kMaxDeltaTime     = 0.25f;       // clamp to avoid spiral of death
    constexpr float kFallbackDeltaTime = 1.f / 60.f;  // used when dt <= 0 or dt > max
    constexpr int   kBulletSubSteps    = 8;
    constexpr int   kBox2DSubSteps     = 4;

    // -- Shape limits ---------------------------------------------------------
    constexpr float kMinHalfExtent = 0.05f;   // prevents degenerate shapes
    constexpr float kMinRadius     = 0.05f;
    constexpr float kMinHeight     = 0.05f;

    // -- Gravity --------------------------------------------------------------
    constexpr float kDefaultGravityX = 0.0f;
    constexpr float kDefaultGravityY = -9.81f;
    constexpr float kDefaultGravityZ = 0.0f;

    // -- Soft body (Bullet-specific for now) ----------------------------------
    constexpr float kSoftBodyASTMultiplier    = 2.0f;   // angular stiffness = linear * this
    constexpr float kSoftBodyVST              = 0.0f;   // volume stiffness
    constexpr int   kSoftBodySolverIterations = 10;
    constexpr float kSoftBodyCollisionMargin  = 0.02f;
    constexpr float kSoftBodyCoplanarEpsilon  = 0.001f; // Z jitter to break coplanarity
    constexpr float kSoftBodyPlaneEpsilon     = 0.001f; // 2D plane constraint tolerance

    // -- Bullet world info ----------------------------------------------------
    constexpr float kBulletAirDensity   = 1.2f;
    constexpr float kBulletWaterDensity = 0.0f;
    constexpr float kBulletWaterOffset  = 0.0f;

    // -- Box2D density helpers ------------------------------------------------
    constexpr float kDefaultDensity = 1.0f;   // fallback when mass/area is invalid
    constexpr float kPi             = 3.14159265f;

    // -- 2D constraints (applied by RigidBodyComponent) -----------------------
    // These are passed as linearFactor / angularFactor to the physics backend.
    // Default: move in X,Y only; rotate around Z only.
    constexpr float kLinearFactorX  = 1.f;
    constexpr float kLinearFactorY  = 1.f;
    constexpr float kLinearFactorZ  = 0.f;
    constexpr float kAngularFactorX = 0.f;
    constexpr float kAngularFactorY = 0.f;
    constexpr float kAngularFactorZ = 1.f;

} // namespace ettycc::physics

#endif
