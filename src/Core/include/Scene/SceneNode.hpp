#ifndef SCENE_NODE_HPP
#define SCENE_NODE_HPP

#include "NodeComponent.hpp"

#include <string>
#include <memory>
#include <map>
#include <vector>

#include <spdlog/spdlog.h>

#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>

namespace ettycc 
{
    class Scene;
    class SceneNode : public std::enable_shared_from_this<SceneNode>
    {
    private:
        uint64_t id_;
        uint64_t sceneId_;
        std::string name_;
        bool enabled_;
        bool initialized;

    public:
        // PUBLIC EXPERIMENTAL MEMBERS 
        bool isSelected_;
        std::map<ProcessingChannel, std::vector<std::shared_ptr<NodeComponent>>> components_;

        std::shared_ptr<SceneNode> parent_; // make it weak ptr...
        std::vector<std::shared_ptr<SceneNode>> children_; 

        // Serialization/Deserialization
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(id_),
               CEREAL_NVP(sceneId_),
               CEREAL_NVP(name_),
               CEREAL_NVP(enabled_),
               CEREAL_NVP(components_),
               CEREAL_NVP(parent_),
               CEREAL_NVP(children_));
        }

    public:
        SceneNode();
        SceneNode(const std::string& name);
        SceneNode(const std::vector<std::shared_ptr<SceneNode>>& children);
        
        ~SceneNode();

        auto InitNode() -> void;
        auto GetId() -> uint64_t;
        auto GetName() -> std::string;
        auto SetName(const std::string& name) -> void;

        auto SetParent(const std::shared_ptr<SceneNode>& node) -> bool; 

        auto AddNode(const std::shared_ptr<SceneNode>& node) -> uint64_t;
        auto AddNodes(const std::vector<std::shared_ptr<SceneNode>>& node) -> std::vector<uint64_t>;
        auto RemoveNode(uint64_t id) -> void;

        auto AddComponent(std::shared_ptr<NodeComponent> component) -> uint64_t;

        auto AddChild(std::shared_ptr<SceneNode> childrenNode) -> void;

        auto GetComponentById(uint64_t componentId) -> std::shared_ptr<NodeComponent>;
        auto GetComponentByName(const std::string& name) -> std::shared_ptr<NodeComponent>;

        auto ComputeComponents(float deltaTime, ProcessingChannel processingChannel) -> void;        
    };
}

#endif