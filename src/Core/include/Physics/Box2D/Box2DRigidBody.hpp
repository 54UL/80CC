#ifndef ETTYCC_BOX2D_RIGID_BODY_HPP
#define ETTYCC_BOX2D_RIGID_BODY_HPP

#include <Physics/IPhysicsBody.hpp>

class b2Body;
class b2World;

namespace ettycc::physics
{
    class Box2DRigidBody : public IPhysicsBody
    {
    public:
        Box2DRigidBody(b2World* world, b2Body* body, float mass);
        ~Box2DRigidBody() override;

        glm::vec3 GetPosition()       const override;
        glm::quat GetRotation()       const override;
        glm::vec3 GetLinearVelocity()  const override;
        float     GetMass()            const override;

        void SetLinearVelocity(const glm::vec3& v) override;
        void ApplyCentralForce(const glm::vec3& f) override;
        void SetTransform(const glm::vec3& pos, const glm::quat& rot) override;
        void Activate() override;

        void SetKinematic(bool kinematic) override;
        bool IsKinematic() const override;

        void SetLinearFactor(const glm::vec3& f)  override;
        void SetAngularFactor(const glm::vec3& f) override;
        void SetAngularVelocity(const glm::vec3& v) override;

    private:
        b2World* world_ = nullptr;
        b2Body*  body_  = nullptr;
        float    mass_  = 0.f;
    };

} // namespace ettycc::physics

#endif
