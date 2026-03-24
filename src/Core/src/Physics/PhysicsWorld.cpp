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
        config_     = new btDefaultCollisionConfiguration();
        dispatcher_ = new btCollisionDispatcher(config_);
        broadphase_ = new btDbvtBroadphase();
        solver_     = new btSequentialImpulseConstraintSolver();
        world_      = new btDiscreteDynamicsWorld(dispatcher_, broadphase_, solver_, config_);
        world_->setGravity(btVector3(0.0f, -9.81f, 0.0f));

        spdlog::info("[PhysicsWorld] initialized — gravity (0, -9.81, 0)");
    }

    void PhysicsWorld::Step(float deltaTime)
    {
        if (world_)
            world_->stepSimulation(deltaTime, 10);
    }

    btDiscreteDynamicsWorld* PhysicsWorld::GetWorld()
    {
        return world_;
    }
}
