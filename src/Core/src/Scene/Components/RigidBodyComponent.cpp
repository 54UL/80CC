#include <Scene/Components/RigidBodyComponent.hpp>
#include <Physics/IPhysicsWorld.hpp>
#include <Physics/PhysicsConstants.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    RigidBodyComponent::RigidBodyComponent(float mass, glm::vec3 halfExtents,
                                           glm::vec3 initialPosition)
        : mass_(mass), halfExtents_(halfExtents), initialPosition_(initialPosition)
    {}

    RigidBodyComponent::~RigidBodyComponent() = default;

    void RigidBodyComponent::InitBody(physics::IPhysicsWorld& world,
                                      Transform& syncTransform,
                                      const Transform* seedTransform)
    {
        if (seedTransform)
            syncTransform = *seedTransform;

        syncTransform_ = &syncTransform;

        const glm::vec3 spawnPos = syncTransform_->getGlobalPosition();
        const glm::vec3 h        = syncTransform_->getGlobalScale();

        halfExtents_ = h;   // keep gizmo in sync with actual physics shape

        physics::RigidBodyDef def;
        def.mass          = mass_;
        def.shape         = physics::ShapeDef::Box(h);
        def.position      = spawnPos;
        def.linearFactor  = {physics::kLinearFactorX, physics::kLinearFactorY, physics::kLinearFactorZ};
        def.angularFactor = {physics::kAngularFactorX, physics::kAngularFactorY, physics::kAngularFactorZ};

        body_ = world.CreateRigidBody(def);

        spdlog::info("[RigidBodyComponent] body created -- mass={:.1f}  pos=({:.2f},{:.2f},{:.2f})",
                     mass_, spawnPos.x, spawnPos.y, spawnPos.z);
    }

    void RigidBodyComponent::SyncToTransform(Transform& t) const
    {
        if (!body_ || !syncTransform_ || mass_ == 0.f || isManipulated_) return;

        glm::vec3 pos   = body_->GetPosition();
        glm::quat rot   = body_->GetRotation();
        glm::vec3 scale  = syncTransform_->getGlobalScale();

        t.SetFromTRS(pos, rot, scale);
    }

    void RigidBodyComponent::ApplyCentralForce(const glm::vec3& f)
    {
        if (body_) body_->ApplyCentralForce(f);
    }

    void RigidBodyComponent::SetLinearVelocity(const glm::vec3& v)
    {
        if (body_) body_->SetLinearVelocity(v);
    }

    glm::vec3 RigidBodyComponent::GetPosition() const
    {
        return body_ ? body_->GetPosition() : glm::vec3{};
    }

    glm::quat RigidBodyComponent::GetRotation() const
    {
        return body_ ? body_->GetRotation() : glm::quat(1.f, 0.f, 0.f, 0.f);
    }

    glm::vec3 RigidBodyComponent::GetLinearVelocity() const
    {
        return body_ ? body_->GetLinearVelocity() : glm::vec3{};
    }

    void RigidBodyComponent::Reinitialize(physics::IPhysicsWorld& world,
                                          float newMass,
                                          const glm::vec3& newHalfExtents)
    {
        if (!body_) return;

        const glm::vec3 pos = GetPosition();
        const glm::vec3 vel = GetLinearVelocity();

        // Destroy old body
        body_.reset();

        mass_        = newMass;
        halfExtents_ = newHalfExtents;

        physics::RigidBodyDef def;
        def.mass          = newMass;
        def.shape         = physics::ShapeDef::Box(newHalfExtents);
        def.position      = pos;
        def.linearFactor  = {physics::kLinearFactorX, physics::kLinearFactorY, physics::kLinearFactorZ};
        def.angularFactor = {physics::kAngularFactorX, physics::kAngularFactorY, physics::kAngularFactorZ};

        body_ = world.CreateRigidBody(def);

        if (body_)
        {
            body_->SetLinearVelocity(vel);
            body_->Activate();
        }

        if (syncTransform_)
            syncTransform_->setGlobalScale(newHalfExtents);
    }

    void RigidBodyComponent::BeginManipulation()
    {
        if (!body_) return;
        isManipulated_ = true;
        body_->SetKinematic(true);
        body_->SetLinearVelocity({0.f, 0.f, 0.f});
        body_->SetAngularVelocity({0.f, 0.f, 0.f});
    }

    void RigidBodyComponent::EndManipulation()
    {
        if (!body_) return;

        SyncFromRenderable();

        body_->SetKinematic(false);
        body_->SetLinearVelocity({0.f, 0.f, 0.f});
        body_->SetAngularVelocity({0.f, 0.f, 0.f});
        body_->Activate();

        isManipulated_ = false;
    }

    void RigidBodyComponent::SyncFromRenderable()
    {
        if (!body_ || !syncTransform_) return;

        const glm::vec3 pos      = syncTransform_->getGlobalPosition();
        const glm::vec3 eulerDeg = syncTransform_->getStoredRotation();
        const glm::quat q        = glm::quat(glm::radians(eulerDeg));

        body_->SetTransform(pos, q);
    }

    void RigidBodyComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        PROP  (mass_,            "Mass");
        PROP  (halfExtents_,     "Half Extents");
        PROP  (initialPosition_, "Initial Position");
    }

} // namespace ettycc
