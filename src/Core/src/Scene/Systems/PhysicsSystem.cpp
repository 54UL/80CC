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
#include <unordered_map>

namespace ettycc
{
    // -- OnStart ---------------------------------------------------------------
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

    // -- OnEntityAdded ---------------------------------------------------------
    void PhysicsSystem::OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity)
    {
        if (scene.registry_.Has<RigidBodyComponent>(entity))
            InitRigidBody(scene, engine, entity);

        if (scene.registry_.Has<SoftBodyComponent>(entity))
            InitSoftBody(scene, engine, entity);
    }

    // -- Gravity: apply previous frame's results ------------------------------
    void PhysicsSystem::ApplyGravityForces(Scene& scene)
    {
        auto& pool = scene.registry_.Pool<RigidBodyComponent>();

        for (size_t i = 0; i < bodySnap_.size(); ++i)
        {
            const glm::vec3& f = forceResults_[i];
            if (f.x == 0.f && f.y == 0.f && f.z == 0.f) continue;

            auto* rb = pool.Get(bodySnap_[i].entity);
            if (rb) rb->ApplyCentralForce(f);
        }
    }

    // -- Gravity: submit computation to ThreadRegistry pool -------------------
    void PhysicsSystem::DispatchGravityJob()
    {
        if (!engine_) return;

        forceResults_.assign(bodySnap_.size(), glm::vec3(0.f));

        gravityJobRunning_.store(true, std::memory_order_release);
        hasGravityResults_ = true;

        auto* attractors = &attractorSnap_;
        auto* bodies     = &bodySnap_;
        auto* forces     = &forceResults_;
        auto* running    = &gravityJobRunning_;

        engine_->threadRegistry_.Submit(
            [attractors, bodies, forces, running]()
            {
                const size_t n = bodies->size();
                const auto& atts = *attractors;

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

                        const float ed2 = (dist2 < att.innerRadius2)
                                         ? att.innerRadius2 : dist2;
                        net += dir * (att.strength * mass / ed2);
                    }

                    (*forces)[i] = net;
                }

                running->store(false, std::memory_order_release);
            });
    }

    // -- PlanetaryDynamics -----------------------------------------------------
    void PhysicsSystem::PlanetaryDynamics(Scene& scene)
    {
        if (hasGravityResults_ &&
            !gravityJobRunning_.load(std::memory_order_acquire))
        {
            ApplyGravityForces(scene);
            hasGravityResults_ = false;
        }

        if (!gravityJobRunning_.load(std::memory_order_acquire))
        {
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

        ProcessFusions(scene);
    }

    // -- OnUpdate --------------------------------------------------------------
    void PhysicsSystem::OnUpdate(Scene& scene, float dt)
    {
        PlanetaryDynamics(scene);

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

    // -- Spatial hash for broadphase fusion checks -----------------------------
    namespace {
        struct CellKey {
            int x, y;
            bool operator==(const CellKey& o) const { return x == o.x && y == o.y; }
        };
        struct CellKeyHash {
            size_t operator()(const CellKey& k) const {
                size_t h = std::hash<int>()(k.x);
                h ^= std::hash<int>()(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
                return h;
            }
        };
        using SpatialGrid = std::unordered_map<CellKey, std::vector<size_t>, CellKeyHash>;
    }

    // -- Planetary fusion -----------------------------------------------------
    void PhysicsSystem::ProcessFusions(Scene& scene)
    {
        constexpr float OVERLAP_FACTOR  = 0.5f;
        constexpr float FUSION_COOLDOWN = 0.5f;

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

        float maxRadius = 0.f;
        for (size_t i = 0; i < comps.size(); ++i)
        {
            auto& rb = comps[i];
            if (!rb.IsInitialized() || !rb.IsDynamic() || !rb.CanFuse())
                continue;

            const glm::vec3 h = rb.GetHalfExtents();
            const float r = (h.x + h.y) * 0.5f;
            maxRadius = std::max(maxRadius, r);
            candidates.push_back({
                entities[i], &rb,
                rb.GetPosition(), h, r,
                rb.GetMass()
            });
        }

        const size_t n = candidates.size();
        if (n < 2) return;

        const float cellSize = std::max(maxRadius * 2.0f / OVERLAP_FACTOR, 0.1f);
        const float invCell  = 1.0f / cellSize;

        SpatialGrid grid;
        grid.reserve(n);
        for (size_t i = 0; i < n; ++i)
        {
            const int cx = (int)std::floor(candidates[i].pos.x * invCell);
            const int cy = (int)std::floor(candidates[i].pos.y * invCell);
            grid[{cx, cy}].push_back(i);
        }

        struct FusionPair { size_t a; size_t b; float dist; };
        std::vector<FusionPair> pairsFound;

        std::vector<size_t> bestPartner(n, SIZE_MAX);
        std::vector<float>  bestDist(n, std::numeric_limits<float>::max());

        for (auto& [cell, indices] : grid)
        {
            static const CellKey offsets[] = {
                {0, 0}, {1, 0}, {0, 1}, {1, 1}, {-1, 1}
            };

            for (const auto& off : offsets)
            {
                const CellKey neighbour = { cell.x + off.x, cell.y + off.y };
                const auto nit = (off.x == 0 && off.y == 0)
                                 ? grid.find(cell)
                                 : grid.find(neighbour);
                if (nit == grid.end()) continue;

                const auto& nIndices = nit->second;
                const bool sameCell = (off.x == 0 && off.y == 0);

                for (size_t ai = 0; ai < indices.size(); ++ai)
                {
                    const size_t idxA = indices[ai];
                    const auto& A = candidates[idxA];

                    const size_t jStart = sameCell ? (ai + 1) : 0;
                    for (size_t bi = jStart; bi < nIndices.size(); ++bi)
                    {
                        const size_t idxB = nIndices[bi];
                        const auto& B = candidates[idxB];

                        const float threshold =
                            (A.radius + B.radius) * OVERLAP_FACTOR;
                        const glm::vec3 diff = A.pos - B.pos;
                        const float dist2 = glm::dot(diff, diff);
                        const float thresh2 = threshold * threshold;

                        if (dist2 < thresh2)
                        {
                            const float dist = std::sqrt(dist2);
                            if (dist < bestDist[idxA])
                            {
                                bestDist[idxA] = dist;
                                bestPartner[idxA] = idxB;
                            }
                            if (dist < bestDist[idxB])
                            {
                                bestDist[idxB] = dist;
                                bestPartner[idxB] = idxA;
                            }
                        }
                    }
                }
            }
        }

        for (size_t i = 0; i < n; ++i)
        {
            if (bestPartner[i] != SIZE_MAX && i < bestPartner[i])
                pairsFound.push_back({ i, bestPartner[i], bestDist[i] });
        }

        // -- Execute fusions ---------------------------------------------------
        std::vector<ecs::Entity> toRemove;
        for (const auto& pair : pairsFound)
        {
            if (candidates[pair.a].entity == ecs::NullEntity) continue;
            if (candidates[pair.b].entity == ecs::NullEntity) continue;

            const bool aWins = (candidates[pair.a].mass >= candidates[pair.b].mass);
            const size_t sIdx = aWins ? pair.a : pair.b;
            const size_t vIdx = aWins ? pair.b : pair.a;

            auto& S = candidates[sIdx];
            auto& V = candidates[vIdx];

            const float newMass = S.mass + V.mass;

            const glm::vec3 newVel =
                (S.mass * S.rb->GetLinearVelocity()
               + V.mass * V.rb->GetLinearVelocity()) / newMass;

            const float scaleFactor = std::cbrt(newMass / S.mass);
            const glm::vec3 newHalf = S.halfExtents * scaleFactor;

            S.rb->SetLinearVelocity(newVel);
            S.rb->Reinitialize(*engine_->physicsWorld_, newMass, newHalf);
            S.rb->SetFusionCooldown(FUSION_COOLDOWN);

            if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(S.entity))
                if (rn->renderable_)
                    rn->renderable_->underylingTransform.setGlobalScale(newHalf);

            S.halfExtents = newHalf;
            S.radius      = (newHalf.x + newHalf.y) * 0.5f;
            S.mass        = newMass;
            S.pos         = S.rb->GetPosition();

            toRemove.push_back(V.entity);
            V.entity = ecs::NullEntity;

            spdlog::debug("[PhysicsSystem] fusion: {} absorbed {} -- mass={:.1f}  scale={:.2f}",
                          S.entity, toRemove.back(), newMass, scaleFactor);
        }

        for (ecs::Entity victimId : toRemove)
        {
            auto* node = scene.GetNode(victimId);
            if (node && node->parent_)
                node->parent_->RemoveNode(victimId);
        }
    }

    // -- Private helpers -------------------------------------------------------
    void PhysicsSystem::InitRigidBody(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* rb   = scene.registry_.Pool<RigidBodyComponent>().Get(e);
        auto* node = scene.GetNode(e);
        if (!rb || !node || rb->IsInitialized()) return;

        const Transform* seedTransform = nullptr;
        if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(e))
            if (rn->renderable_)
                seedTransform = &rn->renderable_->underylingTransform;

        rb->InitBody(*engine.physicsWorld_, node->transform_, seedTransform);
    }

    void PhysicsSystem::InitSoftBody(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* sb   = scene.registry_.Pool<SoftBodyComponent>().Get(e);
        auto* node = scene.GetNode(e);
        if (!sb || !node || sb->IsInitialized()) return;

        sb->InitBody(*engine.physicsWorld_, engine);
        if (sb->IsInitialized())
            node->transform_.setGlobalPosition(sb->GetCentroid());
    }

} // namespace ettycc
