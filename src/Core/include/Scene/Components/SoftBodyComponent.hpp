#ifndef SOFT_BODY_COMPONENT_HPP
#define SOFT_BODY_COMPONENT_HPP

#include <Scene/Api.hpp>
#include <Scene/PropertySystem.hpp>
#include <Physics/IPhysicsSoftBody.hpp>
#include <Graphics/Rendering/Renderable.hpp>

namespace ettycc { namespace physics { class IPhysicsWorld; } }

#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>

#include <memory>
#include <string>

namespace ettycc
{
    struct EditorPropertyVisitor;
    class  Engine;

    class SoftBodyComponent
    {
    public:
        static constexpr const char*        componentType = "SoftBody";
        static constexpr ProcessingChannel  channel       = ProcessingChannel::MAIN;

        SoftBodyComponent() = default;
        SoftBodyComponent(float radius, glm::vec3 pos, float mass, std::string texPath);
        ~SoftBodyComponent();

        SoftBodyComponent(const SoftBodyComponent&)            = delete;
        SoftBodyComponent& operator=(const SoftBodyComponent&) = delete;

        SoftBodyComponent(SoftBodyComponent&& o) noexcept
            : radius_(o.radius_), rings_(o.rings_), sectors_(o.sectors_)
            , mass_(o.mass_), initialPosition_(o.initialPosition_)
            , stiffness_(o.stiffness_), pressure_(o.pressure_)
            , texturePath_(std::move(o.texturePath_))
            , body_(std::move(o.body_))
            , renderable_(std::move(o.renderable_))
            , lastTrackedCentroid_(o.lastTrackedCentroid_)
        {}

        SoftBodyComponent& operator=(SoftBodyComponent&& o) noexcept
        {
            if (this == &o) return *this;
            body_.reset();

            radius_               = o.radius_;
            rings_                = o.rings_;
            sectors_              = o.sectors_;
            mass_                 = o.mass_;
            initialPosition_      = o.initialPosition_;
            stiffness_            = o.stiffness_;
            pressure_             = o.pressure_;
            texturePath_          = std::move(o.texturePath_);
            body_                 = std::move(o.body_);
            renderable_           = std::move(o.renderable_);
            lastTrackedCentroid_  = o.lastTrackedCentroid_;

            return *this;
        }

        // -- System-facing API -------------------------------------------------
        void InitBody(physics::IPhysicsWorld& world, Engine& engine);
        void UpdateBody(Transform& t);
        bool IsInitialized() const { return body_ != nullptr; }
        void ReleaseBody() { body_.reset(); }

        std::shared_ptr<Renderable> GetRenderable() const { return renderable_; }

        // Returns the abstract soft body for wireframe/gizmo rendering.
        physics::IPhysicsSoftBody* GetSoftBody() const { return body_.get(); }

        glm::vec3 GetCentroid() const;

        // -- Editor inspector --------------------------------------------------
        void InspectProperties(EditorPropertyVisitor& v);

        // -- Serialization -----------------------------------------------------
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
        // -- Serialized --------------------------------------------------------
        float       radius_          = 1.0f;
        int         rings_           = 3;
        int         sectors_         = 16;
        float       mass_            = 1.0f;
        glm::vec3   initialPosition_ = {0.f, 0.f, 0.f};
        float       stiffness_       = 0.05f;
        float       pressure_        = 0.0f;
        std::string texturePath_;

        // -- Runtime -----------------------------------------------------------
        std::unique_ptr<physics::IPhysicsSoftBody> body_;
        std::shared_ptr<Renderable>                renderable_;
        glm::vec3 lastTrackedCentroid_ = {0.f, 0.f, 0.f};
    };

} // namespace ettycc

#endif
