#include <Scene/Scene.hpp>

namespace ettycc
{
    Scene::Scene(const std::string& name): sceneName_{name}
    {
        root_node_ = std::make_shared<SceneNode>();     
        nodes_flat_ = std::vector<std::shared_ptr<SceneNode>>(); // this is used to avoid recursion and depth search yeah (idc about memory)
    }

    Scene::~Scene()
    {

    }

    auto Scene::Init() -> void
    {
        root_node_ = std::make_shared<SceneNode>("SCENE-ROOT");
    }

    std::shared_ptr<SceneNode> Scene::GetNodeById(uint64_t id)
    {
        return std::shared_ptr<SceneNode>(); // TODO: STD ALGORITHMS
    }

    auto Scene::GetNodesByName(const std::string &name) -> std::vector<std::shared_ptr<SceneNode>>
    {
        return std::vector<std::shared_ptr<SceneNode>>(); // TODO: STD ALGORITHMS
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


/*

BRIEF
    MAIN GOAL:


SceneManager::SceneManager()
{
}
    TickScene(){
        for object each
            {
                object.step()
            }
    }
    ... on object class:

    object::step(){
        for each component
            component.update()
    }
    ..
    Component is an interface...

    RenderingEntity : IComponent {
        update(object_ctx){
          transform = object_ctx->getComponent<Transform>();

          // Process because it can be the camera calculations and other rendering stuff..
          RenderingEntity->Process(transform);
        }
    }

    RigidBody : IComponent {
        update(object_ctx){
            transform = object_ctx->getComponent<Transform>();
            updatePhysics(transform);
        }
    }

    Networking : IComponent {
        update(object_ctx){
            transform = object_ctx->getComponent<Transform>();
            updateNetwork(transform);  // mutates the transforms
        }
    }

    AudioSource : IComponent {
        update(object_ctx){
            transform = object_ctx->getComponent<Transform>();
            updateAudio(transform);  // transform for positional audio??
        }
    }

*/


/*
      
        std::vector<std::shared_ptr<SceneNode>> stack;
        
        std::vector<std::shared_ptr<SceneNode>> flatNodes = {};

        while (!stack.empty())
        {
            auto peek = stack.front();
            stack.pop_back();

            // Parent first
            flatNodes.emplace_back(peek);

            // Children last
            flatNodes.insert(flatNodes.end(), peek->children_.begin(), peek->children_.end());

            // Add children to the stack to extract his children if they have...
            stack.insert(stack.end(), peek->children_.begin(), peek->children_.end());
        }

        for(auto& node : flatNodes) {
            // node->
        }
*/