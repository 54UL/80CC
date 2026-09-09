#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Engine.hpp>
#include <Threading/ThreadRegistry.hpp>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <cmath>

namespace ettycc
{
    // -- Utility: extract boundary vertices from a sprite's shape ---------------
    std::vector<glm::vec2> ExtractBoundaryVerts(const Sprite* sprite)
    {
        std::vector<glm::vec2> result;
        if (!sprite) return result;
        const auto& shape = sprite->GetShape();
        if (shape.vertices.size() < 3) return result;

        // Fan layout: vertex[0] is centroid, boundary starts at [1]
        size_t start = (shape.vertices.size() > 3) ? 1 : 0;
        result.reserve(shape.vertices.size() - start);
        for (size_t i = start; i < shape.vertices.size(); ++i)
            result.push_back(shape.vertices[i].position);
        return result;
    }

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

    // -- OnUpdate --------------------------------------------------------------
    void PhysicsSystem::OnUpdate(Scene& scene, float dt)
    {
        // Sync rigid body transforms
        {
            auto& rbPool   = scene.registry_.Pool<RigidBodyComponent>();
            auto& comps    = rbPool.Components();
            auto& entities = rbPool.Entities();
            const size_t n = comps.size();

            if (engine_)
            {
                engine_->threadRegistry_.ParallelFor(n,
                    [&comps, &entities, &scene](size_t begin, size_t end)
                    {
                        for (size_t i = begin; i < end; ++i)
                        {
                            if (comps[i].IsInitialized())
                            {
                                auto* node = scene.GetNode(entities[i]);
                                if (node) comps[i].SyncToTransform(node->transform_);
                            }
                        }
                    });
            }
            else
            {
                for (size_t i = 0; i < n; ++i)
                {
                    if (comps[i].IsInitialized())
                    {
                        auto* node = scene.GetNode(entities[i]);
                        if (node) comps[i].SyncToTransform(node->transform_);
                    }
                }
            }
        }

        // Sync soft body transforms
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

    // -- Private helpers -------------------------------------------------------
    void PhysicsSystem::InitRigidBody(Scene& scene, Engine& engine, ecs::Entity e)
    {
        auto* rb   = scene.registry_.Pool<RigidBodyComponent>().Get(e);
        auto* node = scene.GetNode(e);
        if (!rb || !node || rb->IsInitialized()) return;

        const Transform* seedTransform = nullptr;
        const std::vector<glm::vec2>* polyVerts = nullptr;
        std::vector<glm::vec2> verts;

        if (auto* rn = scene.registry_.Pool<RenderableNode>().Get(e))
        {
            if (rn->renderable_)
            {
                seedTransform = &rn->renderable_->transform();
                if (auto* sprite = dynamic_cast<Sprite*>(rn->renderable_.get()))
                {
                    verts = ExtractBoundaryVerts(sprite);
                    if (!verts.empty())
                        polyVerts = &verts;
                }
            }
        }

        rb->InitBody(*engine.physicsWorld_, node->transform_, seedTransform, polyVerts);
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

    // -- Slice a rigid body into two halves -----------------------------------
    void PhysicsSystem::SliceBody(Scene& scene, ecs::Entity entity,
                                  glm::vec2 linePoint, glm::vec2 lineDir, float breakForce)
    {
        if (!engine_) return;

        auto* rb = scene.registry_.Pool<RigidBodyComponent>().Get(entity);
        auto* rn = scene.registry_.Pool<RenderableNode>().Get(entity);
        if (!rb || !rn || !rb->IsInitialized()) return;

        auto* sprite = dynamic_cast<Sprite*>(rn->renderable_.get());
        if (!sprite) return;

        auto* node = scene.GetNode(entity);
        if (!node || !node->parent_) return;

        // -- Transform world-space line into local shape space ----------------
        const glm::vec3 worldPos   = rb->GetPosition();
        const glm::vec3 worldScale = rn->renderable_->transform().getGlobalScale();
        const float scaleX = (glm::abs(worldScale.x) > 1e-6f) ? worldScale.x : 1.f;
        const float scaleY = (glm::abs(worldScale.y) > 1e-6f) ? worldScale.y : 1.f;

        glm::vec2 localLinePoint = {
            (linePoint.x - worldPos.x) / scaleX,
            (linePoint.y - worldPos.y) / scaleY
        };
        glm::vec2 localLineDir = glm::normalize(glm::vec2{ lineDir.x / scaleX, lineDir.y / scaleY });
        glm::vec2 localNormal  = { -localLineDir.y, localLineDir.x };

        // -- Slice the geometry -----------------------------------------------
        const SpriteShape& srcShape = sprite->GetShape();
        auto sliceResult = SliceSpriteShape(srcShape, localLinePoint, localNormal);

        if (sliceResult.sideA.indices.empty() || sliceResult.sideB.indices.empty())
        {
            spdlog::info("[PhysicsSystem] slice missed entity {} -- line doesn't intersect geometry", entity);
            return;
        }

        // -- Compute mass distribution based on area --------------------------
        const float areaA = sliceResult.sideA.ComputeArea();
        const float areaB = sliceResult.sideB.ComputeArea();
        const float totalArea = areaA + areaB;
        if (totalArea < 1e-6f) return;

        const float origMass = rb->GetMass();
        const float massA = origMass * (areaA / totalArea);
        const float massB = origMass * (areaB / totalArea);

        // -- Compute centroids in world space ---------------------------------
        glm::vec2 centroidA = sliceResult.sideA.ComputeCentroid();
        glm::vec2 centroidB = sliceResult.sideB.ComputeCentroid();
        glm::vec3 worldCentroidA = { worldPos.x + centroidA.x * scaleX,
                                     worldPos.y + centroidA.y * scaleY, 0.f };
        glm::vec3 worldCentroidB = { worldPos.x + centroidB.x * scaleX,
                                     worldPos.y + centroidB.y * scaleY, 0.f };

        auto computeHalfExtents = [&](const SpriteShape& shape) -> glm::vec3 {
            glm::vec2 mn(1e9f), mx(-1e9f);
            for (auto& v : shape.vertices) {
                mn = glm::min(mn, v.position);
                mx = glm::max(mx, v.position);
            }
            glm::vec2 halfLocal = (mx - mn) * 0.5f;
            return { glm::max(halfLocal.x * glm::abs(scaleX), 0.02f),
                     glm::max(halfLocal.y * glm::abs(scaleY), 0.02f),
                     glm::max(worldScale.z, 0.02f) };
        };

        glm::vec3 halfA = computeHalfExtents(sliceResult.sideA);
        glm::vec3 halfB = computeHalfExtents(sliceResult.sideB);

        // -- Preserve original velocity ---------------------------------------
        const glm::vec3 origVel = rb->GetLinearVelocity();
        const bool wasTextureless = sprite->IsTextureless();
        const auto srcProceduralParams = sprite->GetProceduralParams();
        const std::string texPath = sprite->GetTexturePath();
        const std::string shaderName = sprite->GetShaderName();

        sliceResult.sideA.RecomputeRadialUVs();
        sliceResult.sideB.RecomputeRadialUVs();

        glm::vec3 breakDir = { localNormal.x, localNormal.y, 0.f };
        breakDir = glm::normalize(breakDir);

        // -- Remove original entity -------------------------------------------
        engine_->renderEngine_.RemoveRenderable(rn->renderable_);
        rb->ReleaseBody();
        auto parentNode = node->parent_;
        parentNode->RemoveNode(entity);

        // -- Helper: create a half-entity ------------------------------------
        static int sliceIdx = 0;
        auto createHalf = [&](SpriteShape& shape, glm::vec3 pos,
                              glm::vec3 halfExt, float mass, float forceSign)
        {
            std::shared_ptr<Sprite> halfSprite;
            if (wasTextureless)
            {
                halfSprite = Sprite::MakeWithShader(shape, shaderName,
                                                     srcProceduralParams.ToUniforms());
            }
            else
            {
                halfSprite = std::make_shared<Sprite>(texPath);
                halfSprite->SetShape(shape);
            }
            halfSprite->underylingTransform.setGlobalPosition(pos);
            halfSprite->underylingTransform.setGlobalScale(halfExt);

            auto halfNode = std::make_shared<SceneNode>("slice-" + std::to_string(sliceIdx++));

            scene.registry_.Add<RenderableNode>(halfNode->GetId(), RenderableNode{halfSprite});
            scene.registry_.Add<RigidBodyComponent>(
                halfNode->GetId(), RigidBodyComponent{mass, halfExt, pos});
            parentNode->AddChild(halfNode);

            auto* newRb = scene.registry_.Pool<RigidBodyComponent>().Get(halfNode->GetId());
            if (newRb && newRb->IsInitialized())
            {
                glm::vec3 vel = origVel + breakDir * (forceSign * breakForce);
                newRb->SetLinearVelocity(vel);
            }
        };

        createHalf(sliceResult.sideA, worldCentroidA, halfA, massA, +1.f);
        createHalf(sliceResult.sideB, worldCentroidB, halfB, massB, -1.f);

        spdlog::info("[PhysicsSystem] sliced entity {} -> massA={:.1f} massB={:.1f} breakForce={:.1f}",
                     entity, massA, massB, breakForce);
    }

} // namespace ettycc
