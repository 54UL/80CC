#ifndef SCENE_HPP
#define SCENE_HPP

#include <vector>
#include <memory>
#include <Scene/SceneNode.hpp>
#include <Scene/NodeComponent.hpp>

namespace ettycc
{
    class Scene
    {
    private:
        std::vector<std::shared_ptr<SceneNode>> nodes_;

    public:
        Scene();
        ~Scene();

        // Scene API
        auto AddNode(std::shared_ptr<SceneNode> node) -> uint64_t;
        auto RemoveNode(uint64_t id) -> void;

        auto GetNodeById(uint64_t id) -> std::shared_ptr<SceneNode>;
        auto GetNodesByName(const std::string &name) -> std::vector<std::shared_ptr<SceneNode>>;

        auto ProcessNodes(float deltaTime) -> void;
    };
}

#endif