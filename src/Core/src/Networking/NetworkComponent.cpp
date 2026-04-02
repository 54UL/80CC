#include <Networking/NetworkComponent.hpp>
#include <Networking/NetworkManager.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <ECS/Registry.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    NetworkComponent::NetworkComponent(uint32_t networkId)
        : networkId_(networkId)
    {}

    NetworkComponent::NetworkComponent(NetworkComponent&& o) noexcept
        : networkId_(o.networkId_)
        , networkManager_(o.networkManager_)
        , syncTransform_(o.syncTransform_)
        , entity_(o.entity_)
        , registry_(o.registry_)
        , physicsLocked_(o.physicsLocked_)
    {
        // Null source so its destructor won't Unregister/ReleasePhysics.
        o.networkManager_ = nullptr;
        o.syncTransform_  = nullptr;
        o.registry_       = nullptr;
    }

    NetworkComponent& NetworkComponent::operator=(NetworkComponent&& o) noexcept
    {
        if (this == &o) return *this;

        // Clean up current state.
        if (physicsLocked_) physicsLocked_ = false;  // skip ReleasePhysics lookup
        if (networkManager_) networkManager_->Unregister(networkId_);

        networkId_       = o.networkId_;
        networkManager_  = o.networkManager_;
        syncTransform_   = o.syncTransform_;
        entity_          = o.entity_;
        registry_        = o.registry_;
        physicsLocked_   = o.physicsLocked_;

        o.networkManager_ = nullptr;
        o.syncTransform_  = nullptr;
        o.registry_       = nullptr;
        return *this;
    }

    NetworkComponent::~NetworkComponent()
    {
        // During scene teardown the RigidBody pool may already be destroyed,
        // so skip the registry lookup — just clear the lock flag.
        physicsLocked_ = false;
        if (networkManager_)
            networkManager_->Unregister(networkId_);
    }

    // ── Look up sibling RigidBodyComponent from the registry on demand ─────
    // Never cache this pointer — pool reallocations invalidate it.
    RigidBodyComponent* NetworkComponent::GetRigidBody() const
    {
        if (!registry_ || entity_ == ecs::NullEntity) return nullptr;
        return registry_->Get<RigidBodyComponent>(entity_);
    }

    // ── System-facing: initialize networking ──────────────────────────────────
    void NetworkComponent::Init(NetworkManager& mgr,
                                Transform& syncTransform,
                                ecs::Entity entity,
                                ecs::Registry& registry)
    {
        networkManager_ = &mgr;
        syncTransform_  = &syncTransform;
        entity_         = entity;
        registry_       = &registry;

        if (!syncTransform_)
            spdlog::warn("[NetworkComponent] id={} — no transform found", networkId_);
        if (!GetRigidBody())
            spdlog::warn("[NetworkComponent] id={} — no sibling RigidBodyComponent", networkId_);

        networkManager_->Register(networkId_, this);
    }

    // ── System-facing: broadcast transform (host only) ────────────────────────
    void NetworkComponent::BroadcastUpdate()
    {
        if (!networkManager_ || !networkManager_->IsActive() || !networkManager_->IsHost())
            return;
        if (!syncTransform_) return;

        const glm::vec3 pos      = syncTransform_->getGlobalPosition();
        const glm::vec3 scale    = syncTransform_->getGlobalScale();
        const glm::vec3 eulerDeg = syncTransform_->getStoredRotation();
        const glm::quat rot      = glm::quat(glm::radians(eulerDeg));

        networkManager_->BroadcastTransform(networkId_, pos, rot, scale);
    }

    // ── Called by NetworkManager on packet receive ─────────────────────────────
    void NetworkComponent::ApplyRemoteTransform(const glm::vec3& pos,
                                                const glm::quat& rot,
                                                const glm::vec3& scale)
    {
        if (!syncTransform_) return;

        auto* rb = GetRigidBody();
        if (rb && !physicsLocked_)
        {
            rb->BeginManipulation();
            physicsLocked_ = true;
        }

        syncTransform_->SetFromTRS(pos, rot, scale);

        if (rb && physicsLocked_)
            rb->SyncFromRenderable();
    }

    void NetworkComponent::ReleasePhysics()
    {
        auto* rb = GetRigidBody();
        if (rb && physicsLocked_)
        {
            rb->EndManipulation();
            physicsLocked_ = false;
        }
    }

    // ── Editor inspector ──────────────────────────────────────────────────────
    void NetworkComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        PROP_RO(networkId_,     "Network ID");
        PROP_F (physicsLocked_, "Physics Frozen", ettycc::PROP_READ_ONLY | ettycc::PROP_NO_SERIAL);
    }

} // namespace ettycc
