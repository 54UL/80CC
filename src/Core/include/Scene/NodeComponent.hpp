#ifndef NODE_COMPONENT_HPP
#define NODE_COMPONENT_HPP

// #include <Scene/SceneNode.hpp>
#include <Scene/Api.hpp>

#include <memory>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ettycc
{
    class Engine;    // forward — Engine includes NodeComponent transitively
    class SceneNode; // forward — avoids circular include with SceneNode.hpp

    class NodeComponent
    {
        public:
        SceneNode* ownerNode_ = nullptr; // set by SceneNode::AddComponent

        virtual NodeComponentInfo GetComponentInfo() = 0;
        virtual void OnStart(std::shared_ptr<Engine> engineInstance) = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        virtual ~NodeComponent() = default;
    };
}


#endif