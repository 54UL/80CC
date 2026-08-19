#ifndef ETTYCC_BULLET_RIGID_BODY_HPP
#define ETTYCC_BULLET_RIGID_BODY_HPP

#include <Physics/IPhysicsBody.hpp>
#include <btBulletDynamicsCommon.h>
#include <memory>

namespace ettycc::physics
{
    class BulletRigidBody : public IPhysicsBody
    {
    public:
        BulletRigidBody(std::unique_ptr<btCollisionShape> shape,
                        std::unique_ptr<btDefaultMotionState> motionState,
                        std::unique_ptr<btRigidBody> body,
                        btDiscreteDynamicsWorld* world);
        ~BulletRigidBody() override;

        // IPhysicsBody
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
        std::unique_ptr<btCollisionShape>     shape_;
        std::unique_ptr<btDefaultMotionState> motionState_;
        std::unique_ptr<btRigidBody>          body_;
        btDiscreteDynamicsWorld*              world_ = nullptr;
    };

} // namespace ettycc::physics

#endif
