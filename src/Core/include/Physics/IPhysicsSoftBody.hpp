#ifndef ETTYCC_IPHYSICS_SOFT_BODY_HPP
#define ETTYCC_IPHYSICS_SOFT_BODY_HPP

#include <glm/glm.hpp>

namespace ettycc::physics
{
    class IPhysicsSoftBody
    {
    public:
        virtual ~IPhysicsSoftBody() = default;

        virtual int       GetNodeCount() const = 0;
        virtual glm::vec3 GetNodePosition(int index) const = 0;
        virtual glm::vec3 GetCentroid() const = 0;

        virtual void Translate(const glm::vec3& delta) = 0;
        virtual void ZeroVelocities() = 0;

        // Constrain nodes to a 2D plane at the given Z with small epsilon spread.
        virtual void ConstrainToPlane(float z, float epsilon) = 0;

        // Face data for rendering / wireframe overlays.
        virtual int  GetFaceCount() const = 0;
        virtual void GetFaceNodePositions(int faceIdx,
                                          glm::vec3& a,
                                          glm::vec3& b,
                                          glm::vec3& c) const = 0;

        virtual void UpdateBounds() = 0;
    };

} // namespace ettycc::physics

#endif
