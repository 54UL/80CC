#ifndef ETTYCC_BULLET_PHYSICS_WORLD_HPP
#define ETTYCC_BULLET_PHYSICS_WORLD_HPP

#include <Physics/IPhysicsWorld.hpp>

#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <LinearMath/btThreads.h>
#include <memory>

namespace ettycc::physics
{
    class BulletPhysicsWorld : public IPhysicsWorld
    {
    public:
        BulletPhysicsWorld() = default;
        ~BulletPhysicsWorld() override;

        // IPhysicsWorld
        void Init()                                                    override;
        void Step(float deltaTime)                                     override;
        void SetGravity(const glm::vec3& g)                            override;
        glm::vec3 GetGravity()                                  const  override;

        std::unique_ptr<IPhysicsBody>     CreateRigidBody(const RigidBodyDef& def)  override;
        void                              DestroyRigidBody(IPhysicsBody* body)      override;

        std::unique_ptr<IPhysicsSoftBody> CreateSoftBody(const SoftBodyDef& def)    override;
        void                              DestroySoftBody(IPhysicsSoftBody* body)   override;

        bool        IsMultithreaded() const override { return multithreaded_; }
        const char* GetName()         const override { return "Bullet"; }

    private:
        std::unique_ptr<btSoftBodyRigidBodyCollisionConfiguration> config_;
        std::unique_ptr<btCollisionDispatcher>                     dispatcher_;
        std::unique_ptr<btBroadphaseInterface>                     broadphase_;
        std::unique_ptr<btSequentialImpulseConstraintSolver>       solver_;
        std::unique_ptr<btSoftRigidDynamicsWorld>                  world_;

        btITaskScheduler* taskScheduler_ = nullptr;
        bool              multithreaded_ = false;
    };

} // namespace ettycc::physics

#endif
