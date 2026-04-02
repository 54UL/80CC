#ifndef SCENE_NODE_HPP
#define SCENE_NODE_HPP

#include <Scene/Transform.hpp>
#include <Scene/Api.hpp>
#include <ECS/Entity.hpp>

#include <string>
#include <memory>
#include <vector>
#include <functional>

#include <spdlog/spdlog.h>

#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

namespace ettycc::ecs { class Registry; }

namespace ettycc
{
    class Scene; // forward — template helpers defined in Scene/SceneNode.inl

    class SceneNode : public std::enable_shared_from_this<SceneNode>
    {
    private:
        ecs::Entity id_      = ecs::NullEntity;
        uint64_t    sceneId_ = 0;
        std::string name_;
        bool        enabled_ = true;

    public:
        bool       isSelected_ = false;
        Transform  transform_;

        std::shared_ptr<SceneNode>              parent_;
        std::vector<std::shared_ptr<SceneNode>> children_;

        // Set by Scene::AddNode so template helpers below can reach the registry.
        Scene* scene_ = nullptr;

        // Components queued before this node was attached to a scene.
        // Flushed into the registry inside AddNode().
        std::vector<std::function<void(ecs::Registry&, ecs::Entity)>> pendingComponents_;

        // ── Serialization ─────────────────────────────────────────────────────
        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(CEREAL_NVP(id_),
               CEREAL_NVP(sceneId_),
               CEREAL_NVP(name_),
               CEREAL_NVP(enabled_),
               CEREAL_NVP(transform_),
               CEREAL_NVP(parent_),
               CEREAL_NVP(children_));
        }

    public:
        SceneNode();
        explicit SceneNode(const std::string& name);
        explicit SceneNode(const std::vector<std::shared_ptr<SceneNode>>& children);
        ~SceneNode();

        auto InitNode()                          -> void;
        auto GetId()    const                    -> ecs::Entity   { return id_; }
        auto GetName()  const                    -> std::string   { return name_; }
        auto SetName(const std::string& name)    -> void          { name_ = name; }
        auto IsEnabled() const                   -> bool          { return enabled_; }

        auto SetParent(const std::shared_ptr<SceneNode>& node)    -> bool;

        auto AddNode (const std::shared_ptr<SceneNode>& node)     -> ecs::Entity;
        auto AddNodes(const std::vector<std::shared_ptr<SceneNode>>& nodes)
                                                                  -> std::vector<ecs::Entity>;
        auto RemoveNode(ecs::Entity id)                           -> void;
        auto AddChild  (std::shared_ptr<SceneNode> childNode)     -> void;

        // ── ECS component helpers (defined in SceneNode.inl, included by Scene.hpp) ──
        // Add a component of type T. If the node is not yet in a scene the
        // component is queued and flushed into the registry when the node enters.
        template<typename T>  void AddComponent(T comp);
        // Get a pointer to component T on this entity (nullptr if absent).
        template<typename T>  T* GetComponent();
        template<typename T>  bool HasComponent() const;
    };
}

#endif
