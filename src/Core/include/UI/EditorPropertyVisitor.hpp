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

namespace ettycc
{
    struct EditorPropertyVisitor
    {
        bool anyChanged    = false;  // set to true if any widget was modified this frame
        int  propertyCount = 0;      // incremented for every visible property rendered

        // Called by PROP_SECTION — draws a labelled separator in the inspector.
        void Section(const char* label) { ImGui::SeparatorText(label); }

        template<typename T>
        void Property(const char* label, T& value, uint32_t flags = PROP_NONE)
        {
            if (flags & PROP_HIDDEN) return;
            ++propertyCount;

            bool ro = (flags & PROP_READ_ONLY) != 0;

            ImGui::PushID(label);
            ImGui::BeginDisabled(ro);

            if constexpr (std::is_same_v<T, float>)
            {
                ImGui::SetNextItemWidth(-1.f);
                anyChanged |= ImGui::DragFloat(label, &value, 0.01f);
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                ImGui::SetNextItemWidth(-1.f);
                anyChanged |= ImGui::DragInt(label, &value);
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                int tmp = static_cast<int>(value);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::DragInt(label, &tmp) && tmp >= 0)
                { value = static_cast<uint32_t>(tmp); anyChanged = true; }
            }
            else if constexpr (std::is_same_v<T, uint64_t>)
            {
                ImGui::LabelText(label, "%llu", static_cast<unsigned long long>(value));
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                anyChanged |= ImGui::Checkbox(label, &value);
            }
            else if constexpr (std::is_same_v<T, glm::vec2>)
            {
                ImGui::SetNextItemWidth(-1.f);
                anyChanged |= ImGui::DragFloat2(label, &value.x, 0.01f);
            }
            else if constexpr (std::is_same_v<T, glm::vec3>)
            {
                ImGui::SetNextItemWidth(-1.f);
                anyChanged |= ImGui::DragFloat3(label, &value.x, 0.01f);
            }
            else if constexpr (std::is_same_v<T, glm::vec4>)
            {
                ImGui::SetNextItemWidth(-1.f);
                anyChanged |= ImGui::DragFloat4(label, &value.x, 0.01f);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                char buf[256] = {};
                strncpy(buf, value.c_str(), sizeof(buf) - 1);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::InputText(label, buf, sizeof(buf)))
                { value = buf; anyChanged = true; }
            }
            else
            {
                ImGui::TextDisabled("<%s: unsupported type>", label);
            }

            ImGui::EndDisabled();
            ImGui::PopID();
        }
    };

} // namespace ettycc

#endif
