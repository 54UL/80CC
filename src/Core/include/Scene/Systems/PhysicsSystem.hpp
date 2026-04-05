#pragma once
#include <ECS/ISystem.hpp>
#include <ECS/Entity.hpp>
#include <vector>

namespace ettycc
{
    // ── PhysicsSystem ─────────────────────────────────────────────────────────
    // MAIN channel.
    // Owns initialization and per-frame update for:
    //   • RigidBodyComponent (Bullet btRigidBody)
    //   • SoftBodyComponent  (Bullet btSoftBody)
    //   • GravityAttractorComponent (radial force field)
    // Also handles planetary fusion: when two dynamic bodies overlap they
    // merge into one with conserved momentum and combined volume.
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

        // Checks all dynamic body pairs and fuses overlapping ones.
        void ProcessFusions(Scene& scene);

        Engine* engine_ = nullptr;
    };
}
