#ifndef NODE_COMPONENT_HPP
#define NODE_COMPONENT_HPP

class NodeComponent
{
    public:
    virtual void OnStart() = 0;
    virtual void OnUpdate() = 0;
    virtual void OnPreUpdate(){}
    virtual void OnLateUpdate(){}
};

#endif