#include <Graphics/Rendering/Renderable.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <Engine.hpp>
#include <Graphics/Rendering/RenderLayerConfig.hpp>

namespace ettycc
{
    // Default Inspect -- shows Enabled flag then the underlying transform.
    // Sprite (and any future Renderable subclass) calls this via Renderable::Inspect(v)
    // before appending its own fields.
    void Renderable::Inspect(EditorPropertyVisitor& v)
    {
        PROP(enabled, "Enabled");

        // Layer dropdown -- pick which layer this object belongs to
        {
            auto& layerConfig = GetDependency(Engine)->renderLayerConfig_;
            int layerCount = layerConfig.GetActiveCount();

            // Find current layer index from bitmask (first set bit)
            int currentIdx = 0;
            for (int i = 0; i < layerCount; ++i)
            {
                if (layer & (1u << i)) { currentIdx = i; break; }
            }

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Layer");
            ImGui::SameLine(ImMax(80.f, ImGui::GetContentRegionAvail().x * 0.38f));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##Layer", layerConfig.GetName(currentIdx).c_str()))
            {
                for (int i = 0; i < layerCount; ++i)
                {
                    bool selected = (currentIdx == i);
                    if (ImGui::Selectable(layerConfig.GetName(i).c_str(), selected))
                        layer = 1u << i;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        PROP_SECTION("Transform");
        transform().Inspect(v);
    }

} // namespace ettycc
