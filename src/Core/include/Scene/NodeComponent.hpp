#ifndef NODE_COMPONENT_HPP
#define NODE_COMPONENT_HPP

#include <Scene/SceneNode.hpp>

class NodeComponent
{
    public:
    // Life cycle functions
    virtual void OnStart() = 0;
    virtual void OnUpdate() = 0;

    // Events
    virtual void OnParentChanged(SceneNode * parentContext) = 0;
};

#endif