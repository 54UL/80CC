#include <Physics/Bullet/BulletRigidBody.hpp>

namespace ettycc::physics
{
    BulletRigidBody::BulletRigidBody(std::unique_ptr<btCollisionShape> shape,
                                     std::unique_ptr<btDefaultMotionState> motionState,
                                     std::unique_ptr<btRigidBody> body,
                                     btDiscreteDynamicsWorld* world)
        : shape_(std::move(shape))
        , motionState_(std::move(motionState))
        , body_(std::move(body))
        , world_(world)
    {}

    BulletRigidBody::~BulletRigidBody()
    {
        if (body_ && world_)
            world_->removeRigidBody(body_.get());
    }

    glm::vec3 BulletRigidBody::GetPosition() const
    {
        if (!body_) return {};
        btTransform t;
        body_->getMotionState()->getWorldTransform(t);
        const btVector3& p = t.getOrigin();
        return {p.getX(), p.getY(), p.getZ()};
    }

    glm::quat BulletRigidBody::GetRotation() const
    {
        if (!body_) return glm::quat(1.f, 0.f, 0.f, 0.f);
        btTransform t;
        body_->getMotionState()->getWorldTransform(t);
        const btQuaternion& q = t.getRotation();
        return glm::quat(q.getW(), q.getX(), q.getY(), q.getZ());
    }

    glm::vec3 BulletRigidBody::GetLinearVelocity() const
    {
        if (!body_) return {};
        const btVector3& v = body_->getLinearVelocity();
        return {v.getX(), v.getY(), v.getZ()};
    }

    float BulletRigidBody::GetMass() const
    {
        if (!body_) return 0.f;
        btScalar invMass = body_->getInvMass();
        return invMass > 0.f ? 1.f / invMass : 0.f;
    }

    void BulletRigidBody::SetLinearVelocity(const glm::vec3& v)
    {
        if (!body_) return;
        body_->activate(true);
        body_->setLinearVelocity(btVector3(v.x, v.y, v.z));
    }

    void BulletRigidBody::ApplyCentralForce(const glm::vec3& f)
    {
        if (!body_) return;
        body_->activate(true);
        body_->applyCentralForce(btVector3(f.x, f.y, f.z));
    }

    void BulletRigidBody::SetTransform(const glm::vec3& pos, const glm::quat& rot)
    {
        if (!body_) return;
        btTransform t;
        t.setOrigin(btVector3(pos.x, pos.y, pos.z));
        t.setRotation(btQuaternion(rot.x, rot.y, rot.z, rot.w));
        motionState_->setWorldTransform(t);
        body_->setWorldTransform(t);
    }

    void BulletRigidBody::Activate()
    {
        if (body_) body_->activate(true);
    }

    void BulletRigidBody::SetKinematic(bool kinematic)
    {
        if (!body_) return;
        int flags = body_->getCollisionFlags();
        if (kinematic)
        {
            flags |= btCollisionObject::CF_KINEMATIC_OBJECT;
            body_->setCollisionFlags(flags);
            body_->setActivationState(DISABLE_DEACTIVATION);
        }
        else
        {
            flags &= ~btCollisionObject::CF_KINEMATIC_OBJECT;
            body_->setCollisionFlags(flags);
            body_->setActivationState(ACTIVE_TAG);
            body_->activate(true);
        }
    }

    bool BulletRigidBody::IsKinematic() const
    {
        if (!body_) return false;
        return (body_->getCollisionFlags() & btCollisionObject::CF_KINEMATIC_OBJECT) != 0;
    }

    void BulletRigidBody::SetLinearFactor(const glm::vec3& f)
    {
        if (body_) body_->setLinearFactor(btVector3(f.x, f.y, f.z));
    }

    void BulletRigidBody::SetAngularFactor(const glm::vec3& f)
    {
        if (body_) body_->setAngularFactor(btVector3(f.x, f.y, f.z));
    }

    void BulletRigidBody::SetAngularVelocity(const glm::vec3& v)
    {
        if (!body_) return;
        body_->setAngularVelocity(btVector3(v.x, v.y, v.z));
    }

} // namespace ettycc::physics
