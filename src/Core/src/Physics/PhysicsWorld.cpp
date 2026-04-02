#include <Physics/PhysicsWorld.hpp>
#include <spdlog/spdlog.h>

namespace ettycc
{
    PhysicsWorld::~PhysicsWorld()
    {
        delete world_;
        delete solver_;
        delete broadphase_;
        delete dispatcher_;
        delete config_;
    }

    void PhysicsWorld::Init()
    {
        config_     = new btSoftBodyRigidBodyCollisionConfiguration();
        dispatcher_ = new btCollisionDispatcher(config_);
        broadphase_ = new btDbvtBroadphase();
        solver_     = new btSequentialImpulseConstraintSolver();
        world_      = new btSoftRigidDynamicsWorld(dispatcher_, broadphase_, solver_, config_);
        world_->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        // Configure the soft-body world info
        btSoftBodyWorldInfo& worldInfo = world_->getWorldInfo();
        worldInfo.m_dispatcher        = dispatcher_;
        worldInfo.m_broadphase        = broadphase_;
        worldInfo.m_gravity           = btVector3(0.0f, -9.81f, 0.0f);
        worldInfo.air_density         = btScalar(1.2);
        worldInfo.water_density       = btScalar(0.0);
        worldInfo.water_offset        = btScalar(0.0);
        worldInfo.water_normal        = btVector3(0.0f, 0.0f, 0.0f);
        worldInfo.m_sparsesdf.Initialize();

        spdlog::info("[PhysicsWorld] initialized (soft+rigid) — gravity (0, -9.81, 0)");
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        if (!world_) return;
        // Guard against degenerate delta (first frame, minimized window, breakpoint, etc.).
        if (deltaTime <= 0.f || deltaTime > 0.25f)
            deltaTime = 1.f / 60.f;
        world_->stepSimulation(deltaTime, 16);
    }

    btDiscreteDynamicsWorld* PhysicsWorld::GetWorld()
    {
        return world_;   // implicit upcast — btSoftRigidDynamicsWorld IS-A btDiscreteDynamicsWorld
    }
    btSoftRigidDynamicsWorld* PhysicsWorld::GetSoftWorld()
    {
        return world_;
    }
}
