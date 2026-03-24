#ifndef PHYSICS_WORLD_HPP
#define PHYSICS_WORLD_HPP

#include <btBulletDynamicsCommon.h>

namespace ettycc
{
    class PhysicsWorld
    {
    public:
        PhysicsWorld() = default;
        ~PhysicsWorld();

        void Init();
        void Step(float deltaTime);
        btDiscreteDynamicsWorld* GetWorld();

    private:
        btDefaultCollisionConfiguration*     config_     = nullptr;
        btCollisionDispatcher*               dispatcher_ = nullptr;
        btBroadphaseInterface*               broadphase_ = nullptr;
        btSequentialImpulseConstraintSolver* solver_     = nullptr;
        btDiscreteDynamicsWorld*             world_      = nullptr;
    };
}

#endif
