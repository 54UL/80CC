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
        std::vector<NodeComponent> components_;

    public:
        SceneNode();
        ~SceneNode();

        void AddComponent(std::shared_ptr<NodeComponent> component);
    };
}

#endif