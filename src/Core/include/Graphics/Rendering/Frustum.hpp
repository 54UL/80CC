#pragma once

#include <glm/glm.hpp>
#include <array>

namespace ettycc
{
    // Six-plane frustum extracted from a Projection * View matrix.
    // Planes point inward — a point is inside when dot(plane, point) >= 0.
    struct Frustum
    {
        enum Plane { Left = 0, Right, Bottom, Top, Near, Far, Count };

        std::array<glm::vec4, Count> planes {};
        bool enabled = false;

        // Extract frustum planes from a combined PV matrix.
        // Each plane is (a, b, c, d) where ax + by + cz + d >= 0 means inside.
        static Frustum FromPV(const glm::mat4& pv)
        {
            Frustum f;
            f.enabled = true;

            // Gribb-Hartmann method — extract planes from rows of the PV matrix.
            const glm::vec4 r0(pv[0][0], pv[1][0], pv[2][0], pv[3][0]);
            const glm::vec4 r1(pv[0][1], pv[1][1], pv[2][1], pv[3][1]);
            const glm::vec4 r2(pv[0][2], pv[1][2], pv[2][2], pv[3][2]);
            const glm::vec4 r3(pv[0][3], pv[1][3], pv[2][3], pv[3][3]);

            f.planes[Left]   = r3 + r0;
            f.planes[Right]  = r3 - r0;
            f.planes[Bottom] = r3 + r1;
            f.planes[Top]    = r3 - r1;
            f.planes[Near]   = r3 + r2;
            f.planes[Far]    = r3 - r2;

            // Normalize each plane so distance tests are in world units.
            for (auto& p : f.planes)
            {
                float len = glm::length(glm::vec3(p));
                if (len > 0.f) p /= len;
            }

            return f;
        }

        // Test an axis-aligned bounding box (center + half-extents) against the frustum.
        // Returns true if the AABB is at least partially inside.
        bool IsVisible(const glm::vec3& center, const glm::vec3& halfExtents) const
        {
            if (!enabled) return true;

            for (const auto& plane : planes)
            {
                const glm::vec3 n(plane);
                // Effective radius: project half-extents onto the plane normal
                float r = halfExtents.x * std::abs(n.x)
                        + halfExtents.y * std::abs(n.y)
                        + halfExtents.z * std::abs(n.z);

                // Signed distance from center to plane
                float d = glm::dot(n, center) + plane.w;

                // If the farthest point is behind the plane, fully outside
                if (d + r < 0.f)
                    return false;
            }
            return true;
        }

        // Convenience: test a 2D sprite with position and scale (z half-extent = small).
        bool IsVisible(const glm::vec3& position, float scaleX, float scaleY) const
        {
            return IsVisible(position, glm::vec3(std::abs(scaleX), std::abs(scaleY), 0.5f));
        }
    };

} // namespace ettycc
