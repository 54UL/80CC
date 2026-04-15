#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Scene/Components/GravityAttractorComponent.hpp>
#include <Engine.hpp>
#include <Threading/ThreadRegistry.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <cmath>

namespace ettycc
{
    // ── OnStart ───────────────────────────────────────────────────────────────
    void PhysicsSystem::OnStart(Scene& scene, Engine& engine)
    {
        engine_ = &engine;

        auto& rbPool = scene.registry_.Pool<RigidBodyComponent>();
        for (size_t i = 0; i < rbPool.Size(); ++i)
            InitRigidBody(scene, engine, rbPool.Entities()[i]);

        auto& sbPool = scene.registry_.Pool<SoftBodyComponent>();
        for (size_t i = 0; i < sbPool.Size(); ++i)
            InitSoftBody(scene, engine, sbPool.Entities()[i]);
    }

    // ── OnEntityAdded ─────────────────────────────────────────────────────────
    void PhysicsSystem::OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity)
    {
        if (scene.registry_.Has<RigidBodyComponent>(entity))
            InitRigidBody(scene, engine, entity);

        if (scene.registry_.Has<SoftBodyComponent>(entity))
            InitSoftBody(scene, engine, entity);
    }

    // ── Gravity: apply previous frame's results ──────────────────────────────
    void PhysicsSystem::ApplyGravityForces(Scene& scene)
    {
        // bodySnap_ entities may have been removed by fusion last frame,
        // so we must validate via pool lookup (single hash per body).
        auto& pool = scene.registry_.Pool<RigidBodyComponent>();

        for (size_t i = 0; i < bodySnap_.size(); ++i)
        {
            const glm::vec3& f = forceResults_[i];
            if (f.x == 0.f && f.y == 0.f && f.z == 0.f) continue;

            auto* rb = pool.Get(bodySnap_[i].entity);
            if (rb) rb->ApplyCentralForce(f);
        }
    }

    // ── Gravity: submit computation to ThreadRegistry pool ───────────────────
    // Precondition: attractorSnap_ and bodySnap_ already populated.
    void PhysicsSystem::DispatchGravityJob()
    {
        if (!engine_) return;

        // Resize force buffer (reuses capacity — no alloc after warmup).
        forceResults_.assign(bodySnap_.size(), glm::vec3(0.f));

        // Hand ownership to worker.
        gravityJobRunning_.store(true, std::memory_order_release);
        hasGravityResults_ = true;

        // Pointers to member buffers — safe because main won't touch them
        // while gravityJobRunning_ == true.
        auto* attractors = &attractorSnap_;
        auto* bodies     = &bodySnap_;
        auto* forces     = &forceResults_;
        auto* running    = &gravityJobRunning_;

        engine_->threadRegistry_.Submit(
            [attractors, bodies, forces, running]()
            {
                const size_t n = bodies->size();
                const auto& atts = *attractors;

                // Linear read of bodies, linear read of attractors, linear write of forces.
                // All contiguous — cache-friendly.
                for (size_t i = 0; i < n; ++i)
                {
                    const glm::vec3& pos  = (*bodies)[i].pos;
                    const float       mass = (*bodies)[i].mass;
                    glm::vec3 net(0.f);

                    for (const auto& att : atts)
                    {
                        const glm::vec3 diff = att.center - pos;
                        const float dist2 = glm::dot(diff, diff);

                        if (dist2 > att.outerRadius2 || dist2 < 0.01f)
                            continue;

                        const float dist = std::sqrt(dist2);
                        const glm::vec3 dir = diff / dist;

                        // Clamp at inner radius (full strength, no blow-up).
                        const float ed2 = (dist2 < att.innerRadius2)
                                         ? att.innerRadius2 : dist2;
                        net += dir * (att.strength * mass / ed2);
                    }

                    (*forces)[i] = net;
                }

                running->store(false, std::memory_order_release);
            });
    }

    // ── PlanetaryDynamics ─────────────────────────────────────────────────────
    void PhysicsSystem::PlanetaryDynamics(Scene& scene)
    {
        // 1. If previous gravity job finished, apply its results.
        if (hasGravityResults_ &&
            !gravityJobRunning_.load(std::memory_order_acquire))
        {
            ApplyGravityForces(scene);
            hasGravityResults_ = false;
        }

        // 2. If worker is idle, snapshot & dispatch.
        if (!gravityJobRunning_.load(std::memory_order_acquire))
        {
            // Snapshot attractors (few — iterate directly).
            attractorSnap_.clear();
            {
                auto& aPool = scene.registry_.Pool<GravityAttractorComponent>();
                auto& comps = aPool.Components();
                for (size_t i = 0; i < comps.size(); ++i)
                {
                    auto& a = comps[i];
                    const float ir = a.GetInnerRadius();
                    const float or_ = a.GetOuterRadius();
                    attractorSnap_.push_back({ a.GetPosition(), a.GetStrength(),
                                               ir * ir, or_ * or_ });
                }
            }

            // Snapshot dynamic bodies (iterate dense array — zero hash lookups).
            bodySnap_.clear();
            {
                auto& rbPool = scene.registry_.Pool<RigidBodyComponent>();
                auto& comps    = rbPool.Components();
                auto& entities = rbPool.Entities();
                for (size_t i = 0; i < comps.size(); ++i)
                {
                    auto& rb = comps[i];
                    if (!rb.IsInitialized() || !rb.IsDynamic()) continue;
                    bodySnap_.push_back({ entities[i], rb.GetPosition(), rb.GetMass() });
                }
            }

            if (!attractorSnap_.empty() && !bodySnap_.empty())
                DispatchGravityJob();
        }

        // 3. Fusion always runs on main thread (mutates scene).
        ProcessFusions(scene);
    }

    // ── OnUpdate ──────────────────────────────────────────────────────────────
    void PhysicsSystem::OnUpdate(Scene& scene, float dt)
    {
        PlanetaryDynamics(scene);

        // Parallel pass over rigid bodies: tick cooldowns + sync transforms.
        // Each body is independent — safe to chunk across pool threads.
        // Bullet step is complete; nodeIndex_ is read-only during this phase.
        {
            auto& rbPool   = scene.registry_.Pool<RigidBodyComponent>();
            auto& comps    = rbPool.Components();
            auto& entities = rbPool.Entities();
            const size_t n = comps.size();

            if (engine_)
            {
                engine_->threadRegistry_.ParallelFor(n,
                    [&comps, &entities, &scene, dt](size_t begin, size_t end)
                    {
                        for (size_t i = begin; i < end; ++i)
                        {
                            auto& rb = comps[i];
                            rb.TickCooldown(dt);

                            if (rb.IsInitialized())
                            {
                                auto* node = scene.GetNode(entities[i]);
                                if (node) rb.SyncToTransform(node->transform_);
                            }
                        }
                    });
            }
            else
            {
                for (size_t i = 0; i < n; ++i)
                {
                    comps[i].TickCooldown(dt);
                    if (comps[i].IsInitialized())
                    {
                        auto* node = scene.GetNode(entities[i]);
                        if (node) comps[i].SyncToTransform(node->transform_);
                    }
                }
            }
        }

        // SoftBody pass (separate pool, dense iteration).
        {
            auto& sbPool   = scene.registry_.Pool<SoftBodyComponent>();
            auto& comps    = sbPool.Components();
            auto& entities = sbPool.Entities();
            for (size_t i = 0; i < comps.size(); ++i)
            {
                auto& sb = comps[i];
                if (!sb.IsInitialized()) continue;
                auto* node = scene.GetNode(entities[i]);
                if (node) sb.UpdateBody(node->transform_);
            }
        }
    }

    // ── Planetary fusion ─────────────────────────────────────────────────────
    // Snapshot ALL candidate data into a flat contiguous array ONCE, then run
    // the O(n^2) pair check over that — zero Bullet calls and zero hash
    // lookups in the inner loop.
    void PhysicsSystem::ProcessFusions(Scene& scene)
    {
        constexpr float OVERLAP_FACTOR  = 0.5f;
        constexpr float FUSION_COOLDOWN = 0.5f;

        // ── Pre-snapshot via dense array iteration (no hash lookups) ─────────
        struct FusionBody {
            ecs::Entity         entity;
            RigidBodyComponent* rb;
            glm::vec3           pos;
            glm::vec3           halfExtents;
            float               radius;
            float               mass;
        };

        auto& rbPool   = scene.registry_.Pool<RigidBodyComponent>();
        auto& comps    = rbPool.Components();
        auto& entities = rbPool.Entities();

        std::vector<FusionBody> candidates;
        candidates.reserve(comps.size());

        for (size_t i = 0; i < comps.size(); ++i)
        {
            auto& rb = comps[i];
            if (!rb.IsInitialized() || !rb.IsDynamic() || !rb.CanFuse())
                continue;

            const glm::vec3 h = rb.GetHalfExtents();
            candidates.push_back({
                entities[i], &rb,
                rb.GetPosition(), h,
                (h.x + h.y) * 0.5f,
                rb.GetMass()
            });
        }

        const size_t n = candidates.size();
        std::vector<ecs::Entity> toRemove;

        // ── Phase 1: Parallel candidate search (read-only on flat array) ─────
        // Each thread finds the best (closest) fusion partner for its range of
        // outer-loop indices.  Output: one candidate pair per outer index, or
        // {-1, -1} if none found.
        struct FusionPair { size_t a; size_t b; float dist; };

        std::vector<FusionPair> pairsFound(n, { SIZE_MAX, SIZE_MAX, 0.f });

        auto findPairs = [&candidates, n, OVERLAP_FACTOR, &pairsFound]
                         (size_t begin, size_t end)
        {
            for (size_t i = begin; i < end; ++i)
            {
                float bestDist = std::numeric_limits<float>::max();
                size_t bestJ = SIZE_MAX;

                for (size_t j = i + 1; j < n; ++j)
                {
                    const float threshold =
                        (candidates[i].radius + candidates[j].radius) * OVERLAP_FACTOR;
                    const float dist = glm::length(candidates[i].pos - candidates[j].pos);
                    if (dist < threshold && dist < bestDist)
                    {
                        bestDist = dist;
                        bestJ = j;
                    }
                }

                if (bestJ != SIZE_MAX)
                    pairsFound[i] = { i, bestJ, bestDist };
            }
        };

        if (engine_ && n > 32)
            engine_->threadRegistry_.ParallelFor(n, findPairs, 32);
        else
            findPairs(0, n);

        // ── Phase 2: Sequential fusion execution ─────────────────────────────
        // Process pairs in order; skip already-consumed entities.
        for (size_t i = 0; i < n; ++i)
        {
            if (pairsFound[i].a == SIZE_MAX) continue;
            if (candidates[i].entity == ecs::NullEntity) continue;

            const size_t j = pairsFound[i].b;
            if (candidates[j].entity == ecs::NullEntity) continue;

            const bool iWins = (candidates[i].mass >= candidates[j].mass);
            const size_t sIdx = iWins ? i : j;
            const size_t vIdx = iWins ? j : i;

            auto& S = candidates[sIdx];
            auto& V = candidates[vIdx];

            const float newMass = S.mass + V.mass;

            const glm::vec3 newVel =
                (S.mass * S.rb->GetLinearVelocity()
               + V.mass * V.rb->GetLinearVelocity()) / newMass;

            const float scaleFactor = std::cbrt(newMass / S.mass);
            const glm::vec3 newHalf = S.halfExtents * scaleFactor;

            S.rb->SetLinearVelocity(newVel);
            S.rb->Reinitialize(newMass, newHalf);
            S.rb->SetFusionCooldown(FUSION_COOLDOWN);

            if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(S.entity))
                if (rn->renderable_)
                    rn->renderable_->underylingTransform.setGlobalScale(newHalf);

            S.halfExtents = newHalf;
            S.radius      = (newHalf.x + newHalf.y) * 110.5f;
            S.mass        = newMass;
            S.pos         = S.rb->GetPosition();

            toRemove.push_back(V.entity);
            V.entity = ecs::NullEntity;

            spdlog::info("[PhysicsSystem] fusion: {} absorbed {} — mass={:.1f}  scale={:.2f}",
                         S.entity, toRemove.back(), newMass, scaleFactor);
        }

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
        auto* rb   = scene.registry_.Pool<RigidBodyComponent>().Get(e);
        auto* node = scene.GetNode(e);
        if (!rb || !node || rb->IsInitialized()) return;

        const Transform* seedTransform = nullptr;
        if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(e))
            if (rn->renderable_)
                seedTransform = &rn->renderable_->underylingTransform;

        rb->InitBody(engine.physicsWorld_.GetWorld(), node->transform_, seedTransform);
    }

    void PhysicsSystem::InitSoftBody(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* sb   = scene.registry_.Pool<SoftBodyComponent>().Get(e);
        auto* node = scene.GetNode(e);
        if (!sb || !node || sb->IsInitialized()) return;

        sb->InitBody(engine.physicsWorld_.GetSoftWorld(), engine);
        if (sb->IsInitialized())
            node->transform_.setGlobalPosition(sb->GetCentroid());
    }

} // namespace ettycc
