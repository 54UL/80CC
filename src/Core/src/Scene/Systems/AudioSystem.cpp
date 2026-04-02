#include <Scene/Systems/AudioSystem.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Components/AudioSourceComponent.hpp>
#include <Scene/Components/AudioListenerComponent.hpp>
#include <Engine.hpp>

namespace ettycc
{
    void AudioSystem::OnStart(Scene& scene, Engine& engine)
    {
        for (ecs::Entity e : scene.registry_.Pool<AudioSourceComponent>().Entities())
            InitSource(scene, engine, e);

        for (ecs::Entity e : scene.registry_.Pool<AudioListenerComponent>().Entities())
            InitListener(scene, engine, e);
    }

    void AudioSystem::OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity)
    {
        if (scene.registry_.Has<AudioSourceComponent>(entity))
            InitSource(scene, engine, entity);

        if (scene.registry_.Has<AudioListenerComponent>(entity))
            InitListener(scene, engine, entity);
    }

    void AudioSystem::OnUpdate(Scene& scene, float dt)
    {
        for (ecs::Entity e : scene.registry_.Pool<AudioSourceComponent>().Entities())
        {
            auto* src  = scene.registry_.Get<AudioSourceComponent>(e);
            auto* node = scene.GetNode(e);
            if (!src || !node || !src->IsInitialized()) continue;
            src->UpdateAudio(dt, node->transform_);
        }

        for (ecs::Entity e : scene.registry_.Pool<AudioListenerComponent>().Entities())
        {
            auto* lst  = scene.registry_.Get<AudioListenerComponent>(e);
            auto* node = scene.GetNode(e);
            if (!lst || !node || !lst->IsInitialized()) continue;
            lst->UpdateListener(dt, node->transform_);
        }
    }

    void AudioSystem::InitSource(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* src  = scene.registry_.Get<AudioSourceComponent>(e);
        auto* node = scene.GetNode(e);
        if (!src || !node || src->IsInitialized()) return;
        src->Init(engine.audioManager_, engine, node->transform_);
    }

    void AudioSystem::InitListener(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* lst  = scene.registry_.Get<AudioListenerComponent>(e);
        auto* node = scene.GetNode(e);
        if (!lst || !node || lst->IsInitialized()) return;
        lst->Init(engine.audioManager_, node->transform_);
    }

} // namespace ettycc
