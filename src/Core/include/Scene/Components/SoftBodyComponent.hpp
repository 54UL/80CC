#ifndef SOFT_BODY_COMPONENT_HPP
#define SOFT_BODY_COMPONENT_HPP

#include <Scene/NodeComponent.hpp>
#include <Scene/PropertySystem.hpp>
#include <Graphics/Rendering/Entities/SoftBodyRenderable.hpp>

#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBody.h>

#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/json.hpp>

#include <memory>
#include <string>
#include <vector>

namespace ettycc
{
    class SoftBodyComponent : public NodeComponent
    {
    public:
        static constexpr const char* componentType = "SoftBody";

        // Default constructor required by cereal
        SoftBodyComponent() = default;

        SoftBodyComponent(float radius, glm::vec3 pos, float mass, std::string texPath);

        ~SoftBodyComponent() override;

        // NodeComponent interface
        NodeComponentInfo GetComponentInfo() override;
        void OnStart(std::shared_ptr<Engine> engineInstance) override;
        void OnUpdate(float deltaTime) override;
        void InspectProperties(EditorPropertyVisitor& v) override;

        // Unified property inspector — drives both editor widgets and serialization
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
            PROP_F    (componentId_,   "ID", ettycc::PROP_READ_ONLY | ettycc::PROP_NO_SERIAL);
        }

        // Cereal split save/load so we preserve exact key names
        template<class Archive>
        void save(Archive& ar) const
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

        template<class Archive>
        void load(Archive& ar)
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

    private:
        // Serialized fields
        float     radius_          = 1.0f;
        int       rings_           = 3;
        int       sectors_         = 16;
        float     mass_            = 1.0f;
        glm::vec3 initialPosition_ = { 0.0f, 0.0f, 0.0f };
        float     stiffness_       = 0.05f;  // 0=jelly, 1=rigid
        float     pressure_        = 0.0f;   // unused for flat disc (kept for serialisation compat)
        std::string texturePath_;
        uint64_t  componentId_     = 0;

        // Runtime — not serialized
        btSoftBody*               body_       = nullptr;
        btSoftRigidDynamicsWorld* softWorld_  = nullptr;
        std::shared_ptr<SoftBodyRenderable> renderable_;
    };

} // namespace ettycc

#endif
