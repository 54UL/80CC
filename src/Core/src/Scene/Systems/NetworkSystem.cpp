#include <Scene/Systems/NetworkSystem.hpp>
#include <Scene/Scene.hpp>
#include <Networking/NetworkComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Engine.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    void NetworkSystem::OnStart(Scene& scene, Engine& engine)
    {
        for (ecs::Entity e : scene.registry_.Pool<NetworkComponent>().Entities())
            InitNetwork(scene, engine, e);
    }

    void NetworkSystem::OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity)
    {
        if (scene.registry_.Has<NetworkComponent>(entity))
            InitNetwork(scene, engine, entity);
    }

    void NetworkSystem::OnUpdate(Scene& scene, float /*dt*/)
    {
        for (ecs::Entity e : scene.registry_.Pool<NetworkComponent>().Entities())
        {
            auto* nc = scene.registry_.Get<NetworkComponent>(e);
            if (!nc || !nc->IsInitialized()) continue;
            nc->BroadcastUpdate();
        }
    }

    void NetworkSystem::InitNetwork(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* nc   = scene.registry_.Get<NetworkComponent>(e);
        auto* node = scene.GetNode(e);
        if (!nc || !node || nc->IsInitialized()) return;

        nc->Init(engine.networkManager_, node->transform_, e, scene.registry_);
    }

} // namespace ettycc
