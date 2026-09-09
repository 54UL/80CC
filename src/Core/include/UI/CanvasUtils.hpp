#pragma once

// Shared coordinate-transform utilities used by SpriteEditor and PolygonEditor.
// All functions are inline free functions -- no linker issues when included
// from multiple translation units.

#include <glm/glm.hpp>
#include <imgui.h>

namespace ettycc::canvas
{
    // -- World <-> Canvas coordinate conversions ------------------------------

    inline ImVec2 WorldToCanvas(glm::vec2 world, ImVec2 cp, ImVec2 cs,
                                glm::vec2 canvasOffset, float canvasZoom)
    {
        float cx = cp.x + cs.x * 0.5f + (world.x + canvasOffset.x) * canvasZoom;
        float cy = cp.y + cs.y * 0.5f - (world.y + canvasOffset.y) * canvasZoom;
        return { cx, cy };
    }

    inline glm::vec2 CanvasToWorld(ImVec2 screen, ImVec2 cp, ImVec2 cs,
                                   glm::vec2 canvasOffset, float canvasZoom)
    {
        float x = (screen.x - cp.x - cs.x * 0.5f) / canvasZoom - canvasOffset.x;
        float y = -(screen.y - cp.y - cs.y * 0.5f) / canvasZoom - canvasOffset.y;
        return { x, y };
    }

    // -- UV <-> Canvas coordinate conversions ---------------------------------

    inline ImVec2 UVToCanvas(glm::vec2 uv, ImVec2 cp, ImVec2 cs,
                             float uvCanvasZoom)
    {
        float cx = cp.x + 20.f + uv.x * uvCanvasZoom;
        float cy = cp.y + cs.y - 20.f - uv.y * uvCanvasZoom;
        return { cx, cy };
    }

    inline glm::vec2 CanvasToUV(ImVec2 screen, ImVec2 cp, ImVec2 cs,
                                float uvCanvasZoom)
    {
        float u = (screen.x - cp.x - 20.f) / uvCanvasZoom;
        float v = -(screen.y - cp.y - cs.y + 20.f) / uvCanvasZoom;
        return { u, v };
    }

    // -- Snap helper ----------------------------------------------------------

    inline float SnapValue(float val, float grid)
    {
        return glm::round(val / grid) * grid;
    }

} // namespace ettycc::canvas
