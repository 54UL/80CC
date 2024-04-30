#ifndef SCENE_HPP
#define SCENE_HPP

#include "SceneNode.hpp"
#include "NodeComponent.hpp"
#include "Api.hpp"

#include <vector>
#include <memory>

#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

namespace ettycc
{
    class Engine; 
    class Scene
    {        
    private:

    public:
        std::shared_ptr<SceneNode> root_node_;     
        std::string sceneName_;
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

        // Serialization/Deserialization
        template <class Archive>
        void serialize(Archive &ar)
        {
            ar(CEREAL_NVP(sceneName_), CEREAL_NVP(root_node_), CEREAL_NVP(nodes_flat_));
        }
    };
}

#endif