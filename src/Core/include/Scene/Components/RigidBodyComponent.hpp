#ifndef RIGID_BODY_COMPONENT_HPP
#define RIGID_BODY_COMPONENT_HPP

#include <Scene/NodeComponent.hpp>
#include <Scene/Transform.hpp>
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    class RigidBodyComponent : public NodeComponent
    {
    public:
        static constexpr const char* componentType = "RigidBody";
        RigidBodyComponent() = default;
        RigidBodyComponent(float mass, glm::vec3 halfExtents, glm::vec3 initialPosition);
        ~RigidBodyComponent() override;

        NodeComponentInfo GetComponentInfo() override;
        void OnStart(std::shared_ptr<Engine> engineInstance) override;
        void OnUpdate(float deltaTime) override;

        // Editor gizmo interface — called when the user starts / ends a drag
        // on the owning node.  Switches between kinematic (editor-controlled)
        // and dynamic (simulation-controlled) mode.
        void BeginManipulation();
        void EndManipulation();

        // Pushes the current renderable world-position into the bullet body.
        // Only meaningful while in kinematic mode (between Begin/EndManipulation).
        void SyncFromRenderable();

    public:
        template <class Archive>
        void save(Archive& ar) const
        {
            ar(cereal::make_nvp("mass",   mass_),
               cereal::make_nvp("half_x", halfExtents_.x),
               cereal::make_nvp("half_y", halfExtents_.y),
               cereal::make_nvp("half_z", halfExtents_.z),
               cereal::make_nvp("pos_x",  initialPosition_.x),
               cereal::make_nvp("pos_y",  initialPosition_.y),
               cereal::make_nvp("pos_z",  initialPosition_.z));
        }

        template <class Archive>
        void load(Archive& ar)
        {
            ar(cereal::make_nvp("mass",   mass_),
               cereal::make_nvp("half_x", halfExtents_.x),
               cereal::make_nvp("half_y", halfExtents_.y),
               cereal::make_nvp("half_z", halfExtents_.z),
               cereal::make_nvp("pos_x",  initialPosition_.x),
               cereal::make_nvp("pos_y",  initialPosition_.y),
               cereal::make_nvp("pos_z",  initialPosition_.z));
        }

    private:
        float     mass_            = 1.0f;
        glm::vec3 halfExtents_     = { 0.5f, 0.5f, 0.5f };
        glm::vec3 initialPosition_ = { 0.0f, 0.0f, 0.0f };
        uint64_t rigidBodyId_     = 0;
        
        // Runtime — not serialized
        Transform*               syncTransform_  = nullptr;
        btDiscreteDynamicsWorld* physWorld_      = nullptr;
        btCollisionShape*        shape_          = nullptr;
        btDefaultMotionState*    motionState_    = nullptr;
        btRigidBody*             body_           = nullptr;
        bool                     isManipulated_  = false; // true while editor gizmo drag is active
    };
}

#endif
