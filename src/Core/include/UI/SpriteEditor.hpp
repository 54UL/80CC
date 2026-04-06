#ifndef SPRITE_EDITOR_HPP
#define SPRITE_EDITOR_HPP

#include <Graphics/Rendering/Entities/SpriteShape.hpp>
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>

namespace ettycc
{
    class Engine;

    class SpriteEditor
    {
    public:
        SpriteEditor() = default;

        // Draw the full sprite editor window. Returns true if the window is open.
        void Draw(const std::shared_ptr<Engine>& engine);

        bool isOpen = false;

    private:
        // ── Canvas state ─────────────────────────────────────────────────
        SpriteShape  shape_         = SpriteShape::MakeQuad();
        int          selectedVert_  = -1;
        bool         draggingVert_  = false;

        // Canvas view
        glm::vec2    canvasOffset_  = { 0.f, 0.f };
        float        canvasZoom_    = 100.f; // pixels per unit

        // ── Snap config ──────────────────────────────────────────────────
        bool  snapEnabled_    = true;
        float snapSize_       = 0.25f;
        bool  snapToGrid_     = true;
        bool  snapToVertices_ = false;
        float snapVertexDist_ = 0.1f; // in world units

        // ── UV editor state ──────────────────────────────────────────────
        int   selectedUVVert_ = -1;
        bool  draggingUV_     = false;
        float uvCanvasZoom_   = 150.f;

        // ── Internal draw helpers ────────────────────────────────────────
        void DrawToolbar();
        void DrawShapePresets();
        void DrawSnapConfig();
        void DrawVertexCanvas(ImVec2 canvasPos, ImVec2 canvasSize);
        void DrawUVCanvas(ImVec2 canvasPos, ImVec2 canvasSize);
        void DrawVertexInspector();
        void DrawShapeInfo();

        // ── Coordinate helpers ───────────────────────────────────────────
        ImVec2 WorldToCanvas(glm::vec2 world, ImVec2 canvasPos, ImVec2 canvasSize) const;
        glm::vec2 CanvasToWorld(ImVec2 screen, ImVec2 canvasPos, ImVec2 canvasSize) const;
        ImVec2 UVToCanvas(glm::vec2 uv, ImVec2 canvasPos, ImVec2 canvasSize) const;
        glm::vec2 CanvasToUV(ImVec2 screen, ImVec2 canvasPos, ImVec2 canvasSize) const;

        float SnapValue(float val, float grid) const;
    };

} // namespace ettycc

#endif
