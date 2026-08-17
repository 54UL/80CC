#ifndef BOX_SELECTOR_HPP
#define BOX_SELECTOR_HPP

#include <imgui.h>
#include <algorithm>
#include <functional>

namespace ettycc
{
    // Reusable rectangular (marquee) box selection widget.
    // Works in screen-space (ImVec2 pixels). The caller is responsible for
    // converting domain objects to screen positions and calling HitTest().
    struct BoxSelector
    {
        bool   active       = false;
        ImVec2 start        = {};
        ImVec2 end          = {};
        bool   deselectMode = false;  // middle-mouse toggles subtract mode

        // Colors
        static constexpr ImU32 kFillAdd      = IM_COL32(100, 150, 255, 40);
        static constexpr ImU32 kLineAdd      = IM_COL32(100, 150, 255, 200);
        static constexpr ImU32 kFillSubtract = IM_COL32(255, 100, 100, 40);
        static constexpr ImU32 kLineSubtract = IM_COL32(255, 100, 100, 200);

        // -- Lifecycle ----------------------------------------------------

        void Begin(ImVec2 mousePos, bool shiftHeld = false)
        {
            active       = true;
            start        = mousePos;
            end          = mousePos;
            deselectMode = false;
        }

        void Cancel()
        {
            active       = false;
            deselectMode = false;
        }

        void Finish()
        {
            active       = false;
            deselectMode = false;
        }

        // -- Per-frame update ---------------------------------------------
        // Returns true while still active. Call every frame between Begin and
        // Finish/Cancel.  Handles middle-mouse toggle, escape/right-click cancel,
        // and left-mouse-release finish automatically.
        // Returns false when the selection ended (finished or cancelled).
        // |cancelled| is set to true if the selection was cancelled (Esc/RMB).

        bool Update(ImVec2 mousePos, bool* cancelled = nullptr)
        {
            if (!active) return false;

            end = mousePos;

            // Middle-mouse toggles deselect mode
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                deselectMode = !deselectMode;

            // Finish on left-mouse release
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                Finish();
                return false;
            }

            // Cancel on Escape or right-click
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                Cancel();
                if (cancelled) *cancelled = true;
                return false;
            }

            return true;
        }

        // -- Query --------------------------------------------------------

        ImVec2 GetMin() const
        {
            return { std::min(start.x, end.x), std::min(start.y, end.y) };
        }

        ImVec2 GetMax() const
        {
            return { std::max(start.x, end.x), std::max(start.y, end.y) };
        }

        bool HitTest(ImVec2 screenPoint) const
        {
            ImVec2 bMin = GetMin();
            ImVec2 bMax = GetMax();
            return screenPoint.x >= bMin.x && screenPoint.x <= bMax.x
                && screenPoint.y >= bMin.y && screenPoint.y <= bMax.y;
        }

        // -- Drawing -----------------------------------------------------

        void Draw(ImDrawList* dl) const
        {
            if (!active) return;
            ImVec2 bMin = GetMin();
            ImVec2 bMax = GetMax();
            ImU32 fillCol = deselectMode ? kFillSubtract : kFillAdd;
            ImU32 lineCol = deselectMode ? kLineSubtract : kLineAdd;
            dl->AddRectFilled(bMin, bMax, fillCol);
            dl->AddRect(bMin, bMax, lineCol, 0.f, 0, 1.5f);
        }
    };

} // namespace ettycc

#endif
