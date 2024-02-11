#ifndef SCENE_HPP
#define SCENE_HPP

#include <vector>
#include <memory>

#include "SceneNode.hpp"
#include "NodeComponent.hpp"

#include "Api.hpp"

namespace ettycc
{
    class Engine; 
    class Scene
    {        
    private:
        std::string sceneName_;

    public:
        std::shared_ptr<SceneNode> root_node_;     
        std::vector<std::shared_ptr<SceneNode>> nodes_flat_; // this is used to avoid recursion and depth search yeah (idc about memory)

        Scene(const std::string& name);
        ~Scene();

        // Scene API
        auto Init() -> void; // not used 4 the moment

        // High level node operations....
        auto GetNodeById(uint64_t id) -> std::shared_ptr<SceneNode>;
        auto GetNodesByName(const std::string &name) -> std::vector<std::shared_ptr<SceneNode>>;
        auto GetAllNodes() -> std::vector<std::shared_ptr<SceneNode>>;
       
        auto Process(float deltaTime, ProcessingChannel processingChannel) -> void; // this is called from the engine api...
    };
}

#endif