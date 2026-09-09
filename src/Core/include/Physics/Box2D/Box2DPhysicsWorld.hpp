#ifndef ETTYCC_BOX2D_PHYSICS_WORLD_HPP
#define ETTYCC_BOX2D_PHYSICS_WORLD_HPP

#include <Physics/IPhysicsWorld.hpp>
#include <memory>

class b2World;

namespace ettycc::physics
{
    class Box2DPhysicsWorld : public IPhysicsWorld
    {
    public:
        Box2DPhysicsWorld();
        ~Box2DPhysicsWorld() override;

        void Init()                                                    override;
        void Step(float deltaTime)                                     override;
        void SetGravity(const glm::vec3& g)                            override;
        glm::vec3 GetGravity()                                  const  override;

        std::unique_ptr<IPhysicsBody>     CreateRigidBody(const RigidBodyDef& def)  override;
        void                              DestroyRigidBody(IPhysicsBody* body)      override;

        // Soft bodies are not supported by Box2D -- returns nullptr.
        std::unique_ptr<IPhysicsSoftBody> CreateSoftBody(const SoftBodyDef& def)    override;
        void                              DestroySoftBody(IPhysicsSoftBody* body)   override;

        bool        IsMultithreaded() const override { return false; }
        const char* GetName()         const override { return "Box2D"; }

    private:
        std::unique_ptr<b2World> world_;
        glm::vec3 gravity_ = {0.f, -9.81f, 0.f};
    };

} // namespace ettycc::physics

#endif
