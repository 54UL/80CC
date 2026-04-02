#pragma once
#include <ECS/ISystem.hpp>

namespace ettycc
{
    // ── PhysicsSystem ─────────────────────────────────────────────────────────
    // MAIN channel.
    // Owns initialization and per-frame update for:
    //   • RigidBodyComponent (Bullet btRigidBody)
    //   • SoftBodyComponent  (Bullet btSoftBody)
    class PhysicsSystem : public ISystem
    {
    public:
        ProcessingChannel Channel() const override { return ProcessingChannel::MAIN; }

        void OnStart      (Scene& scene, Engine& engine) override;
        void OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity) override;
        void OnUpdate     (Scene& scene, float dt) override;

    private:
        void InitRigidBody(Scene& scene, Engine& engine, ecs::Entity e);
        void InitSoftBody (Scene& scene, Engine& engine, ecs::Entity e);
    };
}
