#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Scene/Components/GravityAttractorComponent.hpp>
#include <Engine.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <cmath>

namespace ettycc
{
    // ── OnStart ───────────────────────────────────────────────────────────────
    void PhysicsSystem::OnStart(Scene& scene, Engine& engine)
    {
        engine_ = &engine;

        for (ecs::Entity e : scene.registry_.Pool<RigidBodyComponent>().Entities())
            InitRigidBody(scene, engine, e);

        for (ecs::Entity e : scene.registry_.Pool<SoftBodyComponent>().Entities())
            InitSoftBody(scene, engine, e);
    }

    // ── OnEntityAdded ─────────────────────────────────────────────────────────
    void PhysicsSystem::OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity)
    {
        if (scene.registry_.Has<RigidBodyComponent>(entity))
            InitRigidBody(scene, engine, entity);

        if (scene.registry_.Has<SoftBodyComponent>(entity))
            InitSoftBody(scene, engine, entity);
    }

    // ── OnUpdate ──────────────────────────────────────────────────────────────
    void PhysicsSystem::OnUpdate(Scene& scene, float dt)
    {
        // Tick fusion cooldowns.
        for (ecs::Entity e : scene.registry_.Pool<RigidBodyComponent>().Entities())
        {
            auto* rb = scene.registry_.Get<RigidBodyComponent>(e);
            if (rb) rb->TickCooldown(dt);
        }

        // Gravity attractors: apply radial force to every dynamic rigid body.
        for (ecs::Entity ae : scene.registry_.Pool<GravityAttractorComponent>().Entities())
        {
            auto* attractor = scene.registry_.Get<GravityAttractorComponent>(ae);
            if (!attractor) continue;

            const glm::vec3 center   = attractor->GetPosition();
            const float     strength = attractor->GetStrength();

            for (ecs::Entity re : scene.registry_.Pool<RigidBodyComponent>().Entities())
            {
                auto* rb = scene.registry_.Get<RigidBodyComponent>(re);
                if (!rb || !rb->IsInitialized() || !rb->IsDynamic()) continue;

                const glm::vec3 pos  = rb->GetPosition();
                const glm::vec3 diff = center - pos;
                const float dist2    = glm::dot(diff, diff);
                if (dist2 < 0.01f) continue;   // avoid singularity at center

                const float dist = std::sqrt(dist2);
                const glm::vec3 dir = diff / dist;

                // F = strength * mass / dist^2  (inverse-square, like real gravity)
                const float mag = strength * rb->GetMass() / dist2;
                rb->ApplyCentralForce(dir * mag);
            }
        }

        // Planetary fusion: merge overlapping dynamic bodies.
        ProcessFusions(scene);

        // RigidBody: pull Bullet simulation result into the node transform.
        for (ecs::Entity e : scene.registry_.Pool<RigidBodyComponent>().Entities())
        {
            auto* rb   = scene.registry_.Get<RigidBodyComponent>(e);
            auto* node = scene.GetNode(e);
            if (!rb || !node || !rb->IsInitialized()) continue;
            rb->SyncToTransform(node->transform_);
        }

        // SoftBody: constrain to 2D plane and update node centroid.
        for (ecs::Entity e : scene.registry_.Pool<SoftBodyComponent>().Entities())
        {
            auto* sb   = scene.registry_.Get<SoftBodyComponent>(e);
            auto* node = scene.GetNode(e);
            if (!sb || !node || !sb->IsInitialized()) continue;
            sb->UpdateBody(node->transform_);
        }
    }

    // ── Planetary fusion ─────────────────────────────────────────────────────
    // Two dynamic rigid bodies fuse when their centers are closer than half the
    // sum of their radii (significant penetration, not just touching).
    // After fusion the survivor gets a cooldown so it can't chain-absorb every
    // neighbour in a single burst.
    void PhysicsSystem::ProcessFusions(Scene& scene)
    {
        constexpr float OVERLAP_FACTOR  = 0.5f;   // require 50 % penetration
        constexpr float FUSION_COOLDOWN = 0.5f;    // seconds before the survivor can fuse again

        const auto entities = scene.registry_.Pool<RigidBodyComponent>().Entities();  // copy
        const size_t n = entities.size();

        // Entities that participated in a fusion this frame (both survivor and victim).
        std::vector<ecs::Entity> involved;
        std::vector<ecs::Entity> toRemove;

        auto isInvolved = [&](ecs::Entity e) {
            for (ecs::Entity x : involved) if (x == e) return true;
            return false;
        };

        for (size_t i = 0; i < n; ++i)
        {
            if (isInvolved(entities[i])) continue;

            auto* rbA = scene.registry_.Get<RigidBodyComponent>(entities[i]);
            if (!rbA || !rbA->IsInitialized() || !rbA->IsDynamic() || !rbA->CanFuse()) continue;

            for (size_t j = i + 1; j < n; ++j)
            {
                if (isInvolved(entities[j])) continue;

                auto* rbB = scene.registry_.Get<RigidBodyComponent>(entities[j]);
                if (!rbB || !rbB->IsInitialized() || !rbB->IsDynamic() || !rbB->CanFuse()) continue;

                const glm::vec3 posA = rbA->GetPosition();
                const glm::vec3 posB = rbB->GetPosition();
                const float dist = glm::length(posA - posB);

                // Average half-extent as radius (2D, ignore Z).
                const glm::vec3 hA = rbA->GetHalfExtents();
                const glm::vec3 hB = rbB->GetHalfExtents();
                const float radiusA = (hA.x + hA.y) * 0.5f;
                const float radiusB = (hB.x + hB.y) * 0.5f;

                // Require significant overlap, not just touching.
                const float threshold = (radiusA + radiusB) * OVERLAP_FACTOR;
                if (dist >= threshold) continue;

                // ── Fuse: heavier body survives ──────────────────────────────
                const float massA = rbA->GetMass();
                const float massB = rbB->GetMass();
                const bool aIsSurvivor = (massA >= massB);

                auto*             survivor   = aIsSurvivor ? rbA : rbB;
                const ecs::Entity survivorId = aIsSurvivor ? entities[i] : entities[j];
                const ecs::Entity victimId   = aIsSurvivor ? entities[j] : entities[i];

                const float mS = survivor->GetMass();
                const float mV = (aIsSurvivor ? rbB : rbA)->GetMass();
                const float newMass = mS + mV;

                // Conserve momentum.
                const glm::vec3 velS = survivor->GetLinearVelocity();
                const glm::vec3 velV = (aIsSurvivor ? rbB : rbA)->GetLinearVelocity();
                const glm::vec3 newVel = (mS * velS + mV * velV) / newMass;

                // Grow: scale up so combined volume = volS + volV.
                const glm::vec3 hS = survivor->GetHalfExtents();
                const float scaleFactor = std::cbrt(newMass / mS);
                const glm::vec3 newHalf = hS * scaleFactor;

                survivor->SetLinearVelocity(newVel);
                survivor->Reinitialize(newMass, newHalf);
                survivor->SetFusionCooldown(FUSION_COOLDOWN);

                // Update the renderable scale.
                if (auto* rn = scene.registry_.Get<RenderableNode>(survivorId))
                    if (rn->renderable_)
                        rn->renderable_->underylingTransform.setGlobalScale(newHalf);

                // Both entities are done for this frame.
                involved.push_back(survivorId);
                involved.push_back(victimId);
                toRemove.push_back(victimId);

                spdlog::info("[PhysicsSystem] fusion: {} absorbed {} — mass={:.1f}  scale={:.2f}",
                             survivorId, victimId, newMass, scaleFactor);

                // Pool may have relocated — re-fetch and stop inner loop.
                rbA = scene.registry_.Get<RigidBodyComponent>(entities[i]);
                break;
            }
        }

        // Deferred removal — destroy absorbed entities after iteration is done.
        for (ecs::Entity victimId : toRemove)
        {
            auto* node = scene.GetNode(victimId);
            if (node && node->parent_)
                node->parent_->RemoveNode(victimId);
        }
    }

    // ── Private helpers ───────────────────────────────────────────────────────
    void PhysicsSystem::InitRigidBody(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* rb   = scene.registry_.Get<RigidBodyComponent>(e);
        auto* node = scene.GetNode(e);
        if (!rb || !node || rb->IsInitialized()) return;

        // Seed transform from sibling RenderableNode if present.
        const Transform* seedTransform = nullptr;
        if (auto* rn = scene.registry_.Get<RenderableNode>(e))
            if (rn->renderable_)
                seedTransform = &rn->renderable_->underylingTransform;

        rb->InitBody(engine.physicsWorld_.GetWorld(), node->transform_, seedTransform);
    }

    void PhysicsSystem::InitSoftBody(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* sb   = scene.registry_.Get<SoftBodyComponent>(e);
        auto* node = scene.GetNode(e);
        if (!sb || !node || sb->IsInitialized()) return;

        sb->InitBody(engine.physicsWorld_.GetSoftWorld(), engine);
        if (sb->IsInitialized())
        {
            // Sync the node transform to the soft body's actual centroid so that
            // UpdateBody's delta logic (nodePos − lastTrackedCentroid_) starts at
            // zero instead of producing a huge false offset that yanks every soft
            // body to the origin on the first frame.
            node->transform_.setGlobalPosition(sb->GetCentroid());
        }
    }

} // namespace ettycc
