#include <Scene/Components/GravityAttractorComponent.hpp>
#include <UI/EditorPropertyVisitor.hpp>

namespace ettycc
{
    void GravityAttractorComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        PROP(position_,    "Position");
        PROP(strength_,    "Strength");
        PROP(innerRadius_, "Inner Radius");
        PROP(outerRadius_, "Outer Radius");
    }
} // namespace ettycc
