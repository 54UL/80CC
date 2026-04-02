#pragma once
#include <ECS/ISystem.hpp>

namespace ettycc
{
    // ── AudioSystem ───────────────────────────────────────────────────────────
    // AUDIO channel.
    // Handles AudioSourceComponent and AudioListenerComponent:
    //   OnStart / OnEntityAdded — create AL sources, seed positions.
    //   OnUpdate                — sync positions, apply spatial audio each frame.
    class AudioSystem : public ISystem
    {
    public:
        ProcessingChannel Channel() const override { return ProcessingChannel::AUDIO; }

        void OnStart      (Scene& scene, Engine& engine) override;
        void OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity) override;
        void OnUpdate     (Scene& scene, float dt) override;

    private:
        void InitSource  (Scene& scene, Engine& engine, ecs::Entity e);
        void InitListener(Scene& scene, Engine& engine, ecs::Entity e);
    };
}
