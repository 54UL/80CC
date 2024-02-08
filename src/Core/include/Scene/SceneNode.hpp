#ifndef SCENE_NODE
#define SCENE_NODE

#include <string>
#include <Scene/NodeComponent.hpp>

namespace ettycc 
{
    class SceneNode
    {
    private:
        uint64_t id;
        std::string name_;
        std::vector<std::shared_ptr<NodeComponent>> components_;

    public:
        SceneNode();
        ~SceneNode();

        uint64_t AddComponent(std::shared_ptr<NodeComponent> component);
        
        auto GetComponentById(uint64_t componentId) -> std::shared_ptr<NodeComponent> component;
        auto GetComponentByName(const std::string& name) -> std::shared_ptr<NodeComponent> component;

    };
}

#endif