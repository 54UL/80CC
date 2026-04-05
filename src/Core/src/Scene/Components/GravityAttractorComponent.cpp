#include <Scene/Components/GravityAttractorComponent.hpp>
#include <UI/EditorPropertyVisitor.hpp>

namespace ettycc
{
    void GravityAttractorComponent::InspectProperties(EditorPropertyVisitor& v)
    {
        PROP(position_, "Position");
        PROP(strength_, "Strength");
    }
} // namespace ettycc
