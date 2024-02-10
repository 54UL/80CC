#include <Scene/Scene.hpp>

namespace ettycc
{
    Scene::Scene(Engine * engine): engineInstance_(engine)
    {

    }

    Scene::~Scene()
    {

    }

    auto Scene::Init() -> void
    {
        // Order of component intalization...
        // executionComponentMap_[ProcessingChannel::RENDERING]
        nodes_ = std::vector<std::shared_ptr<SceneNode>>();
    }

    uint64_t Scene::AddNode(const std::shared_ptr<SceneNode> &node)
    {
        // executionChannels_[processingChannel].emplace_back(node);
        nodes_.emplace_back(node);

        for (const auto &kvp : node->components_)
        {
            const std::vector<std::shared_ptr<NodeComponent>> &channelValues = kvp.second;
            executionComponentMap_[kvp.first].insert(executionComponentMap_[kvp.first].end(), channelValues.begin(), channelValues.end());
            // after added to the exec map initialize them...

            for (const auto &component : node->components_[kvp.first])
            {
                component->OnStart(engineInstance_);
            }
        }

        return node->GetId();
    }
    
    auto Scene::AddNodes(const std::vector<std::shared_ptr<SceneNode>> &nodes) -> std::vector<uint64_t>
    {
        //todo: not the best solution, use a better algo
        std::vector<uint64_t> ids;
    
        for (const auto& node : nodes)
        {
            auto id = AddNode(node);
            ids.emplace_back(id);
        }

        return ids;
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
        return nodes_;
    }
    
    auto Scene::Process(float deltaTime, ProcessingChannel processingChannel) -> void
    {        
        auto nodesToProcessUpdate = executionComponentMap_[processingChannel];

        // lol iterate just like this, multi thread compatible????
        for (auto &node : nodesToProcessUpdate)
        {
            node->OnUpdate(deltaTime);
        }
    }

    void Scene::RemoveNode(uint64_t id)
    {

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