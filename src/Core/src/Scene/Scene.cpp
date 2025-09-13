#include <Scene/Scene.hpp>

namespace ettycc
{
    Scene::Scene(const std::string& name): sceneName_{name}
    {
        root_node_ = std::make_shared<SceneNode>(sceneName_);
        nodes_flat_ = std::vector<std::shared_ptr<SceneNode>>(); // this is used to avoid recursion and depth search yeah (idc about memory)
    }

    Scene::~Scene()
    {

    }

    auto Scene::Init() -> void
    {
        // root_node_ = std::make_shared<SceneNode>("SCENE-ROOT");

        // if node_flat populated means it was loaded
        for (const auto &node : nodes_flat_)
        {
            node->InitNode();
        }
    }

    std::shared_ptr<SceneNode> Scene::GetNodeById(uint64_t id)
    {
        return std::shared_ptr<SceneNode>(); // TODO: STD ALGORITHMS
    }

    auto Scene::GetNodesByName(const std::string &name) -> std::vector<std::shared_ptr<SceneNode>>
    {
        std::vector<std::shared_ptr<SceneNode>> result;
        for (const auto& node : nodes_flat_)
        {
            if (node && node->GetName() == name)
            {
                result.push_back(node);
            }
        }
        return result;
    }

    auto Scene::GetAllNodes() -> std::vector<std::shared_ptr<SceneNode>>
    {
        return nodes_flat_;
    }

    auto Scene::Process(float deltaTime, ProcessingChannel processingChannel) -> void
    {
        // iterate over all the nodes like this 4 the moment (improvents here pls)
        for (auto &node : nodes_flat_)
        {
            node->ComputeComponents(deltaTime, processingChannel);
        }
    }
}