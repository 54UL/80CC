#pragma once
#include <ECS/ISystem.hpp>

namespace ettycc
{
    // ── RenderSystem ──────────────────────────────────────────────────────────
    // RENDERING channel.
    // Handles RenderableNode initialization (registering with the render engine)
    // and per-frame transform sync (node transform → renderable).
    class RenderSystem : public ISystem
    {
    public:
        ProcessingChannel Channel() const override { return ProcessingChannel::RENDERING; }

        void OnStart      (Scene& scene, Engine& engine) override;
        void OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity) override;
        void OnUpdate     (Scene& scene, float dt) override;

    private:
        void InitRenderable(Scene& scene, Engine& engine, ecs::Entity e);
    };
}
