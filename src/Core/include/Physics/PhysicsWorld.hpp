#ifndef PHYSICS_WORLD_HPP
#define PHYSICS_WORLD_HPP

#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>

namespace ettycc
{
    class PhysicsWorld
    {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld();

        void Init();
        void Step(float deltaTime);

        // Backward-compat accessor — btSoftRigidDynamicsWorld IS-A btDiscreteDynamicsWorld
        btDiscreteDynamicsWorld* GetWorld();

        // Full soft-body world accessor
        btSoftRigidDynamicsWorld* GetSoftWorld();

    private:
        btSoftBodyRigidBodyCollisionConfiguration* config_     = nullptr;
        btCollisionDispatcher*                     dispatcher_ = nullptr;
        btBroadphaseInterface*                     broadphase_ = nullptr;
        btSequentialImpulseConstraintSolver*       solver_     = nullptr;
        btSoftRigidDynamicsWorld*                  world_      = nullptr;
    };
}

#endif
