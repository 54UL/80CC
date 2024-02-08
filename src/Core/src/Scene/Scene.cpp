#include <Scene/Scene.hpp>

namespace ettycc
{

    Scene::Scene()
    {
    }

    Scene::~Scene()
    {
    }
    
    uint64_t Scene::AddNode(std::shared_ptr<SceneNode> node)
    {

    }

    std::shared_ptr<SceneNode> Scene::GetNodeById(uint64_t id)
    {

    }

    auto Scene::GetNodesByName(const std::string &name) -> std::vector<std::shared_ptr<SceneNode>>
    {
        return std::vector<std::shared_ptr<SceneNode>>();
    }

    void Scene::RemoveNode(uint64_t id)
    {

    }

    auto ettycc::Scene::ProcessNodes(float deltaTime) -> void
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
