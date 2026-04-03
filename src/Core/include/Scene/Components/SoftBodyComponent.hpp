#ifndef SOFT_BODY_COMPONENT_HPP
#define SOFT_BODY_COMPONENT_HPP

#include <Scene/Api.hpp>
#include <Scene/PropertySystem.hpp>
#include <Graphics/Rendering/Entities/SoftBodyRenderable.hpp>

#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBody.h>

#include <cereal/archives/json.hpp>

#include <memory>
#include <string>
#include <vector>

namespace ettycc
{
    struct EditorPropertyVisitor;
    class  Engine;

    // ── SoftBodyComponent ─────────────────────────────────────────────────────
    // Pure data component for a Bullet soft body (deformable disc mesh).
    // PhysicsSystem handles initialization and per-frame update.
    class SoftBodyComponent
    {
    public:
        static constexpr const char*        componentType = "SoftBody";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::MAIN;

        SoftBodyComponent() = default;
        SoftBodyComponent(float radius, glm::vec3 pos, float mass, std::string texPath);
        ~SoftBodyComponent();

        // Non-copyable: owns Bullet soft-body pointer. Movable so std::vector
        // can relocate without triggering the destructor on a live body.
        SoftBodyComponent(const SoftBodyComponent&)            = delete;
        SoftBodyComponent& operator=(const SoftBodyComponent&) = delete;

        SoftBodyComponent(SoftBodyComponent&& o) noexcept
            : radius_(o.radius_), rings_(o.rings_), sectors_(o.sectors_)
            , mass_(o.mass_), initialPosition_(o.initialPosition_)
            , stiffness_(o.stiffness_), pressure_(o.pressure_)
            , texturePath_(std::move(o.texturePath_))
            , body_(std::move(o.body_)), softWorld_(o.softWorld_)
            , renderable_(std::move(o.renderable_))
            , lastTrackedCentroid_(o.lastTrackedCentroid_)
        {
            o.softWorld_ = nullptr;
        }

        SoftBodyComponent& operator=(SoftBodyComponent&& o) noexcept
        {
            if (this == &o) return *this;
            if (body_ && softWorld_) softWorld_->removeSoftBody(body_.get());

            radius_               = o.radius_;
            rings_                = o.rings_;
            sectors_              = o.sectors_;
            mass_                 = o.mass_;
            initialPosition_      = o.initialPosition_;
            stiffness_            = o.stiffness_;
            pressure_             = o.pressure_;
            texturePath_          = std::move(o.texturePath_);
            body_                 = std::move(o.body_);
            softWorld_            = o.softWorld_;
            renderable_           = std::move(o.renderable_);
            lastTrackedCentroid_  = o.lastTrackedCentroid_;

            o.softWorld_ = nullptr;
            return *this;
        }

        // ── System-facing API (called by PhysicsSystem) ───────────────────────
        void InitBody(btSoftRigidDynamicsWorld* world, Engine& engine);
        void UpdateBody(Transform& t);
        bool IsInitialized() const { return body_ != nullptr; }

        // Picker support — lets FindNodeByRenderable trace back to this node.
        std::shared_ptr<Renderable> GetRenderable() const { return renderable_; }

        // Returns the current centroid of the soft body from the Bullet node
        // positions.  Falls back to initialPosition_ if the body is not ready.
        glm::vec3 GetCentroid() const;

        // ── Editor inspector ──────────────────────────────────────────────────
        void InspectProperties(EditorPropertyVisitor& v);

        // ── Serialization ─────────────────────────────────────────────────────
        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(cereal::make_nvp("radius",    radius_),
               cereal::make_nvp("rings",     rings_),
               cereal::make_nvp("sectors",   sectors_),
               cereal::make_nvp("mass",      mass_),
               cereal::make_nvp("pos_x",     initialPosition_.x),
               cereal::make_nvp("pos_y",     initialPosition_.y),
               cereal::make_nvp("pos_z",     initialPosition_.z),
               cereal::make_nvp("stiffness", stiffness_),
               cereal::make_nvp("pressure",  pressure_),
               cereal::make_nvp("texture",   texturePath_));
        }

        template<typename Visitor>
        void Inspect(Visitor& v)
        {
            PROP      (mass_,          "Mass");
            PROP      (radius_,        "Radius");
            PROP      (stiffness_,     "Stiffness");
            PROP      (pressure_,      "Pressure");
            PROP_RO   (rings_,         "Rings");
            PROP_RO   (sectors_,       "Sectors");
            PROP      (texturePath_,   "Texture");
        }

    private:
        // ── Serialized ────────────────────────────────────────────────────────
        float       radius_          = 1.0f;
        int         rings_           = 3;
        int         sectors_         = 16;
        float       mass_            = 1.0f;
        glm::vec3   initialPosition_ = {0.f, 0.f, 0.f};
        float       stiffness_       = 0.05f;
        float       pressure_        = 0.0f;
        std::string texturePath_;

        // ── Runtime (not serialized, set by PhysicsSystem) ────────────────────
        std::unique_ptr<btSoftBody>         body_;
        btSoftRigidDynamicsWorld*           softWorld_  = nullptr;  // non-owning
        std::shared_ptr<SoftBodyRenderable> renderable_;
        glm::vec3 lastTrackedCentroid_ = {0.f, 0.f, 0.f};
    };

} // namespace ettycc

#endif
