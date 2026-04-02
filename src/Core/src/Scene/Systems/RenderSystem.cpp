#include <Scene/Systems/RenderSystem.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Engine.hpp>

namespace ettycc
{
    void RenderSystem::OnStart(Scene& scene, Engine& engine)
    {
        for (ecs::Entity e : scene.registry_.Pool<RenderableNode>().Entities())
            InitRenderable(scene, engine, e);
    }

    void RenderSystem::OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity)
    {
        if (scene.registry_.Has<RenderableNode>(entity))
            InitRenderable(scene, engine, entity);
    }

    void RenderSystem::OnUpdate(Scene& scene, float /*dt*/)
    {
        for (ecs::Entity e : scene.registry_.Pool<RenderableNode>().Entities())
        {
            auto* rn   = scene.registry_.Get<RenderableNode>(e);
            auto* node = scene.GetNode(e);
            if (!rn || !node || !rn->IsInitialized()) continue;
            rn->SyncTransform(node->transform_);
        }
    }

    void RenderSystem::InitRenderable(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* rn   = scene.registry_.Get<RenderableNode>(e);
        auto* node = scene.GetNode(e);
        if (!rn || !node || rn->IsInitialized()) return;

        rn->InitRenderable(engine);

        // Seed the node transform from the renderable's stored transform.
        if (rn->renderable_)
            node->transform_ = rn->renderable_->underylingTransform;
    }

} // namespace ettycc
