#ifndef SCENE_NODE_HPP
#define SCENE_NODE_HPP

#include "NodeComponent.hpp"

#include <string>
#include <memory>
#include <map>
#include <vector>

namespace ettycc 
{
    class Scene;
    class SceneNode
    {
    private:
        uint64_t id_;
        uint64_t sceneId_;
        std::string name_;
        bool enabled_;

    public:
        //experimental...
        std::map<ProcessingChannel, std::vector<std::shared_ptr<NodeComponent>>> components_;
        std::shared_ptr<SceneNode> parent_;
        std::vector<std::shared_ptr<SceneNode>> children_; 

    public:
        SceneNode();
        SceneNode(const std::string& name);
        SceneNode(const std::shared_ptr<SceneNode>& root);
        SceneNode(const std::shared_ptr<SceneNode>& root, const std::vector<std::shared_ptr<SceneNode>>& children);
        ~SceneNode();
        auto InitNode() -> void;
        auto GetId() -> uint64_t;
        auto GetName() -> std::string;

        auto AddNode(const std::shared_ptr<SceneNode>& node) -> uint64_t;
        auto AddNodes(const std::vector<std::shared_ptr<SceneNode>>& node) -> std::vector<uint64_t>;
        auto RemoveNode(uint64_t id) -> void;

        auto AddComponent(std::shared_ptr<NodeComponent> component) -> uint64_t;
        auto GetComponentById(uint64_t componentId) -> std::shared_ptr<NodeComponent>;
        auto GetComponentByName(const std::string& name) -> std::shared_ptr<NodeComponent>;

        // auto RegisterComponents(float deltaTime);
        auto ComputeComponents(float deltaTime, ProcessingChannel processingChannel) -> void;
    };
}

#endif