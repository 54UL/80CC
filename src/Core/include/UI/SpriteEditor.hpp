#ifndef SPRITE_EDITOR_HPP
#define SPRITE_EDITOR_HPP

#include <Graphics/Rendering/Entities/SpriteShape.hpp>
#include <imgui.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <set>
#include <vector>
#include <utility>

namespace ettycc
{
    class Engine;

    class SpriteEditor
    {
    public:
        SpriteEditor() = default;

        void Draw(const std::shared_ptr<Engine>& engine);

        bool isOpen = false;

    private:
        // ── Selection mode ───────────────────────────────────────────────
        enum class EditMode { Vertex, Edge };
        EditMode editMode_ = EditMode::Vertex;

        // ── Edge representation (ordered pair of vertex indices) ─────────
        struct Edge
        {
            int a = -1, b = -1;
            bool operator==(const Edge& o) const { return a == o.a && b == o.b; }
            bool operator!=(const Edge& o) const { return !(*this == o); }
            bool operator<(const Edge& o) const { return a < o.a || (a == o.a && b < o.b); }
            bool Valid() const { return a >= 0 && b >= 0; }

            static Edge Make(int x, int y) { return x < y ? Edge{x, y} : Edge{y, x}; }
            static Edge None() { return { -1, -1 }; }
        };

        // ── Mini-gizmo state ─────────────────────────────────────────────
        enum class GizmoAxis { None, X, Y, XY };
        GizmoAxis gizmoHovered_  = GizmoAxis::None;
        GizmoAxis gizmoDragging_ = GizmoAxis::None;
        ImVec2    gizmoDragStart_ = {};

        // ── Modal extrude state (Blender-like: press E, then drag) ──────
        bool      extruding_     = false;
        int       extrudeAnchor_ = -1;
        Edge      extrudeEdgeAnchor_ = Edge::None();
        std::vector<int> extrudeNewVerts_;  // indices of newly created verts
        ImVec2    extrudeStartMouse_ = {};

        // ── Canvas state ─────────────────────────────────────────────────
        SpriteShape  shape_         = SpriteShape::MakeQuad();
        int          selectedVert_  = -1;     // "active" vertex (last clicked)
        int          hoveredVert_   = -1;
        bool         draggingVert_  = false;

        std::set<int> selectedVerts_;         // multi-selection set

        Edge         selectedEdge_  = Edge::None();
        Edge         hoveredEdge_   = Edge::None();

        // ── Box (rectangle) selection ────────────────────────────────────
        bool      boxSelecting_    = false;
        ImVec2    boxStart_        = {};
        ImVec2    boxEnd_          = {};
        bool      boxDeselectMode_ = false;   // middle-mouse toggles deselect

        // ── Click-pending (distinguishes click from drag on empty space) ─
        bool      clickPending_    = false;
        ImVec2    clickPendingPos_ = {};

        // Canvas view
        glm::vec2    canvasOffset_  = { 0.f, 0.f };
        float        canvasZoom_    = 100.f;

        // ── Snap config ──────────────────────────────────────────────────
        bool  snapEnabled_    = true;
        float snapSize_       = 0.25f;
        bool  snapToGrid_     = true;
        bool  snapToVertices_ = false;
        float snapVertexDist_ = 0.1f;

        // ── Auto-merge config ────────────────────────────────────────────
        bool  autoMerge_      = true;
        float mergeDist_      = 0.05f;  // world-space threshold

        // ── UV editor state ──────────────────────────────────────────────
        int   selectedUVVert_ = -1;
        bool  draggingUV_     = false;
        float uvCanvasZoom_   = 150.f;

        // ── Material preview ────────────────────────────────────────────
        std::string   previewTexturePath_;
        GLuint        previewTextureHandle_ = 0;
        std::shared_ptr<Engine> frameEngine_;  // set during Draw(), cleared after

        // ── Context menu helper ──────────────────────────────────────────
        bool  wantsContextMenu_ = false;
        int   contextVert_      = -1;
        Edge  contextEdge_      = Edge::None();

        // ── Internal draw helpers ────────────────────────────────────────
        void DrawToolbar();
        void DrawShapePresets();
        void DrawSnapConfig();
        void DrawVertexCanvas(ImVec2 canvasPos, ImVec2 canvasSize);
        void DrawUVCanvas(ImVec2 canvasPos, ImVec2 canvasSize);
        void DrawVertexInspector();
        void DrawShapeInfo();
        void DrawGeometryOps();
        void DrawMaterialPreview();
        void DrawMiniGizmo(ImDrawList* dl, ImVec2 origin, ImVec2 canvasPos, ImVec2 canvasSize);

        // ── Vertex operations ────────────────────────────────────────────
        void AddVertexAtWorld(glm::vec2 worldPos);
        void DeleteVertex(int index);
        void DeleteSelectedVertex();
        void DeleteSelectedVerts();   // batch delete for multi-selection

        // ── Geometry operations ──────────────────────────────────────────
        void ExtrudeVertex(int index, glm::vec2 direction);
        void ExtrudeEdge(Edge edge, glm::vec2 direction);
        void SubdivideEdge(Edge edge);
        void MergeVertices(int a, int b);
        void FlipEdge(Edge edge);
        void DissolveVertex(int index);
        void DissolveSelectedVerts();  // batch dissolve for multi-selection
        void DissolveEdge(Edge edge);

        // ── Auto-merge / edge-clip on extrude confirm ────────────────────
        void AutoMergeExtruded();
        int  FindNearestVertex(glm::vec2 pos, float maxDist, const std::set<int>& exclude) const;
        int  SplitEdgeAtPoint(Edge edge, glm::vec2 point);
        float PointToEdgeDistWorld(glm::vec2 p, glm::vec2 a, glm::vec2 b, float& outT) const;

        // ── Local-space helpers ──────────────────────────────────────────
        glm::vec2 ComputeVertexNormal(int index) const;
        glm::vec2 ComputeEdgeNormal(Edge edge) const;
        glm::vec2 ComputeEdgeTangent(Edge edge) const;

        // ── Edge helpers ─────────────────────────────────────────────────
        std::set<Edge> CollectEdges() const;
        float PointToEdgeDist(ImVec2 point, ImVec2 edgeA, ImVec2 edgeB) const;

        // ── Selection helpers ────────────────────────────────────────────
        bool IsVertSelected(int i) const { return selectedVerts_.count(i) > 0; }
        void SelectVert(int i, bool additive);
        void ClearSelection();
        void RemapSelectionAfterDelete(int deletedIdx);

        // ── Coordinate helpers ───────────────────────────────────────────
        ImVec2 WorldToCanvas(glm::vec2 world, ImVec2 canvasPos, ImVec2 canvasSize) const;
        glm::vec2 CanvasToWorld(ImVec2 screen, ImVec2 canvasPos, ImVec2 canvasSize) const;
        ImVec2 UVToCanvas(glm::vec2 uv, ImVec2 canvasPos, ImVec2 canvasSize) const;
        glm::vec2 CanvasToUV(ImVec2 screen, ImVec2 canvasPos, ImVec2 canvasSize) const;

        float SnapValue(float val, float grid) const;

        void MarkCustom();
    };

} // namespace ettycc

#endif
