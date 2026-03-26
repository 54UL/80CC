#include <Networking/NetworkComponent.hpp>
#include <Networking/NetworkManager.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/SceneNode.hpp>
#include <Engine.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    NetworkComponent::NetworkComponent(uint32_t networkId)
        : networkId_(networkId)
    {
        componentId_ = Utils::GetNextIncrementalId();
    }

    NetworkComponent::~NetworkComponent()
    {
        ReleasePhysics();
        if (networkManager_)
            networkManager_->Unregister(networkId_);
    }

    NodeComponentInfo NetworkComponent::GetComponentInfo()
    {
        return { componentId_, componentType, true, ProcessingChannel::MAIN };
    }

    void NetworkComponent::OnStart(std::shared_ptr<Engine> engineInstance)
    {
        networkManager_ = &engineInstance->networkManager_;

        if (!ownerNode_) return;

        // Find sibling RenderableNode -> grab transform
        auto renderIt = ownerNode_->components_.find(ProcessingChannel::RENDERING);
        if (renderIt != ownerNode_->components_.end())
        {
            for (auto& comp : renderIt->second)
            {
                auto* rn = dynamic_cast<RenderableNode*>(comp.get());
                if (rn && rn->renderable_)
                {
                    syncTransform_ = &rn->renderable_->underylingTransform;
                    break;
                }
            }
        }

        // Find sibling RigidBodyComponent so we can freeze it on the client
        auto mainIt = ownerNode_->components_.find(ProcessingChannel::MAIN);
        if (mainIt != ownerNode_->components_.end())
        {
            for (auto& comp : mainIt->second)
            {
                auto* rb = dynamic_cast<RigidBodyComponent*>(comp.get());
                if (rb) { rigidBody_ = rb; break; }
            }
        }

        if (!syncTransform_)
            spdlog::warn("[NetworkComponent] id={} — no sibling RenderableNode", networkId_);
        if (!rigidBody_)
            spdlog::warn("[NetworkComponent] id={} — no sibling RigidBodyComponent, "
                         "physics won't be suppressed on client", networkId_);

        networkManager_->Register(networkId_, this);
    }

    void NetworkComponent::OnUpdate(float /*deltaTime*/)
    {
        // Only the authoritative host broadcasts transforms
        if (!networkManager_ || !networkManager_->IsActive() || !networkManager_->IsHost())
            return;

        if (!syncTransform_) return;

        const glm::vec3 pos      = syncTransform_->getGlobalPosition();
        const glm::vec3 scale    = syncTransform_->getGlobalScale();
        const glm::vec3 eulerDeg = syncTransform_->getStoredRotation();
        const glm::quat rot      = glm::quat(glm::radians(eulerDeg));

        networkManager_->BroadcastTransform(networkId_, pos, rot, scale);
    }

    // ── ApplyRemoteTransform ─────────────────────────────────────────────────
    // Called by NetworkManager when a packet for our networkId arrives.
    // First call freezes the sibling RigidBody into kinematic mode so the
    // local physics simulation doesn't fight the authoritative server data.
    void NetworkComponent::ApplyRemoteTransform(const glm::vec3& pos,
                                                const glm::quat& rot,
                                                const glm::vec3& scale)
    {
        if (!syncTransform_) return;

        // Freeze physics on first received packet (lazy — only when server is live)
        if (rigidBody_ && !physicsLocked_)
        {
            rigidBody_->BeginManipulation();
            physicsLocked_ = true;
        }

        // Apply authoritative world transform
        syncTransform_->SetFromTRS(pos, rot, scale);

        // Push it into Bullet so collision detection stays in sync
        if (rigidBody_ && physicsLocked_)
            rigidBody_->SyncFromRenderable();
    }

    // ── ReleasePhysics ────────────────────────────────────────────────────────
    // Called by NetworkManager on disconnect, or from the dtor.
    // Hands physics back to Bullet so the local simulation resumes.
    void NetworkComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        PROP_RO(networkId_,     "Network ID");
        PROP_F (physicsLocked_, "Physics Frozen", ettycc::PROP_READ_ONLY | ettycc::PROP_NO_SERIAL);
    }

    void NetworkComponent::ReleasePhysics()
    {
        if (rigidBody_ && physicsLocked_)
        {
            rigidBody_->EndManipulation();
            physicsLocked_ = false;
        }
    }

} // namespace ettycc
