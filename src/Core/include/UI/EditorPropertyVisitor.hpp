#ifndef EDITOR_PROPERTY_VISITOR_HPP
#define EDITOR_PROPERTY_VISITOR_HPP

// ── EditorPropertyVisitor ─────────────────────────────────────────────────────
// ImGui-rendering visitor.  Only compiled when building the editor.
// Components include this ONLY in their .cpp — never in their .hpp — so that
// imgui never leaks into game-build headers.
//
// Usage:
//   #include <UI/EditorPropertyVisitor.hpp>
//
//   void MyComponent::InspectProperties(EditorPropertyVisitor& v)
//   {
//       PROP(someFloat_,  "Some Float");
//       PROP(someVec3_,   "Some Vec3");
//   }

#include <Scene/PropertySystem.hpp>
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>
#include <type_traits>
#include <cstring>

#include "imgui_internal.h"

namespace ettycc
{
    struct EditorPropertyVisitor
    {
        bool anyChanged      = false; // true if any widget value changed this frame
        int  propertyCount   = 0;     // incremented for every rendered property
        bool dragActivated   = false; // rising edge: user just started dragging a widget
        bool dragDeactivated = false; // falling edge: user just released a drag widget
        bool isDragging      = false; // level: a drag widget is currently held down

        void Section(const char* label) { ImGui::SeparatorText(label); }

        template<typename T>
        void Property(const char* label, T& value, uint32_t flags = PROP_NONE)
        {
            if (flags & PROP_HIDDEN) return;
            ++propertyCount;

            const bool ro = (flags & PROP_READ_ONLY) != 0;

            // ── Two-column layout: label left, widget right ────────────────
            const float avail      = ImGui::GetContentRegionAvail().x;
            const float labelWidth = ImMax(80.f, avail * 0.38f);
            const float widgetWidth = avail - labelWidth - ImGui::GetStyle().ItemSpacing.x;

            ImGui::PushID(label);
            ImGui::AlignTextToFramePadding();

            if (ro)
                ImGui::TextDisabled("%s", label);
            else
                ImGui::TextUnformatted(label);

            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(widgetWidth);
            ImGui::BeginDisabled(ro);

            char wid[128];
            snprintf(wid, sizeof(wid), "##%s", label);

            if constexpr (std::is_same_v<T, float>)
            {
                anyChanged |= ImGui::DragFloat(wid, &value, 0.01f);
                TrackDrag();
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                anyChanged |= ImGui::DragInt(wid, &value);
                TrackDrag();
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                int tmp = static_cast<int>(value);
                if (ImGui::DragInt(wid, &tmp) && tmp >= 0)
                { value = static_cast<uint32_t>(tmp); anyChanged = true; }
                TrackDrag();
            }
            else if constexpr (std::is_same_v<T, uint64_t>)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(value));
                ImGui::SetNextItemWidth(widgetWidth);
                ImGui::InputText(wid, buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                anyChanged |= ImGui::Checkbox(wid, &value);
            }
            else if constexpr (std::is_same_v<T, glm::vec2>)
            {
                anyChanged |= ImGui::DragFloat2(wid, &value.x, 0.01f);
                TrackDrag();
            }
            else if constexpr (std::is_same_v<T, glm::vec3>)
            {
                anyChanged |= ImGui::DragFloat3(wid, &value.x, 0.01f);
                TrackDrag();
            }
            else if constexpr (std::is_same_v<T, glm::vec4>)
            {
                anyChanged |= ImGui::DragFloat4(wid, &value.x, 0.01f);
                TrackDrag();
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                char buf[256] = {};
                strncpy(buf, value.c_str(), sizeof(buf) - 1);
                if (ImGui::InputText(wid, buf, sizeof(buf)))
                { value = buf; anyChanged = true; }
            }
            else
            {
                ImGui::TextDisabled("<unsupported>");
            }

            ImGui::EndDisabled();
            ImGui::PopID();
        }

    private:
        void TrackDrag()
        {
            isDragging     |= ImGui::IsItemActive();
            dragActivated  |= ImGui::IsItemActivated();
            dragDeactivated|= ImGui::IsItemDeactivated();
        }
    };

} // namespace ettycc

#endif
