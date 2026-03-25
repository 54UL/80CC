#include <Graphics/Rendering/Renderable.hpp>
#include <UI/EditorPropertyVisitor.hpp>

namespace ettycc
{
    // Default Inspect — shows Enabled flag then the underlying transform.
    // Sprite (and any future Renderable subclass) calls this via Renderable::Inspect(v)
    // before appending its own fields.
    void Renderable::Inspect(EditorPropertyVisitor& v)
    {
        PROP(enabled, "Enabled");
        PROP_SECTION("Transform");
        underylingTransform.Inspect(v);
    }

} // namespace ettycc
