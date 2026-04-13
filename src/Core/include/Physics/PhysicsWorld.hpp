#ifndef PHYSICS_WORLD_HPP
#define PHYSICS_WORLD_HPP

#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <LinearMath/btThreads.h>
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

        btDiscreteDynamicsWorld* GetWorld();
        btSoftRigidDynamicsWorld* GetSoftWorld();

        bool IsMultithreaded() const { return multithreaded_; }

    private:
        std::unique_ptr<btSoftBodyRigidBodyCollisionConfiguration> config_;
        // Type-erased dispatcher — may be btCollisionDispatcher or btCollisionDispatcherMt.
        std::unique_ptr<btCollisionDispatcher>                     dispatcher_;
        std::unique_ptr<btBroadphaseInterface>                     broadphase_;
        std::unique_ptr<btSequentialImpulseConstraintSolver>       solver_;
        std::unique_ptr<btSoftRigidDynamicsWorld>                  world_;

        btITaskScheduler* taskScheduler_ = nullptr;
        bool              multithreaded_ = false;
    };
}

#endif
