#include <Scene/SceneNode.hpp>
#include <Dependency.hpp>
#include <spdlog/spdlog.h>

namespace ettycc 
{
    SceneNode::SceneNode()
    {
        InitNode();
    }

    SceneNode::SceneNode(const std::string &name) : name_(name)
    {
        InitNode();
    }

    SceneNode::SceneNode(const std::shared_ptr<SceneNode> &root) : parent_(root)
    {
        InitNode();
        SetParent(root);
    }

    SceneNode::SceneNode(const std::shared_ptr<SceneNode> &root, const std::string &name) : parent_(root), name_(name)
    {
        InitNode();
        SetParent(root);
    }

    SceneNode::SceneNode(const std::shared_ptr<SceneNode> &root, const std::vector<std::shared_ptr<SceneNode>> &children) : parent_(root), children_(children)
    {
        // TODO: DO THIS CASE...
    }

    SceneNode::~SceneNode()
    {

    }

    auto SceneNode::InitNode() -> void
    {
        children_ = std::vector<std::shared_ptr<SceneNode>>();
        components_ = std::map<ProcessingChannel, std::vector<std::shared_ptr<NodeComponent>>>();        
    }

    auto SceneNode::GetId() -> uint64_t
    {
        return id_;
    }

    auto SceneNode::GetName() -> std::string
    {
        return name_;
    }

    auto SceneNode::SetParent(const std::shared_ptr<SceneNode> &node) -> bool
    {
        if (parent_ != node) {
            node->AddNode(parent_);
            parent_ = node;
            return true;
        } 

        spdlog::warn("node {} is already a child of {}", name_, node->name_);
        return false;
    }

    auto SceneNode::AddNode(const std::shared_ptr<SceneNode> &node) -> uint64_t
    {
        children_.emplace_back(node);
        for (const auto &kvp : node->components_)
        {
            const std::vector<std::shared_ptr<NodeComponent>> &channelValues = kvp.second;

            // after added to the exec map initialize them...
            for (const auto &component : node->components_[kvp.first])
            {
                component->OnStart(GetDependency(Engine));
            }
        }

        // todo: fix this with an internal scene state to avoid passing Scene * parentScene... with something like:
        // SceneContext::RegisterNodes(nodes, sceneId);
        auto mainScene = GetDependency(Engine)->mainScene_;
        mainScene->nodes_flat_.emplace_back(node);

        return node->GetId();
    }

    auto SceneNode::RemoveNode(uint64_t id) -> void
    {
    }

    auto SceneNode::AddNodes(const std::vector<std::shared_ptr<SceneNode>> &nodes) -> std::vector<uint64_t>
    {
        std::vector<uint64_t> ids;

        // todo: not the best solution, use a better algo
        for (auto &node : nodes)
        {
            auto id = AddNode(node);
            ids.emplace_back(id);
        }

        return ids;
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

    auto ettycc::SceneNode::ComputeComponents(float deltaTime, ProcessingChannel processingChannel) -> void
    {
        auto componentsToProcess = components_[processingChannel];

        // lol iterate just like this 4 the moment...
        for (auto &component : componentsToProcess)
        {
            component->OnUpdate(deltaTime);
        } 
    }
}


