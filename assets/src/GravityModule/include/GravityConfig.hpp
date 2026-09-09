#pragma once
#include <glm/glm.hpp>

namespace gravity
{
    // Collision outcome classification based on specific impact energy Q*.
    //   Q = 0.5 * m_reduced * v_rel^2 / M_total
    enum class CollisionOutcome { Fusion, Fracture, Shatter };

    struct GravitySceneConfig
    {
        int   boxCount          = 2000;
        float orbitRadius       = 5.0f;
        float attractorStrength = 60.0f;
        float boxSize           = 0.08f;
        float innerRadius       = 0.8f;
        float outerRadius       = 30.0f;
        float fusionOverlap     = 1.5f;
        float fusionCooldown    = 0.2f;

        // N-body (body-to-body gravitational attraction)
        bool  nBodyEnabled      = true;
        float nBodyG            = 0.02f;
        float nBodySoftening    = 0.15f;

        // Collision energy thresholds (specific energy Q*)
        float qFusion           = 5.0f;
        float qShatter          = 50.0f;

        // Rock pool
        int   rockPoolSize      = 64;
        float rockRoughness     = 0.3f;
        int   shatterFragments  = 6;
    };

} // namespace gravity
