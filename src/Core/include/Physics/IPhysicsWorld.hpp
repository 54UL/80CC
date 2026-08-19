#ifndef ETTYCC_IPHYSICS_WORLD_HPP
#define ETTYCC_IPHYSICS_WORLD_HPP

#include <Physics/PhysicsDefs.hpp>
#include <Physics/IPhysicsBody.hpp>
#include <Physics/IPhysicsSoftBody.hpp>

#include <memory>

namespace ettycc::physics
{
    class IPhysicsWorld
    {
    public:
        virtual ~IPhysicsWorld() = default;

        virtual void Init() = 0;
        virtual void Step(float deltaTime) = 0;

        virtual void      SetGravity(const glm::vec3& g) = 0;
        virtual glm::vec3 GetGravity() const = 0;

        // Rigid body lifecycle
        virtual std::unique_ptr<IPhysicsBody>     CreateRigidBody(const RigidBodyDef& def) = 0;
        virtual void                              DestroyRigidBody(IPhysicsBody* body) = 0;

        // Soft body lifecycle
        virtual std::unique_ptr<IPhysicsSoftBody> CreateSoftBody(const SoftBodyDef& def) = 0;
        virtual void                              DestroySoftBody(IPhysicsSoftBody* body) = 0;

        virtual bool        IsMultithreaded() const = 0;
        virtual const char* GetName()         const = 0;
    };

} // namespace ettycc::physics

#endif
