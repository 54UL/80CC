#ifndef NETWORK_COMPONENT_HPP
#define NETWORK_COMPONENT_HPP

#include <Scene/NodeComponent.hpp>
#include <Scene/Transform.hpp>
#include <Scene/PropertySystem.hpp>

#include <cereal/archives/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace ettycc
{
    class NetworkManager;     // forward
    class RigidBodyComponent; // forward

    // ── NetworkComponent ──────────────────────────────────────────────────────
    // Attach to a SceneNode to mark it as network-replicated.
    //
    // Host:   OnUpdate reads transform -> BroadcastTransform every frame.
    // Client: NetworkManager receives packet -> ApplyRemoteTransform.
    //         On first apply the sibling RigidBodyComponent is switched to
    //         kinematic so Bullet stops simulating it, letting the network
    //         drive the body directly.
    class NetworkComponent : public NodeComponent
    {
    public:
        static constexpr const char* componentType = "Network";

        NetworkComponent() = default;
        explicit NetworkComponent(uint32_t networkId);
        ~NetworkComponent() override;

        NodeComponentInfo GetComponentInfo() override;
        void OnStart (std::shared_ptr<Engine> engineInstance) override;
        void OnUpdate(float deltaTime) override;

        // Called by NetworkManager when a remote transform packet arrives.
        void ApplyRemoteTransform(const glm::vec3& pos,
                                  const glm::quat& rot,
                                  const glm::vec3& scale);

        // Called by NetworkManager on disconnect — hands physics back to Bullet.
        void ReleasePhysics();

        void InspectProperties(EditorPropertyVisitor& v) override;

        uint32_t GetNetworkId() const { return networkId_; }

    public:
        // ── Cereal serialization (original key names preserved) ───────────────
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
        uint32_t          networkId_      = 0;
        uint64_t          componentId_    = 0;
        NetworkManager*   networkManager_ = nullptr;
        Transform*        syncTransform_  = nullptr;
        RigidBodyComponent* rigidBody_    = nullptr;  // sibling, may be null
        bool              physicsLocked_  = false;    // true while kinematic on client
    };

} // namespace ettycc

#endif
