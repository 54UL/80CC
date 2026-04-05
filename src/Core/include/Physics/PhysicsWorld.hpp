#ifndef PHYSICS_WORLD_HPP
#define PHYSICS_WORLD_HPP

#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <memory>

namespace ettycc
{
    class PhysicsWorld
    {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld();

        void Init();
        void Step(float deltaTime);
        void SetGravity(const btVector3& g);

        // Backward-compat accessor — btSoftRigidDynamicsWorld IS-A btDiscreteDynamicsWorld
        btDiscreteDynamicsWorld* GetWorld();

        // Full soft-body world accessor
        btSoftRigidDynamicsWorld* GetSoftWorld();

    private:
        // Declared in destruction-safe order (reverse of construction).
        // world_ depends on all others, so it must be destroyed first.
        std::unique_ptr<btSoftBodyRigidBodyCollisionConfiguration> config_;
        std::unique_ptr<btCollisionDispatcher>                     dispatcher_;
        std::unique_ptr<btBroadphaseInterface>                     broadphase_;
        std::unique_ptr<btSequentialImpulseConstraintSolver>       solver_;
        std::unique_ptr<btSoftRigidDynamicsWorld>                  world_;
    };
}

#endif
