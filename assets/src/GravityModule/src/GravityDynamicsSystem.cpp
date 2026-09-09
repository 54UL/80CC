#include "GravityDynamicsSystem.hpp"
#include "FusionDataComponent.hpp"

#include <Scene/Scene.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/GravityAttractorComponent.hpp>
#include <Scene/Systems/PhysicsSystem.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Engine.hpp>
#include <Threading/ThreadRegistry.hpp>
#include <Math/Constants.hpp>
#include <Math/Utils.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace gravity
{
    using namespace ettycc;

    // -- Spatial hash for broadphase collision checks ---------------------------
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

    struct BroadBody {
        ecs::Entity entity;
        glm::vec3   pos;
        glm::vec3   vel;
        glm::vec3   halfExtents;
        float       radius;
        float       mass;
    };

    // -- OnStart ---------------------------------------------------------------
    void GravityDynamicsSystem::OnStart(Scene& scene, Engine& engine)
    {
        engine_ = &engine;
    }

    void GravityDynamicsSystem::OnEntityAdded(Scene& /*scene*/, Engine& /*engine*/,
                                               ecs::Entity /*entity*/)
    {
        // FusionDataComponent is added alongside RigidBodyComponent by scene setup
    }

    void GravityDynamicsSystem::ApplyConfig(const GravitySceneConfig& cfg)
    {
        fusionOverlap_    = cfg.fusionOverlap;
        fusionCooldown_   = cfg.fusionCooldown;
        nBodyEnabled_     = cfg.nBodyEnabled;
        nBodyG_           = cfg.nBodyG;
        nBodySoftening_   = cfg.nBodySoftening;
        qFusion_          = cfg.qFusion;
        qShatter_         = cfg.qShatter;
        rockRoughness_    = cfg.rockRoughness;
        shatterFragments_ = cfg.shatterFragments;
    }

    // -- OnUpdate --------------------------------------------------------------
    void GravityDynamicsSystem::OnUpdate(Scene& scene, float dt)
    {
        // Tick fusion cooldowns
        if (auto* pool = scene.registry_.TryGetPool<FusionDataComponent>())
        {
            auto& comps = pool->Components();
            for (size_t i = 0; i < comps.size(); ++i)
                comps[i].Tick(dt);
        }

        PlanetaryDynamics(scene);
    }

    // -- Gravity: apply previous frame's results ------------------------------
    void GravityDynamicsSystem::ApplyGravityForces(Scene& scene)
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
    void GravityDynamicsSystem::DispatchGravityJob()
    {
        if (!engine_) return;

        forceResults_.assign(bodySnap_.size(), glm::vec3(0.f));

        gravityJobRunning_.store(true, std::memory_order_release);
        hasGravityResults_ = true;

        auto* attractors = &attractorSnap_;
        auto* bodies     = &bodySnap_;
        auto* forces     = &forceResults_;
        auto* running    = &gravityJobRunning_;
        const bool  doNBody    = nBodyEnabled_;
        const float G          = nBodyG_;
        const float softening2 = nBodySoftening_ * nBodySoftening_;

        engine_->threadRegistry_.Submit(
            [attractors, bodies, forces, running, doNBody, G, softening2]()
            {
                const size_t n = bodies->size();
                const auto& atts = *attractors;

                // -- Attractor forces (fixed gravity wells) --------------------
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

                        const float dist = glm::sqrt(dist2);
                        const glm::vec3 dir = diff / dist;

                        const float ed2 = (dist2 < att.innerRadius2)
                                         ? att.innerRadius2 : dist2;
                        net += dir * (att.strength * mass / ed2);
                    }

                    (*forces)[i] = net;
                }

                // -- N-body: body-to-body gravitational attraction -------------
                if (doNBody && n > 1)
                {
                    for (size_t i = 0; i < n - 1; ++i)
                    {
                        const glm::vec3& pi = (*bodies)[i].pos;
                        const float       mi = (*bodies)[i].mass;

                        for (size_t j = i + 1; j < n; ++j)
                        {
                            const glm::vec3 diff = (*bodies)[j].pos - pi;
                            const float dist2 = glm::dot(diff, diff) + softening2;
                            const float dist  = glm::sqrt(dist2);
                            const float invD3 = 1.0f / (dist * dist2);

                            const float mj = (*bodies)[j].mass;
                            const glm::vec3 fij = diff * (G * mi * mj * invD3);

                            (*forces)[i] += fij;
                            (*forces)[j] -= fij;
                        }
                    }
                }

                running->store(false, std::memory_order_release);
            });
    }

    // -- PlanetaryDynamics -----------------------------------------------------
    void GravityDynamicsSystem::PlanetaryDynamics(Scene& scene)
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
            if (auto* aPool = scene.registry_.TryGetPool<GravityAttractorComponent>())
            {
                auto& comps = aPool->Components();
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

            const bool hasAttractors = !attractorSnap_.empty();
            const bool hasNBody     = nBodyEnabled_ && bodySnap_.size() > 1;
            if (!bodySnap_.empty() && (hasAttractors || hasNBody))
                DispatchGravityJob();
        }

        DetectEscapingBodies(scene);
        ProcessCollisions(scene);
    }

    // -- Collision outcome classification ------------------------------------
    CollisionOutcome GravityDynamicsSystem::ClassifyCollision(float massA, float massB,
                                                              const glm::vec3& velA,
                                                              const glm::vec3& velB) const
    {
        const glm::vec3 relVel = velA - velB;
        const float vRel2 = glm::dot(relVel, relVel);
        const float mTotal = massA + massB;
        if (mTotal < 1e-6f) return CollisionOutcome::Fusion;

        const float mReduced = (massA * massB) / mTotal;
        const float Q = 0.5f * mReduced * vRel2 / mTotal;

        if (Q < qFusion_)  return CollisionOutcome::Fusion;
        if (Q < qShatter_) return CollisionOutcome::Fracture;
        return CollisionOutcome::Shatter;
    }

    // -- Rock Pool: acquire a recycled or new procedural entity ----------------
    ecs::Entity GravityDynamicsSystem::AcquirePooledRock(Scene& scene, float mass,
                                                          glm::vec3 halfExt, glm::vec3 pos)
    {
        int segs = glm::clamp(static_cast<int>(glm::sqrt(mass) * 4.f), 8, 16);

        // Try to reuse a dormant pooled rock
        for (auto& slot : rockPool_)
        {
            if (!slot.active)
            {
                slot.active = true;

                auto* rb = scene.registry_.Pool<RigidBodyComponent>().Get(slot.entity);
                auto* rn = scene.registry_.Pool<RenderableNode>().Get(slot.entity);
                if (rb && rn && rn->renderable_)
                {
                    rn->renderable_->transform().setGlobalPosition(pos);
                    rn->renderable_->transform().setGlobalScale(halfExt);

                    if (auto* sprite = dynamic_cast<Sprite*>(rn->renderable_.get()))
                    {
                        auto shape = SpriteShape::MakeRock(segs, rockRoughness_, rockSeedCounter_++);
                        sprite->SetShape(shape);
                        auto bv = ExtractBoundaryVerts(sprite);
                        rb->Reinitialize(*engine_->physicsWorld_, mass, halfExt,
                                         bv.empty() ? nullptr : &bv);
                    }
                    else
                    {
                        rb->Reinitialize(*engine_->physicsWorld_, mass, halfExt);
                    }
                    return slot.entity;
                }
            }
        }

        // No pooled rock available -- create a new procedural entity
        static int fragIdx = 0;
        auto rockShape = SpriteShape::MakeRock(segs, rockRoughness_, rockSeedCounter_++);
        auto fragSprite = Sprite::MakeWithShader(rockShape, "rock_procedural");
        fragSprite->underylingTransform.setGlobalPosition(pos);
        fragSprite->underylingTransform.setGlobalScale(halfExt);

        auto fragNode = std::make_shared<SceneNode>("frag-" + std::to_string(fragIdx++));
        scene.registry_.Add<RenderableNode>(fragNode->GetId(), RenderableNode{fragSprite});
        scene.registry_.Add<RigidBodyComponent>(
            fragNode->GetId(), RigidBodyComponent{mass, halfExt, pos});
        scene.registry_.Add<FusionDataComponent>(fragNode->GetId(), FusionDataComponent{});

        if (scene.root_node_)
            scene.root_node_->AddChild(fragNode);

        rockPool_.push_back({ fragNode->GetId(), true });
        return fragNode->GetId();
    }

    void GravityDynamicsSystem::ReleasePooledRock(Scene& scene, ecs::Entity entity)
    {
        for (auto& slot : rockPool_)
        {
            if (slot.entity == entity)
            {
                slot.active = false;

                if (auto* rb = scene.registry_.Pool<RigidBodyComponent>().Get(entity))
                    rb->ReleaseBody();

                if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(entity))
                {
                    if (rn->renderable_)
                        engine_->renderEngine_.RemoveRenderable(rn->renderable_);
                }
                return;
            }
        }

        // Not pooled -- full removal
        if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(entity))
            if (rn->renderable_)
                engine_->renderEngine_.RemoveRenderable(rn->renderable_);

        if (auto* rb = scene.registry_.Pool<RigidBodyComponent>().Get(entity))
            rb->ReleaseBody();

        auto* node = scene.GetNode(entity);
        if (node && node->parent_)
            node->parent_->RemoveNode(entity);
    }

    // -- Execute Fusion --------------------------------------------------------
    void GravityDynamicsSystem::ExecuteFusion(Scene& scene, const CollisionCmd& cmd,
                                              std::vector<ecs::Entity>& toRemove)
    {
        const bool aWins       = (cmd.massA >= cmd.massB);
        const ecs::Entity sEnt = aWins ? cmd.entityA : cmd.entityB;
        const ecs::Entity vEnt = aWins ? cmd.entityB : cmd.entityA;
        const glm::vec3 sPos   = aWins ? cmd.posA    : cmd.posB;
        const glm::vec3 vPos   = aWins ? cmd.posB    : cmd.posA;
        const glm::vec3 sVel   = aWins ? cmd.velA    : cmd.velB;
        const glm::vec3 vVel   = aWins ? cmd.velB    : cmd.velA;
        const glm::vec3 sHalf  = aWins ? cmd.halfA   : cmd.halfB;
        const float sMass      = aWins ? cmd.massA   : cmd.massB;
        const float vMass      = aWins ? cmd.massB   : cmd.massA;

        auto* sRb = scene.registry_.Pool<RigidBodyComponent>().Get(sEnt);
        if (!sRb || !sRb->IsInitialized()) return;

        constexpr float kAbsorbFraction = 0.80f;
        constexpr float kScatterFraction = 1.f - kAbsorbFraction;
        const float absorbedMass = vMass * kAbsorbFraction;
        const float scatterMass  = vMass * kScatterFraction;
        const float newMass = sMass + absorbedMass;

        const glm::vec3 newVel = (sMass * sVel + absorbedMass * vVel) / newMass;

        const float scaleFactor = math::MassScaleFactor(newMass, sMass);
        const glm::vec3 newHalf = sHalf * scaleFactor;

        // Grow survivor's shape toward the impact point
        auto* rnS = scene.registry_.Pool<RenderableNode>().Get(sEnt);
        if (rnS && rnS->renderable_)
        {
            auto* spriteS = dynamic_cast<Sprite*>(rnS->renderable_.get());
            if (spriteS)
            {
                SpriteShape shape = spriteS->GetShape();
                if (shape.grid_.res > 0)
                {
                    const float sX = math::SafeScale(sHalf.x);
                    const float sY = math::SafeScale(sHalf.y);
                    const glm::vec3 diff = vPos - sPos;
                    glm::vec2 impactLocal = { diff.x / sX, diff.y / sY };

                    float len = glm::length(impactLocal);
                    if (len > 0.95f) impactLocal *= 0.95f / len;

                    int totalCells = shape.grid_.res * shape.grid_.res;
                    int currentOccupied = shape.grid_.OccupiedCount();
                    float massRatio = absorbedMass / glm::max(sMass, 0.01f);
                    int cellsToAdd = glm::max(1, static_cast<int>(
                        static_cast<float>(currentOccupied) * massRatio));
                    cellsToAdd = glm::min(cellsToAdd, totalCells - currentOccupied);

                    shape.grid_.GrowToward(impactLocal, cellsToAdd);

                    SpriteShape rebuilt = shape.grid_.BuildMesh();
                    rebuilt.preset = SpriteShape::Preset::Custom;
                    rebuilt.name   = "Fused";
                    rebuilt.grid_  = std::move(shape.grid_);
                    spriteS->SetShape(rebuilt);
                }
            }
        }

        sRb->SetLinearVelocity(newVel);
        {
            auto* spriteS_ptr = rnS ? dynamic_cast<Sprite*>(rnS->renderable_.get()) : nullptr;
            auto bv = ExtractBoundaryVerts(spriteS_ptr);
            sRb->Reinitialize(*engine_->physicsWorld_, newMass, newHalf,
                               bv.empty() ? nullptr : &bv);
        }

        if (auto* fd = scene.registry_.Get<FusionDataComponent>(sEnt))
            fd->SetCooldown(glm::max(fusionCooldown_, 0.5f));

        if (rnS && rnS->renderable_)
            rnS->renderable_->transform().setGlobalScale(newHalf);

        // Scatter fragments
        const glm::vec3 impactPoint = (sPos + vPos) * 0.5f;
        const int fragCount = glm::clamp(static_cast<int>(scatterMass / 0.05f), 2, 6);
        const float fragMass = scatterMass / static_cast<float>(fragCount);
        const float fragRadius = glm::sqrt(fragMass) * 0.4f;
        const glm::vec3 fragHalf = { glm::max(fragRadius, 0.02f),
                                     glm::max(fragRadius, 0.02f), 0.02f };

        const float angleStep = math::kTwoPi / static_cast<float>(fragCount);

        for (int i = 0; i < fragCount; ++i)
        {
            float angle = angleStep * static_cast<float>(i)
                        + static_cast<float>(rockSeedCounter_ & 0xFF) * 0.01f;
            float spreadDist = (cmd.radiusA + cmd.radiusB) * 0.25f;

            glm::vec3 fragPos = impactPoint + glm::vec3{
                glm::cos(angle) * spreadDist,
                glm::sin(angle) * spreadDist, 0.f };

            ecs::Entity fragEntity = AcquirePooledRock(scene, fragMass, fragHalf, fragPos);

            auto* fragRb = scene.registry_.Pool<RigidBodyComponent>().Get(fragEntity);
            if (fragRb && fragRb->IsInitialized())
            {
                glm::vec3 radial = { glm::cos(angle), glm::sin(angle), 0.f };
                glm::vec3 fragVel = vVel * 0.3f + radial * glm::length(vVel) * 0.5f;
                fragRb->SetLinearVelocity(fragVel);
            }
            if (auto* fd = scene.registry_.Get<FusionDataComponent>(fragEntity))
                fd->SetCooldown(3.0f);
        }

        toRemove.push_back(vEnt);

        spdlog::info("[GravityDynamics] fusion: {} absorbed {} -- mass={:.1f} (+{} debris)",
                     sEnt, vEnt, newMass, fragCount);
    }

    // -- Execute Fracture ------------------------------------------------------
    void GravityDynamicsSystem::ExecuteFracture(Scene& scene, const CollisionCmd& cmd)
    {
        const bool aIsBigger    = (cmd.massA >= cmd.massB);
        const ecs::Entity bigEnt   = aIsBigger ? cmd.entityA : cmd.entityB;
        const ecs::Entity smallEnt = aIsBigger ? cmd.entityB : cmd.entityA;
        const glm::vec3 bigPos     = aIsBigger ? cmd.posA    : cmd.posB;
        const glm::vec3 smallPos   = aIsBigger ? cmd.posB    : cmd.posA;
        const glm::vec3 bigVel     = aIsBigger ? cmd.velA    : cmd.velB;
        const glm::vec3 bigHalf    = aIsBigger ? cmd.halfA   : cmd.halfB;
        const float bigMass        = aIsBigger ? cmd.massA   : cmd.massB;
        const float smallMass      = aIsBigger ? cmd.massB   : cmd.massA;
        const glm::vec3 relVel     = cmd.velA - cmd.velB;

        auto* bigRb   = scene.registry_.Pool<RigidBodyComponent>().Get(bigEnt);
        auto* smallRb = scene.registry_.Pool<RigidBodyComponent>().Get(smallEnt);
        if (!bigRb || !bigRb->IsInitialized() || !smallRb || !smallRb->IsInitialized()) return;

        auto* rn = scene.registry_.Pool<RenderableNode>().Get(bigEnt);
        if (!rn || !rn->renderable_) return;

        auto* sprite = dynamic_cast<Sprite*>(rn->renderable_.get());
        if (!sprite) return;

        const glm::vec3 impactDir3 = glm::normalize(smallPos - bigPos);
        glm::vec2 impactDir = { impactDir3.x, impactDir3.y };
        if (glm::length(impactDir) < 1e-6f) impactDir = { 1.f, 0.f };
        impactDir = glm::normalize(impactDir);

        const float scaleX = (glm::abs(bigHalf.x) > 1e-6f) ? bigHalf.x : 1.f;
        const float scaleY = (glm::abs(bigHalf.y) > 1e-6f) ? bigHalf.y : 1.f;
        glm::vec2 localLinePoint = impactDir * 0.7f;
        glm::vec2 localNormal = impactDir;

        auto sliceResult = SliceSpriteShape(sprite->GetShape(), localLinePoint, localNormal);

        if (sliceResult.sideA.indices.empty() || sliceResult.sideB.indices.empty())
        {
            const glm::vec3 bounce = glm::normalize(relVel) * 2.f;
            smallRb->SetLinearVelocity(smallRb->GetLinearVelocity() - bounce);
            bigRb->SetLinearVelocity(bigRb->GetLinearVelocity() + bounce * (smallMass / bigMass));
            return;
        }

        const float areaChip = sliceResult.sideA.ComputeArea();
        const float areaMain = sliceResult.sideB.ComputeArea();
        const float totalArea = areaChip + areaMain;
        if (totalArea < 1e-6f) return;

        const float chipMass = bigMass * (areaChip / totalArea);
        const float mainMass = bigMass * (areaMain / totalArea);

        sliceResult.sideB.RecomputeRadialUVs();
        sprite->SetShape(sliceResult.sideB);
        const float mainScale = math::MassScaleFactor(mainMass, bigMass);
        const glm::vec3 mainHalf = bigHalf * mainScale;
        {
            auto bv = ExtractBoundaryVerts(sprite);
            bigRb->Reinitialize(*engine_->physicsWorld_, mainMass, mainHalf,
                                 bv.empty() ? nullptr : &bv);
        }
        if (auto* fd = scene.registry_.Get<FusionDataComponent>(bigEnt))
            fd->SetCooldown(glm::max(fusionCooldown_, 0.5f));
        if (rn->renderable_)
            rn->renderable_->transform().setGlobalScale(mainHalf);

        glm::vec2 chipCentroid = sliceResult.sideA.ComputeCentroid();
        glm::vec3 chipPos = { bigPos.x + chipCentroid.x * scaleX,
                              bigPos.y + chipCentroid.y * scaleY, 0.f };
        const float chipRadius = glm::sqrt(chipMass) * 0.5f;
        glm::vec3 chipHalf = { glm::max(chipRadius, 0.02f), glm::max(chipRadius, 0.02f), 0.02f };

        ecs::Entity chipEntity = AcquirePooledRock(scene, chipMass, chipHalf, chipPos);

        // Fresh lookups after AcquirePooledRock
        bigRb   = scene.registry_.Pool<RigidBodyComponent>().Get(bigEnt);
        smallRb = scene.registry_.Pool<RigidBodyComponent>().Get(smallEnt);
        if (!bigRb || !smallRb) return;

        auto* chipRb = scene.registry_.Pool<RigidBodyComponent>().Get(chipEntity);
        if (chipRb && chipRb->IsInitialized())
        {
            glm::vec3 chipVel = bigRb->GetLinearVelocity() + impactDir3 * glm::length(relVel) * 0.5f;
            chipRb->SetLinearVelocity(chipVel);
        }
        if (auto* fd = scene.registry_.Get<FusionDataComponent>(chipEntity))
            fd->SetCooldown(1.0f);

        smallRb->SetLinearVelocity(smallRb->GetLinearVelocity() * -0.5f);
        if (auto* fd = scene.registry_.Get<FusionDataComponent>(smallEnt))
            fd->SetCooldown(glm::max(fusionCooldown_, 0.5f));

        spdlog::info("[GravityDynamics] fracture: {} chipped by {} -- chipMass={:.2f}",
                     bigEnt, smallEnt, chipMass);
    }

    // -- Execute Shatter -------------------------------------------------------
    void GravityDynamicsSystem::ExecuteShatter(Scene& scene, const CollisionCmd& cmd,
                                                std::vector<ecs::Entity>& toRemove)
    {
        const glm::vec3 impactPoint = (cmd.posA * cmd.massB + cmd.posB * cmd.massA)
                                    / (cmd.massA + cmd.massB);
        const glm::vec3 relVel = cmd.velA - cmd.velB;
        const float speed = glm::length(relVel);

        const glm::vec3 totalMomentum = cmd.massA * cmd.velA + cmd.massB * cmd.velB;
        const float totalMass = cmd.massA + cmd.massB;

        toRemove.push_back(cmd.entityA);
        toRemove.push_back(cmd.entityB);

        const int fragCount = shatterFragments_;
        const float fragMass = totalMass / static_cast<float>(fragCount);
        const float fragRadius = glm::sqrt(fragMass) * 0.5f;
        const glm::vec3 fragHalf = { glm::max(fragRadius, 0.02f),
                                     glm::max(fragRadius, 0.02f), 0.02f };

        const glm::vec3 comVel = totalMomentum / totalMass;

        const float angleStep = math::kTwoPi / static_cast<float>(fragCount);
        for (int i = 0; i < fragCount; ++i)
        {
            float angle = angleStep * static_cast<float>(i)
                        + static_cast<float>(rockSeedCounter_ & 0xFF) * 0.01f;
            float spreadDist = (cmd.radiusA + cmd.radiusB) * 0.3f;

            glm::vec3 fragPos = impactPoint + glm::vec3{
                glm::cos(angle) * spreadDist,
                glm::sin(angle) * spreadDist, 0.f };

            ecs::Entity fragEntity = AcquirePooledRock(scene, fragMass, fragHalf, fragPos);

            auto* fragRb = scene.registry_.Pool<RigidBodyComponent>().Get(fragEntity);
            if (fragRb && fragRb->IsInitialized())
            {
                glm::vec3 radial = { glm::cos(angle), glm::sin(angle), 0.f };
                glm::vec3 fragVel = comVel + radial * speed * 0.4f;
                fragRb->SetLinearVelocity(fragVel);
            }
            if (auto* fd = scene.registry_.Get<FusionDataComponent>(fragEntity))
                fd->SetCooldown(3.0f);
        }

        spdlog::info("[GravityDynamics] shatter at ({:.1f},{:.1f}) -- {} fragments, totalMass={:.1f}",
                     impactPoint.x, impactPoint.y, fragCount, totalMass);
    }

    // -- Process Collisions ----------------------------------------------------
    void GravityDynamicsSystem::ProcessCollisions(Scene& scene)
    {
        const float OVERLAP_FACTOR = fusionOverlap_;

        auto& rbPool   = scene.registry_.Pool<RigidBodyComponent>();
        auto& comps    = rbPool.Components();
        auto& entities = rbPool.Entities();

        std::vector<BroadBody> candidates;
        candidates.reserve(comps.size());

        // Check fusion cooldown via FusionDataComponent
        auto* fusionPool = scene.registry_.TryGetPool<FusionDataComponent>();

        float maxRadius = 0.f;
        for (size_t i = 0; i < comps.size(); ++i)
        {
            auto& rb = comps[i];
            if (!rb.IsInitialized() || !rb.IsDynamic())
                continue;

            // Check fusion cooldown
            if (fusionPool)
            {
                auto* fd = fusionPool->Get(entities[i]);
                if (fd && !fd->CanFuse()) continue;
            }

            const glm::vec3 h = rb.GetHalfExtents();
            const float r = (h.x + h.y) * 0.5f;
            maxRadius = glm::max(maxRadius, r);
            candidates.push_back({
                entities[i],
                rb.GetPosition(),
                rb.GetLinearVelocity(),
                h, r,
                rb.GetMass()
            });
        }

        const size_t n = candidates.size();
        if (n < 2) return;

        const float cellSize = glm::max(maxRadius * 2.0f / OVERLAP_FACTOR, 0.1f);
        const float invCell  = 1.0f / cellSize;

        SpatialGrid grid;
        grid.reserve(n);
        for (size_t i = 0; i < n; ++i)
        {
            const int cx = (int)glm::floor(candidates[i].pos.x * invCell);
            const int cy = (int)glm::floor(candidates[i].pos.y * invCell);
            grid[{cx, cy}].push_back(i);
        }

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
                            const float dist = glm::sqrt(dist2);
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

        // Build collision command batch
        std::vector<CollisionCmd> commands;
        for (size_t i = 0; i < n; ++i)
        {
            if (bestPartner[i] == SIZE_MAX || i >= bestPartner[i]) continue;

            const auto& A = candidates[i];
            const auto& B = candidates[bestPartner[i]];

            CollisionOutcome outcome = ClassifyCollision(A.mass, B.mass, A.vel, B.vel);

            commands.push_back({
                outcome,
                A.entity, B.entity,
                A.pos, B.pos,
                A.vel, B.vel,
                A.halfExtents, B.halfExtents,
                A.mass, B.mass,
                A.radius, B.radius
            });
        }

        // Execute mutations
        std::vector<ecs::Entity> toRemove;
        std::unordered_set<ecs::Entity> consumed;
        int collisionsProcessed = 0;
        for (const auto& cmd : commands)
        {
            if (consumed.count(cmd.entityA) || consumed.count(cmd.entityB)) continue;

            if (collisionsProcessed >= maxCollisionsPerFrame_) break;
            ++collisionsProcessed;

            switch (cmd.outcome)
            {
            case CollisionOutcome::Fusion:
                ExecuteFusion(scene, cmd, toRemove);
                consumed.insert(cmd.massA >= cmd.massB ? cmd.entityB : cmd.entityA);
                break;
            case CollisionOutcome::Fracture:
                ExecuteFracture(scene, cmd);
                break;
            case CollisionOutcome::Shatter:
                ExecuteShatter(scene, cmd, toRemove);
                consumed.insert(cmd.entityA);
                consumed.insert(cmd.entityB);
                break;
            }
        }

        // Deferred cleanup
        for (ecs::Entity victimId : toRemove)
        {
            if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(victimId))
                if (rn->renderable_)
                    engine_->renderEngine_.RemoveRenderable(rn->renderable_);

            if (auto* rb = scene.registry_.Pool<RigidBodyComponent>().Get(victimId))
                rb->ReleaseBody();

            auto* node = scene.GetNode(victimId);
            if (node && node->parent_)
                node->parent_->RemoveNode(victimId);
        }
    }

    // -- Escape detection ------------------------------------------------------
    void GravityDynamicsSystem::DetectEscapingBodies(Scene& scene)
    {
        if (attractorSnap_.empty()) return;

        auto& rbPool   = scene.registry_.Pool<RigidBodyComponent>();
        auto& comps    = rbPool.Components();
        auto& entities = rbPool.Entities();

        for (size_t i = 0; i < comps.size(); ++i)
        {
            auto& rb = comps[i];
            if (!rb.IsInitialized() || !rb.IsDynamic()) continue;

            const float mass = rb.GetMass();
            if (mass <= 1.1f) continue;

            const glm::vec3 pos = rb.GetPosition();
            const glm::vec3 vel = rb.GetLinearVelocity();
            const float speed2  = glm::dot(vel, vel);
            if (speed2 < 0.01f) continue;

            bool escapingAll = true;
            for (const auto& att : attractorSnap_)
            {
                const glm::vec3 diff = att.center - pos;
                const float dist2 = glm::dot(diff, diff);
                if (dist2 < 0.01f) { escapingAll = false; break; }

                const float dist = glm::sqrt(dist2);
                const glm::vec3 radialDir = diff / dist;

                const float radialVel = glm::dot(vel, radialDir);
                if (radialVel >= 0.f) { escapingAll = false; break; }

                const float escapeSpeed = glm::sqrt(2.f * att.strength / dist);
                if (glm::abs(radialVel) < escapeSpeed) { escapingAll = false; break; }
            }

            if (escapingAll)
            {
                auto* rn = scene.registry_.Pool<RenderableNode>().Get(entities[i]);
                if (!rn || !rn->renderable_) continue;

                auto* sprite = dynamic_cast<Sprite*>(rn->renderable_.get());
                if (!sprite) continue;

                const glm::vec3 h = rb.GetHalfExtents();
                const float radius = (h.x + h.y) * 0.5f;
                if (radius < 0.15f) continue;

                const float revertMass = 1.0f;
                const float revertR = 0.1f;
                sprite->SetShape(SpriteShape::MakeRock(12, rockRoughness_, rockSeedCounter_++));
                rn->renderable_->transform().setGlobalScale({revertR, revertR, 0.02f});
                {
                    auto bv = ExtractBoundaryVerts(sprite);
                    rb.Reinitialize(*engine_->physicsWorld_, revertMass, {revertR, revertR, 0.02f},
                                    bv.empty() ? nullptr : &bv);
                }
                if (auto* fd = scene.registry_.Get<FusionDataComponent>(entities[i]))
                    fd->SetCooldown(2.0f);
                spdlog::info("[GravityDynamics] escape: entity {} reverted to particle", entities[i]);
            }
        }
    }

    // -- FusionDataComponent inspector -----------------------------------------
    void FusionDataComponent::InspectProperties(ettycc::EditorPropertyVisitor& v)
    {
        PROP(cooldown, "Fusion Cooldown");
    }

} // namespace gravity
