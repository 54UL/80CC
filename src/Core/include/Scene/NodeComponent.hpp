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

    // Forward-declared so NodeComponent can declare InspectProperties without
    // pulling imgui into every translation unit that includes this header.
    struct EditorPropertyVisitor;

    class NodeComponent
    {
        public:
        SceneNode* ownerNode_ = nullptr; // set by SceneNode::AddComponent

        virtual NodeComponentInfo GetComponentInfo() = 0;
        virtual void OnStart(std::shared_ptr<Engine> engineInstance) = 0;
        virtual void OnUpdate(float deltaTime) = 0;

        // Override in each component to expose serializable/editable fields.
        // Implementation goes in the .cpp and includes EditorPropertyVisitor.hpp
        // so that imgui is never pulled into game-build translation units.
        // Use the PROP / PROP_RO / PROP_F macros inside the body.
        virtual void InspectProperties(EditorPropertyVisitor& /*v*/) {}

        virtual ~NodeComponent() = default;
    };
}


#endif