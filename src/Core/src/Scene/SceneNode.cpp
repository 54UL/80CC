#include <Scene/SceneNode.hpp>

namespace ettycc 
{
    SceneNode::SceneNode(const std::shared_ptr<SceneNode> &root) : parent_{root}
    {
        
    }   

    SceneNode::SceneNode(const std::shared_ptr<SceneNode> &root, const std::vector<std::shared_ptr<SceneNode>> &children) : parent_{root}, children_{children}
    {
        
    }

    SceneNode::~SceneNode()
    {

    }

    auto SceneNode::GetId() -> uint64_t
    {
        return id_;
    }

    auto SceneNode::GetName() -> std::string
    {
        return name_;
    }

    auto SceneNode::AddComponent(std::shared_ptr<NodeComponent> component) -> uint64_t
    {
        NodeComponentInfo info = component->GetComponentInfo(); 
        components_[info.processingChannel].emplace_back(component);
        return  info.id;
    }

    auto SceneNode::GetComponentById(uint64_t componentId) -> std::shared_ptr<NodeComponent>
    {
        // return components_.at(componentId);
        return std::shared_ptr<NodeComponent>();
    }

    auto SceneNode::GetComponentByName(const std::string &name) -> std::shared_ptr<NodeComponent>
    {
        return std::shared_ptr<NodeComponent>();
    }
}


