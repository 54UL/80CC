#pragma once
#include <ECS/ISystem.hpp>
#include <ECS/Entity.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <atomic>

namespace ettycc
{
    class RigidBodyComponent;

    // ── PhysicsSystem ─────────────────────────────────────────────────────────
    // MAIN channel.
    // Owns initialization and per-frame update for:
    //   * RigidBodyComponent (Bullet btRigidBody)
    //   * SoftBodyComponent  (Bullet btSoftBody)
    //   * GravityAttractorComponent (radial force field)
    // Also handles planetary fusion: when two dynamic bodies overlap they
    // merge into one with conserved momentum and combined volume.
    //
    // Gravity computation is offloaded to the ThreadRegistry pool.
    // Data flows through flat contiguous buffers (no queues) with an
    // atomic flag handoff — one frame of latency, zero main-thread stalls.
    class PhysicsSystem : public ISystem
    {
    public:
        ProcessingChannel Channel() const override { return ProcessingChannel::MAIN; }

        void OnStart      (Scene& scene, Engine& engine) override;
        void OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity) override;
        void OnUpdate     (Scene& scene, float dt) override;

        void PlanetaryDynamics(Scene& scene);

    private:
        void InitRigidBody(Scene& scene, Engine& engine, ecs::Entity e);
        void InitSoftBody (Scene& scene, Engine& engine, ecs::Entity e);
        void ProcessFusions(Scene& scene);

        // ── Async gravity (flat buffers, no queue) ────────────────────────────
        // Ownership contract:
        //   gravityJobRunning_ == false  → main thread owns all buffers
        //   gravityJobRunning_ == true   → worker owns bodySnap_ + forceResults_
        //                                  (main must not touch them)

        struct AttractorSnapshot {
            glm::vec3 center;
            float     strength;
            float     innerRadius2;
            float     outerRadius2;
        };

        struct BodySnapshot {
            ecs::Entity entity;     // for safe lookup at apply time
            glm::vec3   pos;
            float       mass;
        };

        std::vector<AttractorSnapshot> attractorSnap_;
        std::vector<BodySnapshot>      bodySnap_;
        std::vector<glm::vec3>         forceResults_;  // parallel to bodySnap_

        std::atomic<bool> gravityJobRunning_{false};
        bool              hasGravityResults_{false};    // main-thread only

        void ApplyGravityForces(Scene& scene);
        void DispatchGravityJob();

        Engine* engine_ = nullptr;
    };
}
