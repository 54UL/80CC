#ifndef RIGID_BODY_COMPONENT_HPP
#define RIGID_BODY_COMPONENT_HPP

#include <Scene/PropertySystem.hpp>
#include <Scene/Transform.hpp>
#include <Scene/Api.hpp>
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    struct EditorPropertyVisitor;

    // ── RigidBodyComponent ────────────────────────────────────────────────────
    // Pure data component — no virtual methods, no back-pointer to its entity.
    // Runtime initialization and per-frame sync are performed by PhysicsSystem.
    class RigidBodyComponent
    {
    public:
        static constexpr const char*        componentType = "RigidBody";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::MAIN;

        RigidBodyComponent() = default;
        RigidBodyComponent(float mass, glm::vec3 halfExtents, glm::vec3 initialPosition);
        ~RigidBodyComponent();

        // Non-copyable: owns Bullet objects. Movable so std::vector can relocate
        // without triggering the destructor on a valid body.
        RigidBodyComponent(const RigidBodyComponent&)            = delete;
        RigidBodyComponent& operator=(const RigidBodyComponent&) = delete;

        RigidBodyComponent(RigidBodyComponent&& o) noexcept
            : mass_(o.mass_), halfExtents_(o.halfExtents_)
            , initialPosition_(o.initialPosition_)
            , syncTransform_(o.syncTransform_)
            , physWorld_(o.physWorld_)
            , shape_(o.shape_), motionState_(o.motionState_), body_(o.body_)
            , isManipulated_(o.isManipulated_)
        {
            o.syncTransform_ = nullptr;
            o.physWorld_     = nullptr;
            o.shape_         = nullptr;
            o.motionState_   = nullptr;
            o.body_          = nullptr;
        }

        RigidBodyComponent& operator=(RigidBodyComponent&& o) noexcept
        {
            if (this == &o) return *this;
            if (body_ && physWorld_) physWorld_->removeRigidBody(body_);
            delete motionState_; motionState_ = nullptr;
            delete body_;        body_        = nullptr;
            delete shape_;       shape_       = nullptr;

            mass_            = o.mass_;
            halfExtents_     = o.halfExtents_;
            initialPosition_ = o.initialPosition_;
            syncTransform_   = o.syncTransform_;
            physWorld_       = o.physWorld_;
            shape_           = o.shape_;
            motionState_     = o.motionState_;
            body_            = o.body_;
            isManipulated_   = o.isManipulated_;

            o.syncTransform_ = nullptr;
            o.physWorld_     = nullptr;
            o.shape_         = nullptr;
            o.motionState_   = nullptr;
            o.body_          = nullptr;
            return *this;
        }

        // ── System-facing API (called by PhysicsSystem) ───────────────────────
        // Creates the Bullet rigid body and links it to the node transform.
        // Optional siblingRenderable pointer seeds the initial transform.
        void InitBody(btDiscreteDynamicsWorld* world,
                      Transform& syncTransform,
                      const Transform* seedTransform = nullptr);

        // Pulls the Bullet simulation result into the node transform.
        void SyncToTransform(Transform& t) const;

        bool IsInitialized() const { return body_ != nullptr; }

        // ── Editor gizmo API ──────────────────────────────────────────────────
        void BeginManipulation();
        void EndManipulation();
        void SyncFromRenderable();          // push node transform → Bullet
        bool IsManipulated() const         { return isManipulated_; }

        // ── Editor inspector ──────────────────────────────────────────────────
        void InspectProperties(EditorPropertyVisitor& v);

        // ── Serialization ─────────────────────────────────────────────────────
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
        // ── Serialized fields ─────────────────────────────────────────────────
        float     mass_            = 1.0f;
        glm::vec3 halfExtents_     = { 0.5f, 0.5f, 0.5f };
        glm::vec3 initialPosition_ = { 0.0f, 0.0f, 0.0f };

        // ── Runtime (not serialized, set by PhysicsSystem::InitBody) ─────────
        Transform*               syncTransform_ = nullptr;
        btDiscreteDynamicsWorld* physWorld_      = nullptr;
        btCollisionShape*        shape_          = nullptr;
        btDefaultMotionState*    motionState_    = nullptr;
        btRigidBody*             body_           = nullptr;
        bool                     isManipulated_  = false;
    };
}

#endif
