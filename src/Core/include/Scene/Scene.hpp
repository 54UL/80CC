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
        std::vector<std::shared_ptr<SceneNode>> nodes_;
        // components by his processing channel (ordered...)
        std::map<ProcessingChannel, std::vector<std::shared_ptr<NodeComponent>>> executionComponentMap_; // for the moment only used to separate the execution layers...        
        Engine * engineInstance_;

    public:
        Scene(Engine * engine);
        ~Scene();

        // Scene API
        auto Init() -> void;
        auto AddNode(const std::shared_ptr<SceneNode>& node, ProcessingChannel processingChannel) -> uint64_t;
        auto RemoveNode(uint64_t id) -> void;

        auto GetNodeById(uint64_t id) -> std::shared_ptr<SceneNode>;
        auto GetNodesByName(const std::string &name) -> std::vector<std::shared_ptr<SceneNode>>;
        auto GetAllNodes() -> std::vector<std::shared_ptr<SceneNode>>;
       
        auto Process(float deltaTime, ProcessingChannel processingChannel) -> void; // this is called from the engine api...

        // serialization part...
        // auto LoadScene(/*file path???*/);
        // auto StoreScene(/*Store the current scene nodes...*/);
    };
}

#endif