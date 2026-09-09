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

    void RenderSystem::OnUpdate(Scene& /*scene*/, float /*dt*/)
    {
        // Transform binding happens once in InitRenderable -- no per-frame
        // copy needed.  The renderable's transform() points directly at the
        // SceneNode's transform.
    }

    void RenderSystem::InitRenderable(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* rn   = scene.registry_.Get<RenderableNode>(e);
        auto* node = scene.GetNode(e);
        if (!rn || !node || rn->IsInitialized()) return;

        rn->InitRenderable(engine);

        // Seed the node transform from the renderable's stored transform,
        // then bind the renderable to the node transform (single source of truth).
        if (rn->renderable_)
        {
            node->transform_ = rn->renderable_->underylingTransform;
            rn->SyncTransform(node->transform_);
        }
    }

} // namespace ettycc
