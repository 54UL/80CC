#pragma once
#include <Scene/Api.hpp>
#include <ECS/Entity.hpp>

namespace ettycc {
    class Scene;  // forward — Scene.hpp includes ISystem.hpp, so forward only here
    class Engine; // forward

    // Base class for all ECS systems.
    // Each system declares which ProcessingChannel it runs on and receives
    // Scene& (which owns the Registry) so it can query and update components.
    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        // The channel this system processes (MAIN, RENDERING, or AUDIO).
        virtual ProcessingChannel Channel() const = 0;

        // Called once when the scene is initialized (all entities already loaded).
        // Systems iterate their component pool here and perform one-time setup.
        virtual void OnStart(Scene& scene, Engine& engine) = 0;

        // Called when a single entity is added to the scene at runtime.
        // Default: no-op.  Override to lazily initialize the entity's components.
        virtual void OnEntityAdded(Scene& scene, Engine& engine, ecs::Entity entity) {}

        // Called every frame for all entities managed by this system.
        virtual void OnUpdate(Scene& scene, float dt) = 0;
    };

} // namespace ettycc
