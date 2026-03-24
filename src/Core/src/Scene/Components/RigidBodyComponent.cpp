#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/SceneNode.hpp>
#include <Engine.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    RigidBodyComponent::RigidBodyComponent(float mass, glm::vec3 halfExtents, glm::vec3 initialPosition)
        : mass_(mass), halfExtents_(halfExtents), initialPosition_(initialPosition)
    {}

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
        return { 0, componentType, true, ProcessingChannel::MAIN };
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

        shape_ = new btBoxShape(btVector3(halfExtents_.x, halfExtents_.y, halfExtents_.z));

        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(spawnPos.x, spawnPos.y, spawnPos.z));

        btVector3 localInertia(0.0f, 0.0f, 0.0f);
        if (mass_ > 0.0f)
            shape_->calculateLocalInertia(mass_, localInertia);

        motionState_ = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo ci(mass_, motionState_, shape_, localInertia);
        body_ = new btRigidBody(ci);
        physWorld_->addRigidBody(body_);

        spdlog::info("[RigidBodyComponent] body created — mass={:.1f}  pos=({:.2f},{:.2f},{:.2f})",
                     mass_, spawnPos.x, spawnPos.y, spawnPos.z);
    }

    void RigidBodyComponent::OnUpdate(float /*deltaTime*/)
    {
        if (!body_ || !syncTransform_ || mass_ == 0.0f)
            return;

        btTransform trans;
        body_->getMotionState()->getWorldTransform(trans);
        const btVector3& pos = trans.getOrigin();
        syncTransform_->setGlobalPosition({ pos.getX(), pos.getY(), pos.getZ() });
    }
}
