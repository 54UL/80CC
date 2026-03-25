#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/SceneNode.hpp>
#include <Engine.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    RigidBodyComponent::RigidBodyComponent(float mass, glm::vec3 halfExtents, glm::vec3 initialPosition)
        : mass_(mass), halfExtents_(halfExtents), initialPosition_(initialPosition)
    {
        rigidBodyId_ = Utils::GetNextIncrementalId();
    }

    RigidBodyComponent::~RigidBodyComponent()
    {
        if (body_ && physWorld_)
            physWorld_->removeRigidBody(body_);

        delete motionState_;
        delete body_;
        delete shape_;
    }

    NodeComponentInfo RigidBodyComponent::GetComponentInfo()
    {
        return { rigidBodyId_, componentType, true, ProcessingChannel::MAIN };
    }

    void RigidBodyComponent::OnStart(std::shared_ptr<Engine> engineInstance)
    {
        physWorld_ = engineInstance->physicsWorld_.GetWorld();
        if (!physWorld_)
        {
            spdlog::error("[RigidBodyComponent] physics world not initialized");
            return;
        }

        // Discover the shared underlying transform from the sibling RenderableNode on this node
        if (ownerNode_)
        {
            auto it = ownerNode_->components_.find(ProcessingChannel::RENDERING);
            if (it != ownerNode_->components_.end())
            {
                for (auto& comp : it->second)
                {
                    auto* rn = dynamic_cast<RenderableNode*>(comp.get());
                    if (rn && rn->renderable_)
                    {
                        syncTransform_ = &rn->renderable_->underylingTransform;
                        break;
                    }
                }
            }
        }

        if (!syncTransform_)
            spdlog::warn("[RigidBodyComponent] no sibling RenderableNode found — physics won't drive visuals");

        // Use initial position from the shared transform if we have one, otherwise fall back to stored value
        glm::vec3 spawnPos = syncTransform_
            ? syncTransform_->getGlobalPosition()
            : initialPosition_;

        // Derive collision shape from the sprite's actual scale so it always
        // matches what is visible.  The sprite quad spans ±1 in local space;
        // after scale that becomes ±scale in world space — identical to the
        // half-extent of a box.
        // btBoxShape is preferred over btConvexHullShape for SDF_RS soft-rigid
        // contact: it uses an analytical SDF (exact distance-to-surface) rather
        // than the GJK approximation used by hull shapes.  This removes the
        // contact-position error that caused visible gaps with soft bodies.
        // btBoxShape absorbs its margin inward, so the outer collision surface
        // is exactly at ±halfExtents as specified.
        const glm::vec3 h = syncTransform_ ? syncTransform_->getGlobalScale() : halfExtents_;
        const btScalar  hx = btScalar(h.x);
        const btScalar  hy = btScalar(h.y);
        const btScalar  hz = btScalar(h.z > 0.05f ? h.z : 0.05f); // never degenerate
        shape_ = new btBoxShape(btVector3(hx, hy, hz));

        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(spawnPos.x, spawnPos.y, spawnPos.z));

        btVector3 localInertia(0.0f, 0.0f, 0.0f);
        if (mass_ > 0.0f)
            shape_->calculateLocalInertia(mass_, localInertia);

        motionState_ = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo ci(mass_, motionState_, shape_, localInertia);
        body_ = new btRigidBody(ci);

        // ── 2-D constraint: lock Z translation and X/Y rotation ──────────────
        // This keeps the simulation fully planar (XY plane) even though Bullet
        // is a 3-D engine, avoiding drift into/out of the screen.
        if (mass_ > 0.0f)
        {
            body_->setLinearFactor (btVector3(1.f, 1.f, 0.f)); // allow X,Y  — lock Z
            body_->setAngularFactor(btVector3(0.f, 0.f, 1.f)); // allow Z rot — lock X,Y
        }

        physWorld_->addRigidBody(body_);

        spdlog::info("[RigidBodyComponent] body created — mass={:.1f}  pos=({:.2f},{:.2f},{:.2f})",
                     mass_, spawnPos.x, spawnPos.y, spawnPos.z);
    }

    void RigidBodyComponent::OnUpdate(float /*deltaTime*/)
    {
        // Skip physics→visual sync while the editor gizmo is manipulating this body
        if (!body_ || !syncTransform_ || mass_ == 0.0f || isManipulated_)
            return;

        btTransform trans;
        body_->getMotionState()->getWorldTransform(trans);

        const btVector3&    btPos = trans.getOrigin();
        const btQuaternion& btQ   = trans.getRotation();

        glm::vec3 pos  (btPos.getX(), btPos.getY(), btPos.getZ());
        glm::quat rot  (btQ.getW(),   btQ.getX(),   btQ.getY(),   btQ.getZ());
        glm::vec3 scale = syncTransform_->getGlobalScale();

        // SetFromTRS builds a correct T*R*S matrix — avoids the broken
        // order-dependency in the individual setter chain.
        syncTransform_->SetFromTRS(pos, rot, scale);
    }

    // -------------------------------------------------------------------------

    void RigidBodyComponent::BeginManipulation()
    {
        if (!body_) return;
        isManipulated_ = true;

        // Switch to kinematic: Bullet no longer drives this body; we push transforms to it
        body_->setCollisionFlags(body_->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        body_->setActivationState(DISABLE_DEACTIVATION);
        body_->setLinearVelocity(btVector3(0.f, 0.f, 0.f));
        body_->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
    }

    void RigidBodyComponent::EndManipulation()
    {
        if (!body_) return;

        // Push final transform one last time before handing control back to physics
        SyncFromRenderable();

        // Restore dynamic mode — clear kinematic flag and wake the body
        body_->setCollisionFlags(body_->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
        body_->setLinearVelocity(btVector3(0.f, 0.f, 0.f));
        body_->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
        body_->setActivationState(ACTIVE_TAG);
        body_->activate(true);

        isManipulated_ = false;
    }

    void RigidBodyComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        PROP  (mass_,            "Mass");
        PROP  (halfExtents_,     "Half Extents");
        PROP  (initialPosition_, "Initial Position");
        PROP_F(rigidBodyId_,     "ID", ettycc::PROP_READ_ONLY | ettycc::PROP_NO_SERIAL);
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

        // For kinematic bodies Bullet polls the motion state each sub-step;
        // also set it directly on the body so it takes effect immediately.
        motionState_->setWorldTransform(t);
        body_->setWorldTransform(t);
    }
}
