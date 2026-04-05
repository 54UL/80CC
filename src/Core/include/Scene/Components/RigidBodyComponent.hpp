#ifndef RIGID_BODY_COMPONENT_HPP
#define RIGID_BODY_COMPONENT_HPP

#include <Scene/PropertySystem.hpp>
#include <Scene/Transform.hpp>
#include <Scene/Api.hpp>
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>
#include <memory>

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
            , shape_(std::move(o.shape_))
            , motionState_(std::move(o.motionState_))
            , body_(std::move(o.body_))
            , isManipulated_(o.isManipulated_)
        {
            o.syncTransform_ = nullptr;
            o.physWorld_     = nullptr;
        }

        RigidBodyComponent& operator=(RigidBodyComponent&& o) noexcept
        {
            if (this == &o) return *this;
            if (body_ && physWorld_) physWorld_->removeRigidBody(body_.get());

            mass_            = o.mass_;
            halfExtents_     = o.halfExtents_;
            initialPosition_ = o.initialPosition_;
            syncTransform_   = o.syncTransform_;
            physWorld_       = o.physWorld_;
            shape_           = std::move(o.shape_);
            motionState_     = std::move(o.motionState_);
            body_            = std::move(o.body_);
            isManipulated_   = o.isManipulated_;

            o.syncTransform_ = nullptr;
            o.physWorld_     = nullptr;
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
        bool IsDynamic()     const { return mass_ > 0.f; }
        float GetMass()      const { return mass_; }

        // Fusion cooldown — prevents chain reactions.
        bool  CanFuse()  const { return fusionCooldown_ <= 0.f; }
        void  SetFusionCooldown(float seconds) { fusionCooldown_ = seconds; }
        void  TickCooldown(float dt) { if (fusionCooldown_ > 0.f) fusionCooldown_ -= dt; }

        void ApplyCentralForce(const glm::vec3& f);
        void SetLinearVelocity(const glm::vec3& v);
        glm::vec3 GetPosition() const;
        glm::vec3 GetLinearVelocity() const;
        glm::vec3 GetHalfExtents() const { return halfExtents_; }
        glm::quat GetRotation() const;

        // Recreate the Bullet body with new mass / half-extents at the current
        // position.  Used by the fusion system after merging two bodies.
        void Reinitialize(float newMass, const glm::vec3& newHalfExtents);

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
        Transform*                           syncTransform_  = nullptr;  // non-owning
        btDiscreteDynamicsWorld*             physWorld_      = nullptr;  // non-owning
        std::unique_ptr<btCollisionShape>    shape_;
        std::unique_ptr<btDefaultMotionState> motionState_;
        std::unique_ptr<btRigidBody>         body_;
        bool                                 isManipulated_  = false;
        float                                fusionCooldown_ = 0.f;
    };
}

#endif
