#pragma once
#include <ECS/ISystem.hpp>
#include <ECS/Entity.hpp>
#include <glm/glm.hpp>
#include "GravityConfig.hpp"
#include <vector>
#include <atomic>
#include <cstdint>

namespace ettycc { class Engine; class Sprite; }

namespace gravity
{
    // Collision command: value-only snapshot produced during the read-only
    // broadphase. No raw pointers -- safe across pool reallocations.
    struct CollisionCmd
    {
        CollisionOutcome outcome;
        ettycc::ecs::Entity entityA, entityB;
        glm::vec3   posA, posB;
        glm::vec3   velA, velB;
        glm::vec3   halfA, halfB;
        float       massA, massB;
        float       radiusA, radiusB;
    };

    // Game-specific physics: gravity attraction, N-body dynamics, collision
    // outcomes (fusion/fracture/shatter), rock pool management.
    //
    // Registered by GravityModule::OnStart; removed on OnDestroy.
    // When this system is absent, Core PhysicsSystem does plain body stepping.
    class GravityDynamicsSystem : public ettycc::ISystem
    {
    public:
        ettycc::ProcessingChannel Channel() const override { return ettycc::ProcessingChannel::MAIN; }

        void OnStart      (ettycc::Scene& scene, ettycc::Engine& engine) override;
        void OnEntityAdded(ettycc::Scene& scene, ettycc::Engine& engine,
                           ettycc::ecs::Entity entity) override;
        void OnUpdate     (ettycc::Scene& scene, float dt) override;

        // Configuration (set from GravitySceneConfig or editor)
        float fusionOverlap_     = 1.5f;
        float fusionCooldown_    = 0.2f;
        bool  nBodyEnabled_      = false;
        float nBodyG_            = 1.0f;
        float nBodySoftening_    = 0.1f;
        float qFusion_           = 5.0f;
        float qShatter_          = 50.0f;
        float rockRoughness_     = 0.3f;
        int   shatterFragments_  = 6;
        int   maxCollisionsPerFrame_ = 4;

        void ApplyConfig(const GravitySceneConfig& cfg);

    private:
        void PlanetaryDynamics(ettycc::Scene& scene);
        void ProcessCollisions(ettycc::Scene& scene);
        void DetectEscapingBodies(ettycc::Scene& scene);

        // Collision outcome classification
        CollisionOutcome ClassifyCollision(float massA, float massB,
                                           const glm::vec3& velA, const glm::vec3& velB) const;

        // Collision handlers
        void ExecuteFusion  (ettycc::Scene& scene, const CollisionCmd& cmd, std::vector<ettycc::ecs::Entity>& toRemove);
        void ExecuteFracture(ettycc::Scene& scene, const CollisionCmd& cmd);
        void ExecuteShatter (ettycc::Scene& scene, const CollisionCmd& cmd, std::vector<ettycc::ecs::Entity>& toRemove);

        // Rock pool: recycled dormant entities for fragments
        struct PooledRock {
            ettycc::ecs::Entity entity = 0;
            bool                active = false;
        };
        std::vector<PooledRock> rockPool_;
        uint32_t rockSeedCounter_ = 1;

        ettycc::ecs::Entity AcquirePooledRock(ettycc::Scene& scene, float mass,
                                               glm::vec3 halfExt, glm::vec3 pos);
        void ReleasePooledRock(ettycc::Scene& scene, ettycc::ecs::Entity entity);

        // Async gravity (flat buffers, no queue)
        struct AttractorSnapshot {
            glm::vec3 center;
            float     strength;
            float     innerRadius2;
            float     outerRadius2;
        };

        struct BodySnapshot {
            ettycc::ecs::Entity entity;
            glm::vec3   pos;
            float       mass;
        };

        std::vector<AttractorSnapshot> attractorSnap_;
        std::vector<BodySnapshot>      bodySnap_;
        std::vector<glm::vec3>         forceResults_;

        std::atomic<bool> gravityJobRunning_{false};
        bool              hasGravityResults_{false};

        void ApplyGravityForces(ettycc::Scene& scene);
        void DispatchGravityJob();

        ettycc::Engine* engine_ = nullptr;
    };

} // namespace gravity
