#ifndef SCENE_NODE_HPP
#define SCENE_NODE_HPP

#include "NodeComponent.hpp"

#include <string>
#include <memory>
#include <map>
#include <vector>

namespace ettycc 
{
    class SceneNode
    {
    private:
        uint64_t id_;
        std::string name_;
        bool enabled_;


    public:
        //experimental...
        std::map<ProcessingChannel, std::vector<std::shared_ptr<NodeComponent>>> components_;
        std::shared_ptr<SceneNode> parent_;
        std::vector<std::shared_ptr<SceneNode>> children_; 

    public:
        SceneNode();
        SceneNode(const std::shared_ptr<SceneNode>& root); // we need the reference from somewhere...
        SceneNode(const std::shared_ptr<SceneNode>& root, const std::vector<std::shared_ptr<SceneNode>>& children);
        ~SceneNode();
        
        auto GetId() -> uint64_t;
        auto GetName() -> std::string;

        auto AddComponent(std::shared_ptr<NodeComponent> component) -> uint64_t;
        auto GetComponentById(uint64_t componentId) -> std::shared_ptr<NodeComponent>;
        auto GetComponentByName(const std::string& name) -> std::shared_ptr<NodeComponent>;
        // auto ProcessComponents(float deltaTime);
    };
}

#endif