#pragma once
#include <ECS/ISystem.hpp>
#include <ECS/Entity.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

namespace ettycc
{
    class RigidBodyComponent;
    class Sprite;
    class SceneNode;

    // -- PhysicsSystem ---------------------------------------------------------
    // MAIN channel.
    // Generic physics stepping: initializes RigidBody/SoftBody components and
    // syncs physics transforms back to the scene each frame.
    //
    // Game-specific physics behaviors (gravity attraction, collision outcomes,
    // N-body dynamics) should be implemented in a separate ISystem registered
    // by a game module.
    class PhysicsSystem : public ISystem
    {
    public:
        ProcessingChannel Channel() const override { return ProcessingChannel::MAIN; }

        void OnStart      (Scene& scene, Engine& engine) override;
        void OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity) override;
        void OnUpdate     (Scene& scene, float dt) override;

        // Slice a rigid body along a world-space line.
        void SliceBody(Scene& scene, ecs::Entity entity,
                       glm::vec2 linePoint, glm::vec2 lineDir, float breakForce = 50.f);

    private:
        void InitRigidBody(Scene& scene, Engine& engine, ecs::Entity e);
        void InitSoftBody (Scene& scene, Engine& engine, ecs::Entity e);

        Engine* engine_ = nullptr;
    };

    // Utility: extract boundary vertices from a sprite's shape
    std::vector<glm::vec2> ExtractBoundaryVerts(const Sprite* sprite);
}
