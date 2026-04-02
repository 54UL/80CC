#pragma once
#include <ECS/ISystem.hpp>

namespace ettycc
{
    // ── NetworkSystem ─────────────────────────────────────────────────────────
    // MAIN channel.
    // Handles NetworkComponent:
    //   OnStart / OnEntityAdded — register with NetworkManager, bind sibling refs.
    //   OnUpdate                — host broadcasts transform each frame.
    class NetworkSystem : public ISystem
    {
    public:
        ProcessingChannel Channel() const override { return ProcessingChannel::MAIN; }

        void OnStart      (Scene& scene, Engine& engine) override;
        void OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity) override;
        void OnUpdate     (Scene& scene, float dt) override;

    private:
        void InitNetwork(Scene& scene, Engine& engine, ecs::Entity e);
    };
}
