#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    RigidBodyComponent::RigidBodyComponent(float mass, glm::vec3 halfExtents,
                                           glm::vec3 initialPosition)
        : mass_(mass), halfExtents_(halfExtents), initialPosition_(initialPosition)
    {}

    RigidBodyComponent::~RigidBodyComponent()
    {
        if (body_ && physWorld_)
            physWorld_->removeRigidBody(body_.get());
    }

    // ── System-facing: initialize Bullet rigid body ───────────────────────────
    void RigidBodyComponent::InitBody(btDiscreteDynamicsWorld* world,
                                      Transform& syncTransform,
                                      const Transform* seedTransform)
    {
        physWorld_ = world;
        if (!physWorld_)
        {
            spdlog::error("[RigidBodyComponent] physics world not initialized");
            return;
        }

        // Seed the node transform from the sibling renderable when provided.
        if (seedTransform)
            syncTransform = *seedTransform;

        syncTransform_ = &syncTransform;

        const glm::vec3 spawnPos = syncTransform_->getGlobalPosition();
        const glm::vec3 h        = syncTransform_->getGlobalScale();

        const btScalar hx = btScalar(h.x > 0.05f ? h.x : 0.05f);
        const btScalar hy = btScalar(h.y > 0.05f ? h.y : 0.05f);
        const btScalar hz = btScalar(h.z > 0.05f ? h.z : 0.05f);

        shape_ = std::make_unique<btBoxShape>(btVector3(hx, hy, hz));

        btTransform startXf;
        startXf.setIdentity();
        startXf.setOrigin(btVector3(spawnPos.x, spawnPos.y, spawnPos.z));

        btVector3 localInertia(0.f, 0.f, 0.f);
        if (mass_ > 0.f)
            shape_->calculateLocalInertia(mass_, localInertia);

        motionState_ = std::make_unique<btDefaultMotionState>(startXf);
        btRigidBody::btRigidBodyConstructionInfo ci(mass_, motionState_.get(), shape_.get(), localInertia);

        body_ = std::make_unique<btRigidBody>(ci);
        // 2-D constraint: lock Z translation and X/Y rotation.
        if (mass_ > 0.f)
        {
            body_->setLinearFactor (btVector3(1.f, 1.f, 0.f));
            body_->setAngularFactor(btVector3(0.f, 0.f, 1.f));
        }

        physWorld_->addRigidBody(body_.get());

        spdlog::info("[RigidBodyComponent] body created — mass={:.1f}  pos=({:.2f},{:.2f},{:.2f})",
                     mass_, spawnPos.x, spawnPos.y, spawnPos.z);
    }

    // ── System-facing: pull physics result into node transform ────────────────
    void RigidBodyComponent::SyncToTransform(Transform& t) const
    {
        if (!body_ || !syncTransform_ || mass_ == 0.f || isManipulated_) return;

        btTransform trans;
        body_->getMotionState()->getWorldTransform(trans);

        const btVector3&    btPos = trans.getOrigin();
        const btQuaternion& btQ   = trans.getRotation();

        glm::vec3 pos  (btPos.getX(),  btPos.getY(),  btPos.getZ());
        glm::quat rot  (btQ.getW(),    btQ.getX(),    btQ.getY(),    btQ.getZ());
        glm::vec3 scale = syncTransform_->getGlobalScale();

        t.SetFromTRS(pos, rot, scale);
    }

    // ── Force / velocity helpers ────────────────────────────────────────────────
    void RigidBodyComponent::ApplyCentralForce(const glm::vec3& f)
    {
        if (!body_) return;
        body_->activate(true);
        body_->applyCentralForce(btVector3(f.x, f.y, f.z));
    }

    void RigidBodyComponent::SetLinearVelocity(const glm::vec3& v)
    {
        if (!body_) return;
        body_->activate(true);
        body_->setLinearVelocity(btVector3(v.x, v.y, v.z));
    }

    glm::vec3 RigidBodyComponent::GetPosition() const
    {
        if (!body_) return {};
        btTransform t;
        body_->getMotionState()->getWorldTransform(t);
        const btVector3& p = t.getOrigin();
        return { p.getX(), p.getY(), p.getZ() };
    }

    glm::vec3 RigidBodyComponent::GetLinearVelocity() const
    {
        if (!body_) return {};
        const btVector3& v = body_->getLinearVelocity();
        return { v.getX(), v.getY(), v.getZ() };
    }

    void RigidBodyComponent::Reinitialize(float newMass, const glm::vec3& newHalfExtents)
    {
        if (!body_ || !physWorld_) return;

        // Capture current state before tearing down.
        const glm::vec3 pos = GetPosition();
        const glm::vec3 vel = GetLinearVelocity();

        // Remove old body from the world.
        physWorld_->removeRigidBody(body_.get());
        body_.reset();
        motionState_.reset();
        shape_.reset();

        // Update stored fields.
        mass_        = newMass;
        halfExtents_ = newHalfExtents;

        const btScalar hx = btScalar(newHalfExtents.x > 0.05f ? newHalfExtents.x : 0.05f);
        const btScalar hy = btScalar(newHalfExtents.y > 0.05f ? newHalfExtents.y : 0.05f);
        const btScalar hz = btScalar(newHalfExtents.z > 0.05f ? newHalfExtents.z : 0.05f);

        shape_ = std::make_unique<btBoxShape>(btVector3(hx, hy, hz));

        btTransform startXf;
        startXf.setIdentity();
        startXf.setOrigin(btVector3(pos.x, pos.y, pos.z));

        btVector3 localInertia(0.f, 0.f, 0.f);
        if (newMass > 0.f)
            shape_->calculateLocalInertia(newMass, localInertia);

        motionState_ = std::make_unique<btDefaultMotionState>(startXf);
        btRigidBody::btRigidBodyConstructionInfo ci(newMass, motionState_.get(), shape_.get(), localInertia);
        body_ = std::make_unique<btRigidBody>(ci);

        if (newMass > 0.f)
        {
            body_->setLinearFactor (btVector3(1.f, 1.f, 0.f));
            body_->setAngularFactor(btVector3(0.f, 0.f, 1.f));
        }

        physWorld_->addRigidBody(body_.get());

        // Restore velocity (momentum-conserved value set by caller).
        body_->setLinearVelocity(btVector3(vel.x, vel.y, vel.z));
        body_->activate(true);

        // Update the sync transform scale so the visual matches.
        if (syncTransform_)
            syncTransform_->setGlobalScale(newHalfExtents);
    }

    // ── Editor gizmo API ──────────────────────────────────────────────────────
    void RigidBodyComponent::BeginManipulation()
    {
        if (!body_) return;
        isManipulated_ = true;

        body_->setCollisionFlags(body_->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        body_->setActivationState(DISABLE_DEACTIVATION);
        body_->setLinearVelocity (btVector3(0.f, 0.f, 0.f));
        body_->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
    }

    void RigidBodyComponent::EndManipulation()
    {
        if (!body_) return;

        SyncFromRenderable();

        body_->setCollisionFlags(body_->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        body_->setLinearVelocity (btVector3(0.f, 0.f, 0.f));
        body_->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
        body_->setActivationState(ACTIVE_TAG);
        body_->activate(true);

        isManipulated_ = false;
    }

    void RigidBodyComponent::SyncFromRenderable()
    {
        if (!body_ || !syncTransform_) return;

        const glm::vec3 pos      = syncTransform_->getGlobalPosition();
        const glm::vec3 eulerDeg = syncTransform_->getStoredRotation();
        const glm::quat q        = glm::quat(glm::radians(eulerDeg));

        btTransform t;
        t.setOrigin  (btVector3(pos.x, pos.y, pos.z));
        t.setRotation(btQuaternion(q.x, q.y, q.z, q.w));

        motionState_->setWorldTransform(t);
        body_->setWorldTransform(t);
    }

    // ── Editor inspector ──────────────────────────────────────────────────────
    void RigidBodyComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        PROP  (mass_,            "Mass");
        PROP  (halfExtents_,     "Half Extents");
        PROP  (initialPosition_, "Initial Position");
    }

} // namespace ettycc
