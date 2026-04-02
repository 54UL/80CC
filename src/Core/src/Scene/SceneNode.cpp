#include <Scene/SceneNode.hpp>
#include <Scene/Scene.hpp>
#include <Dependency.hpp>

namespace ettycc
{
    SceneNode::SceneNode()
    {
        InitNode();
    }

    SceneNode::SceneNode(const std::string& name) : name_(name)
    {
        InitNode();
    }

    SceneNode::SceneNode(const std::vector<std::shared_ptr<SceneNode>>& children)
        : id_(Utils::GetNextEntityId()), sceneId_(0), name_("unnamed"), enabled_(true),
          parent_(nullptr), children_(children)
    {}

    SceneNode::~SceneNode() {}

    auto SceneNode::InitNode() -> void
    {
        id_      = Utils::GetNextEntityId();
        sceneId_ = 0;
        enabled_ = true;

        if (name_.empty()) name_ = "unnamed";

        parent_   = nullptr;
        children_ = {};
    }

    auto SceneNode::SetParent(const std::shared_ptr<SceneNode>& node) -> bool
    {
        if (parent_ == node) return false;
        parent_ = node;
        return true;
    }

    auto SceneNode::AddNode(const std::shared_ptr<SceneNode>& node) -> ecs::Entity
    {
        children_.emplace_back(node);

        // Propagate scene pointer so template helpers (AddComponent etc.) work.
        if (scene_) node->scene_ = scene_;

        auto engine    = GetDependency(Engine);
        auto mainScene = engine->mainScene_;

        mainScene->nodes_flat_.push_back(node);
        mainScene->nodeIndex_[node->GetId()] = node.get();   // fast lookup
        mainScene->registry_.Track(node->GetId());

        // Flush components queued before this node had a scene pointer.
        for (auto& fn : node->pendingComponents_)
            fn(mainScene->registry_, node->GetId());
        node->pendingComponents_.clear();

        // Let all systems initialize this entity's components.
        mainScene->NotifyEntityAdded(node->GetId(), *engine);

        return node->GetId();
    }

    auto SceneNode::AddNodes(const std::vector<std::shared_ptr<SceneNode>>& nodes)
        -> std::vector<ecs::Entity>
    {
        std::vector<ecs::Entity> ids;
        ids.reserve(nodes.size());
        for (auto& n : nodes)
            ids.push_back(AddNode(n));
        return ids;
    }

    auto SceneNode::RemoveNode(ecs::Entity id) -> void
    {
        auto it = std::remove_if(children_.begin(), children_.end(),
            [id](const std::shared_ptr<SceneNode>& child)
            { return child && child->GetId() == id; });

        if (it == children_.end()) return;

        for (auto iter = it; iter != children_.end(); ++iter)
        {
            if (!*iter) continue;
            (*iter)->parent_ = nullptr;

            auto engine = GetDependency(Engine);
            if (engine && engine->mainScene_)
            {
                auto& flat = engine->mainScene_->nodes_flat_;
                flat.erase(std::remove(flat.begin(), flat.end(), *iter), flat.end());
                engine->mainScene_->nodeIndex_.erase(id);
                engine->mainScene_->registry_.Destroy(id);
            }
        }

        children_.erase(it, children_.end());
    }

    auto SceneNode::AddChild(std::shared_ptr<SceneNode> childNode) -> void
    {
        if (childNode->SetParent(shared_from_this()))
            AddNode(childNode);
        else
            spdlog::warn("Node [{}] cannot be parent of [{}] (already same parent)",
                         name_, childNode->GetName());
    }

} // namespace ettycc
