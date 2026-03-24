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

        // Runtime — not serialized
        Transform*               syncTransform_ = nullptr; // resolved via ownerNode_ in OnStart
        btDiscreteDynamicsWorld* physWorld_     = nullptr;
        btCollisionShape*        shape_         = nullptr;
        btDefaultMotionState*    motionState_   = nullptr;
        btRigidBody*             body_          = nullptr;
    };
}

#endif
