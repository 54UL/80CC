#include <Physics/PhysicsWorld.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    PhysicsWorld::~PhysicsWorld()
    {
        // Explicit teardown order: world first, then its dependencies.
        world_.reset();
        solver_.reset();
        broadphase_.reset();
        dispatcher_.reset();
        config_.reset();
    }

    void PhysicsWorld::Init()
    {
        config_     = std::make_unique<btSoftBodyRigidBodyCollisionConfiguration>();
        dispatcher_ = std::make_unique<btCollisionDispatcher>(config_.get());
        broadphase_ = std::make_unique<btDbvtBroadphase>();
        solver_     = std::make_unique<btSequentialImpulseConstraintSolver>();
        world_      = std::make_unique<btSoftRigidDynamicsWorld>(
                          dispatcher_.get(), broadphase_.get(), solver_.get(), config_.get());
        world_->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        // Configure the soft-body world info
        btSoftBodyWorldInfo& worldInfo = world_->getWorldInfo();
        worldInfo.m_dispatcher        = dispatcher_.get();
        worldInfo.m_broadphase        = broadphase_.get();
        worldInfo.m_gravity           = btVector3(0.0f, -9.81f, 0.0f);
        worldInfo.air_density         = btScalar(1.2);
        worldInfo.water_density       = btScalar(0.0);
        worldInfo.water_offset        = btScalar(0.0);
        worldInfo.water_normal        = btVector3(0.0f, 0.0f, 0.0f);
        worldInfo.m_sparsesdf.Initialize();

        // Skip AABB recalculation for sleeping/static bodies — big win with many boxes.
        world_->setForceUpdateAllAabbs(false);

        spdlog::info("[PhysicsWorld] initialized (soft+rigid) — gravity (0, -9.81, 0)");
    }

    void PhysicsWorld::SetGravity(const btVector3& g)
    {
        if (world_) world_->setGravity(g);
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        if (!world_) return;
        // Guard against degenerate delta (first frame, minimized window, breakpoint, etc.).
        if (deltaTime <= 0.f || deltaTime > 0.25f)
            deltaTime = 1.f / 60.f;
        world_->stepSimulation(deltaTime, 8);
    }

    btDiscreteDynamicsWorld* PhysicsWorld::GetWorld()
    {
        return world_.get();   // implicit upcast — btSoftRigidDynamicsWorld IS-A btDiscreteDynamicsWorld
    }
    btSoftRigidDynamicsWorld* PhysicsWorld::GetSoftWorld()
    {
        return world_.get();
    }
}
