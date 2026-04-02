#ifndef SCENE_HPP
#define SCENE_HPP

#include <Scene/SceneNode.hpp>
#include <Scene/Api.hpp>
#include <ECS/Registry.hpp>
#include <ECS/ISystem.hpp>

#include <vector>
#include <memory>
#include <unordered_map>

#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    class Engine;

    template <class T>
    struct EntryView
    {
        ecs::Entity  entity    = 0;
        const T*     component = nullptr;

        template<class Archive>
        void serialize(Archive& a) const
        {
            a(cereal::make_nvp("first",  entity),
              cereal::make_nvp("second", *component));
        }
    };

    class Scene
    {
    public:
        std::string                             sceneName_;
        std::shared_ptr<SceneNode>              root_node_;
        std::vector<std::shared_ptr<SceneNode>> nodes_flat_; // flat list for O(n) iteration

        // Highest entity ID in use when the scene was saved.  Restored on load
        // so the global counter is fast-forwarded past it and new nodes never
        // get IDs that collide with already-loaded ones.
        uint64_t                                maxEntityId_ = 0;

        ecs::Registry                           registry_;
        std::vector<std::unique_ptr<ISystem>>   systems_;
        std::unordered_map<ecs::Entity, SceneNode*> nodeIndex_;

    public:
        explicit Scene(const std::string& name);
        ~Scene();

        // ── Initialization ────────────────────────────────────────────────────
        // Rebuilds nodeIndex_ from nodes_flat_, tracks all entity IDs in the
        // registry, then calls OnStart on every registered system.
        auto Init(Engine& engine) -> void;

        // ── Node lookup ───────────────────────────────────────────────────────
        auto GetNode        (ecs::Entity id)               -> SceneNode*;
        auto GetNodesByName (const std::string& name)      -> std::vector<std::shared_ptr<SceneNode>>;
        auto GetAllNodes    ()                             -> std::vector<std::shared_ptr<SceneNode>>;
        auto GetTransform   (ecs::Entity e)                -> Transform*;

        // ── ECS system management ─────────────────────────────────────────────
        auto RegisterSystem(std::unique_ptr<ISystem> system) -> void;

        // Called by SceneNode::AddNode when a node enters the scene at runtime.
        // Propagates the event to all registered systems.
        auto NotifyEntityAdded(ecs::Entity e, Engine& engine) -> void;

        // ── Per-frame update ──────────────────────────────────────────────────
        // Calls OnUpdate on every system whose Channel() == processingChannel.
        auto Process(float deltaTime, ProcessingChannel processingChannel) -> void;

        // ── Serialization ─────────────────────────────────────────────────────
        template <class Archive>
        void serialize(Archive& ar);

    private:
        // Defined in Scene.cpp for the two concrete cereal archive types.
        void SerializeComponents(cereal::JSONOutputArchive& ar);
        void SerializeComponents(cereal::JSONInputArchive&  ar);

        void RebuildIndex();
    };
}

// Template helpers for SceneNode that require Scene to be fully defined.
#include <Scene/SceneNode.inl>

#endif
