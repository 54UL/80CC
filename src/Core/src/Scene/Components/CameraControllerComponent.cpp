#include <Scene/Components/CameraControllerComponent.hpp>
#include <UI/EditorPropertyVisitor.hpp>

namespace ettycc
{

void CameraControllerComponent::InspectProperties(EditorPropertyVisitor& v)
{
    PROP(zoom,      "Zoom");
    PROP(zoomSpeed, "Zoom Step");
    PROP(zoomMin,   "Zoom Min");
    PROP(zoomMax,   "Zoom Max");
}

} // namespace ettycc
