#ifndef ETTYCC_PHYSICS_DEFS_HPP
#define ETTYCC_PHYSICS_DEFS_HPP

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>

namespace ettycc::physics
{
    enum class ShapeType
    {
        Box,
        Sphere,
        Circle,
        Capsule,
        TriMesh
    };

    struct ShapeDef
    {
        ShapeType type       = ShapeType::Box;
        glm::vec3 halfExtents = {0.5f, 0.5f, 0.5f};
        float     radius      = 0.5f;
        float     height      = 1.0f;

        // For TriMesh shapes
        std::vector<float> vertices;
        std::vector<int>   indices;

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
