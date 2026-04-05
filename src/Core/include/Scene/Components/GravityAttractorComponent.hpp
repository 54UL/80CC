#ifndef GRAVITY_ATTRACTOR_COMPONENT_HPP
#define GRAVITY_ATTRACTOR_COMPONENT_HPP

#include <Scene/PropertySystem.hpp>
#include <Scene/Api.hpp>
#include <glm/glm.hpp>
#include <cereal/archives/json.hpp>

namespace ettycc
{
    struct EditorPropertyVisitor;

    class GravityAttractorComponent
    {
    public:
        static constexpr const char*       componentType = "GravityAttractor";
        static constexpr ProcessingChannel channel       = ProcessingChannel::MAIN;

        GravityAttractorComponent() = default;
        GravityAttractorComponent(glm::vec3 position, float strength)
            : position_(position), strength_(strength) {}

        glm::vec3 GetPosition() const { return position_; }
        float     GetStrength() const { return strength_; }

        void InspectProperties(EditorPropertyVisitor& v);

        template <class Archive>
        void serialize(Archive& ar)
        {
            ar(cereal::make_nvp("pos_x",    position_.x),
               cereal::make_nvp("pos_y",    position_.y),
               cereal::make_nvp("pos_z",    position_.z),
               cereal::make_nvp("strength", strength_));
        }

    private:
        glm::vec3 position_ = { 0.0f, 0.0f, 0.0f };
        float     strength_ = 50.0f;
    };

} // namespace ettycc

#endif
