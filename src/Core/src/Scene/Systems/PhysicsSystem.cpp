#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Scene.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Engine.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    // ── OnStart ───────────────────────────────────────────────────────────────
    void PhysicsSystem::OnStart(Scene& scene, Engine& engine)
    {
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
    void PhysicsSystem::OnUpdate(Scene& scene, float /*dt*/)
    {
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
