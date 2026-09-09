#ifndef RIGID_BODY_COMPONENT_HPP
#define RIGID_BODY_COMPONENT_HPP

#include <Scene/PropertySystem.hpp>
#include <Scene/Transform.hpp>
#include <Scene/Api.hpp>
#include <Physics/IPhysicsBody.hpp>
#include <glm/glm.hpp>
#include <vector>

namespace ettycc { namespace physics { class IPhysicsWorld; } }
#include <cereal/archives/json.hpp>
#include <memory>

namespace ettycc
{
    struct EditorPropertyVisitor;

    class RigidBodyComponent
    {
    public:
        static constexpr const char*        componentType = "RigidBody";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::MAIN;

        RigidBodyComponent() = default;
        RigidBodyComponent(float mass, glm::vec3 halfExtents, glm::vec3 initialPosition);
        ~RigidBodyComponent();

        RigidBodyComponent(const RigidBodyComponent&)            = delete;
        RigidBodyComponent& operator=(const RigidBodyComponent&) = delete;

        RigidBodyComponent(RigidBodyComponent&& o) noexcept
            : mass_(o.mass_), halfExtents_(o.halfExtents_)
            , initialPosition_(o.initialPosition_)
            , syncTransform_(o.syncTransform_)
            , body_(std::move(o.body_))
            , isManipulated_(o.isManipulated_)
        {
            o.syncTransform_ = nullptr;
        }

        RigidBodyComponent& operator=(RigidBodyComponent&& o) noexcept
        {
            if (this == &o) return *this;
            body_.reset();

            mass_            = o.mass_;
            halfExtents_     = o.halfExtents_;
            initialPosition_ = o.initialPosition_;
            syncTransform_   = o.syncTransform_;
            body_            = std::move(o.body_);
            isManipulated_   = o.isManipulated_;

            o.syncTransform_ = nullptr;
            return *this;
        }

        // -- System-facing API (called by PhysicsSystem) -----------------------
        void InitBody(physics::IPhysicsWorld& world,
                      Transform& syncTransform,
                      const Transform* seedTransform = nullptr,
                      const std::vector<glm::vec2>* polyVerts = nullptr);

        void SyncToTransform(Transform& t) const;

        bool IsInitialized() const { return body_ != nullptr; }
        void ReleaseBody() { body_.reset(); }
        bool IsDynamic()     const { return mass_ > 0.f; }
        float GetMass()      const { return mass_; }

        void ApplyCentralForce(const glm::vec3& f);
        void SetLinearVelocity(const glm::vec3& v);
        glm::vec3 GetPosition() const;
        glm::vec3 GetLinearVelocity() const;
        glm::vec3 GetHalfExtents() const { return halfExtents_; }
        glm::quat GetRotation() const;

        void Reinitialize(physics::IPhysicsWorld& world, float newMass, const glm::vec3& newHalfExtents,
                          const std::vector<glm::vec2>* polyVerts = nullptr);

        // -- Editor gizmo API --------------------------------------------------
        void BeginManipulation();
        void EndManipulation();
        void SyncFromRenderable();
        bool IsManipulated() const { return isManipulated_; }

        // -- Editor inspector --------------------------------------------------
        void InspectProperties(EditorPropertyVisitor& v);

        // -- Serialization -----------------------------------------------------
        template <class Archive>
        void serialize(Archive& ar)
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
        // -- Serialized fields -------------------------------------------------
        float     mass_            = 1.0f;
        glm::vec3 halfExtents_     = { 0.5f, 0.5f, 0.5f };
        glm::vec3 initialPosition_ = { 0.0f, 0.0f, 0.0f };

        // -- Runtime (not serialized) ------------------------------------------
        Transform*                               syncTransform_  = nullptr;
        std::unique_ptr<physics::IPhysicsBody>   body_;
        bool                                     isManipulated_  = false;
    };
}

#endif
