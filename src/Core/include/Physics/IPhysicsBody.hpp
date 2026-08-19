#ifndef ETTYCC_IPHYSICS_BODY_HPP
#define ETTYCC_IPHYSICS_BODY_HPP

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace ettycc::physics
{
    class IPhysicsBody
    {
    public:
        virtual ~IPhysicsBody() = default;

        virtual glm::vec3 GetPosition()       const = 0;
        virtual glm::quat GetRotation()       const = 0;
        virtual glm::vec3 GetLinearVelocity()  const = 0;
        virtual float     GetMass()            const = 0;

        virtual void SetLinearVelocity(const glm::vec3& v) = 0;
        virtual void ApplyCentralForce(const glm::vec3& f) = 0;
        virtual void SetTransform(const glm::vec3& pos, const glm::quat& rot) = 0;
        virtual void Activate() = 0;

        virtual void SetKinematic(bool kinematic) = 0;
        virtual bool IsKinematic() const = 0;

        virtual void SetLinearFactor(const glm::vec3& f)  = 0;
        virtual void SetAngularFactor(const glm::vec3& f) = 0;

        virtual void SetAngularVelocity(const glm::vec3& v) = 0;
    };

} // namespace ettycc::physics

#endif
