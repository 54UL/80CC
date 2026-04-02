#ifndef NETWORK_COMPONENT_HPP
#define NETWORK_COMPONENT_HPP

#include <Scene/Api.hpp>
#include <Scene/Transform.hpp>
#include <Scene/PropertySystem.hpp>
#include <ECS/Entity.hpp>

#include <cereal/archives/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace ettycc::ecs { class Registry; }

namespace ettycc
{
    class NetworkManager;
    class RigidBodyComponent;

    // ── NetworkComponent ──────────────────────────────────────────────────────
    // Marks the entity as network-replicated.
    //
    // Host:   BroadcastUpdate() is called by NetworkSystem each MAIN frame.
    // Client: NetworkManager receives a packet and calls ApplyRemoteTransform().
    //         On first apply the sibling RigidBodyComponent is switched to
    //         kinematic so Bullet stops simulating it.
    class NetworkComponent
    {
    public:
        static constexpr const char*        componentType = "Network";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::MAIN;

        NetworkComponent() = default;
        explicit NetworkComponent(uint32_t networkId);
        ~NetworkComponent();

        // Non-copyable, movable.  The destructor calls Unregister(), so the
        // move constructor MUST null the source's networkManager_ — otherwise
        // a vector reallocation inside ComponentPool moves the element and
        // the moved-from destructor silently unregisters the live component.
        NetworkComponent(const NetworkComponent&)            = delete;
        NetworkComponent& operator=(const NetworkComponent&) = delete;

        NetworkComponent(NetworkComponent&& o) noexcept;
        NetworkComponent& operator=(NetworkComponent&& o) noexcept;

        // ── System-facing API (called by NetworkSystem) ───────────────────────
        void Init(NetworkManager& mgr,
                  Transform& syncTransform,
                  ecs::Entity entity,
                  ecs::Registry& registry);
        void BroadcastUpdate();
        bool IsInitialized() const { return networkManager_ != nullptr; }

        // ── Called by NetworkManager on packet receive ─────────────────────────
        void ApplyRemoteTransform(const glm::vec3& pos,
                                  const glm::quat& rot,
                                  const glm::vec3& scale);
        void ReleasePhysics();

        // ── Accessors ─────────────────────────────────────────────────────────
        uint32_t GetNetworkId() const { return networkId_; }

        // ── Editor inspector ──────────────────────────────────────────────────
        void InspectProperties(EditorPropertyVisitor& v);

        // ── Serialization ─────────────────────────────────────────────────────
        template <class Archive>
        void save(Archive& ar) const
        {
            ar(cereal::make_nvp("networkId", networkId_));
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(cereal::make_nvp("networkId", networkId_));
        }

    private:
        // ── Serialized ────────────────────────────────────────────────────────
        uint32_t networkId_ = 0;

        // ── Runtime (not serialized, set by NetworkSystem) ────────────────────
        NetworkManager*     networkManager_ = nullptr;
        Transform*          syncTransform_  = nullptr;
        ecs::Entity         entity_         = ecs::NullEntity;
        ecs::Registry*      registry_       = nullptr;
        bool                physicsLocked_  = false;

        // Looks up the sibling RigidBodyComponent from the registry each time
        // instead of caching a raw pointer that goes stale on pool reallocation.
        RigidBodyComponent* GetRigidBody() const;
    };

} // namespace ettycc

#endif
