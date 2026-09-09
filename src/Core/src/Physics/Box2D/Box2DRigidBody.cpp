#include <Physics/Box2D/Box2DRigidBody.hpp>
#include <Physics/PhysicsConstants.hpp>
#include <Math/Utils.hpp>
#include <glm/gtc/quaternion.hpp>

#include <box2d/box2d.h>
#include <cmath>

namespace ettycc::physics
{
    Box2DRigidBody::Box2DRigidBody(b2World* world, b2Body* body, float mass)
        : world_(world), body_(body), mass_(mass)
    {}

    Box2DRigidBody::~Box2DRigidBody()
    {
        if (body_ && world_)
            world_->DestroyBody(body_);
    }

    glm::vec3 Box2DRigidBody::GetPosition() const
    {
        if (!body_) return {};
        const b2Vec2& p = body_->GetPosition();
        const float inv = 1.f / kBox2DScale;
        return {p.x * inv, p.y * inv, 0.f};
    }

    glm::quat Box2DRigidBody::GetRotation() const
    {
        if (!body_) return glm::quat(1.f, 0.f, 0.f, 0.f);
        float angle = body_->GetAngle();
        return glm::angleAxis(angle, glm::vec3(0.f, 0.f, 1.f));
    }

    glm::vec3 Box2DRigidBody::GetLinearVelocity() const
    {
        if (!body_) return {};
        const b2Vec2& v = body_->GetLinearVelocity();
        const float inv = 1.f / kBox2DScale;
        return {v.x * inv, v.y * inv, 0.f};
    }

    float Box2DRigidBody::GetMass() const
    {
        return mass_;
    }

    void Box2DRigidBody::SetLinearVelocity(const glm::vec3& v)
    {
        if (!body_) return;
        body_->SetLinearVelocity({v.x * kBox2DScale, v.y * kBox2DScale});
    }

    void Box2DRigidBody::ApplyCentralForce(const glm::vec3& f)
    {
        if (!body_) return;
        body_->ApplyForceToCenter({f.x * kBox2DScale, f.y * kBox2DScale}, true);
    }

    void Box2DRigidBody::SetTransform(const glm::vec3& pos, const glm::quat& rot)
    {
        if (!body_) return;
        float angle = math::QuatToAngle2D(rot);
        body_->SetTransform({pos.x * kBox2DScale, pos.y * kBox2DScale}, angle);
    }

    void Box2DRigidBody::Activate()
    {
        if (body_) body_->SetAwake(true);
    }

    void Box2DRigidBody::SetKinematic(bool kinematic)
    {
        if (!body_) return;
        body_->SetType(kinematic ? b2_kinematicBody : b2_dynamicBody);
    }

    bool Box2DRigidBody::IsKinematic() const
    {
        if (!body_) return false;
        return body_->GetType() == b2_kinematicBody;
    }

    void Box2DRigidBody::SetLinearFactor(const glm::vec3& f)
    {
        // Box2D is inherently 2D -- no per-axis linear constraints.
        (void)f;
    }

    void Box2DRigidBody::SetAngularFactor(const glm::vec3& f)
    {
        if (!body_) return;
        body_->SetFixedRotation(f.z == 0.f);
    }

    void Box2DRigidBody::SetAngularVelocity(const glm::vec3& v)
    {
        if (body_) body_->SetAngularVelocity(v.z);
    }

} // namespace ettycc::physics
