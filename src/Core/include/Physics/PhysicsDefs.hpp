#ifndef ETTYCC_PHYSICS_DEFS_HPP
#define ETTYCC_PHYSICS_DEFS_HPP

#include <Math/Utils.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace ettycc::physics
{
    enum class ShapeType
    {
        Box,
        Sphere,
        Circle,
        Capsule,
        TriMesh,
        ConvexPolygon   // 2D convex polygon from sprite vertices
    };

    // Downsample a convex polygon to at most maxVerts by keeping the most
    // angularly-spaced vertices.  Preserves convexity and overall shape.
    inline std::vector<glm::vec2> DownsampleConvexPoly(const std::vector<glm::vec2>& verts, int maxVerts)
    {
        if (static_cast<int>(verts.size()) <= maxVerts || maxVerts < 3)
            return verts;

        // Compute centroid
        glm::vec2 centroid(0.f);
        for (auto& v : verts) centroid += v;
        centroid /= static_cast<float>(verts.size());

        // Sort by angle around centroid
        struct AngleVert { float angle; glm::vec2 pos; };
        std::vector<AngleVert> sorted;
        sorted.reserve(verts.size());
        for (auto& v : verts)
        {
            float a = math::AngleSigned(v - centroid);
            sorted.push_back({a, v});
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const AngleVert& a, const AngleVert& b) { return a.angle < b.angle; });

        // Uniformly sample by angle
        const int n = static_cast<int>(sorted.size());
        const float step = static_cast<float>(n) / static_cast<float>(maxVerts);
        std::vector<glm::vec2> result;
        result.reserve(maxVerts);
        for (int i = 0; i < maxVerts; ++i)
        {
            int idx = static_cast<int>(i * step) % n;
            result.push_back(sorted[idx].pos);
        }
        return result;
    }

    struct ShapeDef
    {
        ShapeType type       = ShapeType::Box;
        glm::vec3 halfExtents = {0.5f, 0.5f, 0.5f};
        float     radius      = 0.5f;
        float     height      = 1.0f;

        // For TriMesh shapes
        std::vector<float> vertices;
        std::vector<int>   indices;

        // For ConvexPolygon: 2D vertices in local space (pre-scaled).
        // Automatically downsampled to kMaxCollisionVerts for physics.
        static constexpr int kMaxCollisionVerts = 8;
        std::vector<glm::vec2> polyVertices;

        static ShapeDef Box(const glm::vec3& half)
        {
            ShapeDef s; s.type = ShapeType::Box; s.halfExtents = half; return s;
        }
        static ShapeDef Sphere(float r)
        {
            ShapeDef s; s.type = ShapeType::Sphere; s.radius = r; return s;
        }
        static ShapeDef Circle(float r)
        {
            ShapeDef s; s.type = ShapeType::Circle; s.radius = r; return s;
        }
        // Create a convex polygon shape from boundary vertices.
        // verts: local-space 2D positions, scale: world half-extents applied to local coords.
        // Automatically downsamples to kMaxCollisionVerts for physics engines.
        static ShapeDef ConvexPoly(const std::vector<glm::vec2>& verts, glm::vec2 scale)
        {
            ShapeDef s;
            s.type = ShapeType::ConvexPolygon;

            // Downsample BEFORE scaling to avoid expensive compound colliders
            auto downsampled = DownsampleConvexPoly(verts, kMaxCollisionVerts);

            s.polyVertices.reserve(downsampled.size());
            for (auto& v : downsampled)
                s.polyVertices.push_back(v * scale);
            return s;
        }
    };

    struct RigidBodyDef
    {
        float     mass          = 1.0f;
        ShapeDef  shape;
        glm::vec3 position      = {0.f, 0.f, 0.f};
        glm::quat rotation      = glm::quat(1.f, 0.f, 0.f, 0.f);
        glm::vec3 linearFactor   = {1.f, 1.f, 0.f};
        glm::vec3 angularFactor  = {0.f, 0.f, 1.f};
    };

    struct SoftBodyDef
    {
        std::vector<float> vertices;
        std::vector<int>   indices;
        float     mass       = 1.0f;
        float     stiffness  = 0.05f;
        float     pressure   = 0.0f;
        float     damping    = 0.3f;
        float     friction   = 0.8f;
        glm::vec3 position   = {0.f, 0.f, 0.f};
        int       bendingDist = 2;
    };

} // namespace ettycc::physics

#endif
