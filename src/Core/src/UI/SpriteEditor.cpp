#include <UI/SpriteEditor.hpp>
#include <UI/Widgets/PathFieldWidget.hpp>
#include <Engine.hpp>
#include <Scene/Assets/ResourceCache.hpp>
#include <Dependencies/Globals.hpp>
#include <Dependency.hpp>
#include <imgui_internal.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>

namespace ettycc
{
    // ── Coordinate conversions ───────────────────────────────────────────────

    ImVec2 SpriteEditor::WorldToCanvas(glm::vec2 world, ImVec2 cp, ImVec2 cs) const
    {
        float cx = cp.x + cs.x * 0.5f + (world.x + canvasOffset_.x) * canvasZoom_;
        float cy = cp.y + cs.y * 0.5f - (world.y + canvasOffset_.y) * canvasZoom_;
        return { cx, cy };
    }

    glm::vec2 SpriteEditor::CanvasToWorld(ImVec2 screen, ImVec2 cp, ImVec2 cs) const
    {
        float x = (screen.x - cp.x - cs.x * 0.5f) / canvasZoom_ - canvasOffset_.x;
        float y = -(screen.y - cp.y - cs.y * 0.5f) / canvasZoom_ - canvasOffset_.y;
        return { x, y };
    }

    ImVec2 SpriteEditor::UVToCanvas(glm::vec2 uv, ImVec2 cp, ImVec2 cs) const
    {
        float cx = cp.x + 20.f + uv.x * uvCanvasZoom_;
        float cy = cp.y + cs.y - 20.f - uv.y * uvCanvasZoom_;
        return { cx, cy };
    }

    glm::vec2 SpriteEditor::CanvasToUV(ImVec2 screen, ImVec2 cp, ImVec2 cs) const
    {
        float u = (screen.x - cp.x - 20.f) / uvCanvasZoom_;
        float v = -(screen.y - cp.y - cs.y + 20.f) / uvCanvasZoom_;
        return { u, v };
    }

    float SpriteEditor::SnapValue(float val, float grid) const
    {
        return std::round(val / grid) * grid;
    }

    void SpriteEditor::MarkCustom()
    {
        shape_.preset = SpriteShape::Preset::Custom;
        shape_.name = "Custom";
    }

    // ── Selection helpers ────────────────────────────────────────────────────

    void SpriteEditor::SelectVert(int i, bool additive)
    {
        if (i < 0 || i >= static_cast<int>(shape_.vertices.size())) return;
        if (!additive) selectedVerts_.clear();
        selectedVerts_.insert(i);
        selectedVert_ = i;
    }

    void SpriteEditor::ClearSelection()
    {
        selectedVerts_.clear();
        selectedVert_ = -1;
        selectedEdge_ = Edge::None();
    }

    void SpriteEditor::RemapSelectionAfterDelete(int deletedIdx)
    {
        std::set<int> remapped;
        for (int v : selectedVerts_)
        {
            if (v == deletedIdx) continue;
            remapped.insert(v > deletedIdx ? v - 1 : v);
        }
        selectedVerts_ = remapped;

        if (selectedVert_ == deletedIdx) selectedVert_ = -1;
        else if (selectedVert_ > deletedIdx) --selectedVert_;

        if (selectedEdge_.a == deletedIdx || selectedEdge_.b == deletedIdx)
            selectedEdge_ = Edge::None();
        else
        {
            if (selectedEdge_.a > deletedIdx) --selectedEdge_.a;
            if (selectedEdge_.b > deletedIdx) --selectedEdge_.b;
        }
    }

    // ── Edge helpers ─────────────────────────────────────────────────────────

    std::set<SpriteEditor::Edge> SpriteEditor::CollectEdges() const
    {
        std::set<Edge> edges;
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            int a = static_cast<int>(shape_.indices[i]);
            int b = static_cast<int>(shape_.indices[i + 1]);
            int c = static_cast<int>(shape_.indices[i + 2]);
            edges.insert(Edge::Make(a, b));
            edges.insert(Edge::Make(b, c));
            edges.insert(Edge::Make(c, a));
        }
        return edges;
    }

    float SpriteEditor::PointToEdgeDist(ImVec2 p, ImVec2 a, ImVec2 b) const
    {
        float dx = b.x - a.x, dy = b.y - a.y;
        float lenSq = dx * dx + dy * dy;
        if (lenSq < 0.001f) return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
        float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
        t = glm::clamp(t, 0.f, 1.f);
        float projX = a.x + t * dx, projY = a.y + t * dy;
        float ex = p.x - projX, ey = p.y - projY;
        return std::sqrt(ex * ex + ey * ey);
    }

    float SpriteEditor::PointToEdgeDistWorld(glm::vec2 p, glm::vec2 a, glm::vec2 b, float& outT) const
    {
        glm::vec2 ab = b - a;
        float lenSq = glm::dot(ab, ab);
        if (lenSq < 1e-8f) { outT = 0.f; return glm::length(p - a); }
        outT = glm::clamp(glm::dot(p - a, ab) / lenSq, 0.f, 1.f);
        glm::vec2 proj = a + ab * outT;
        return glm::length(p - proj);
    }

    // ── Local-space helpers ──────────────────────────────────────────────────

    glm::vec2 SpriteEditor::ComputeVertexNormal(int index) const
    {
        if (index < 0 || index >= static_cast<int>(shape_.vertices.size()))
            return { 0.f, 1.f };

        glm::vec2 avgNormal(0.f);
        int count = 0;

        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            int tri[3] = {
                static_cast<int>(shape_.indices[i]),
                static_cast<int>(shape_.indices[i + 1]),
                static_cast<int>(shape_.indices[i + 2])
            };

            for (int e = 0; e < 3; ++e)
            {
                if (tri[e] == index)
                {
                    glm::vec2 p  = shape_.vertices[tri[e]].position;
                    glm::vec2 p1 = shape_.vertices[tri[(e + 1) % 3]].position;
                    glm::vec2 p2 = shape_.vertices[tri[(e + 2) % 3]].position;

                    glm::vec2 e1 = p1 - p;
                    glm::vec2 e2 = p2 - p;
                    glm::vec2 bisect = glm::normalize(e1) + glm::normalize(e2);
                    if (glm::length(bisect) > 0.001f)
                    {
                        avgNormal -= glm::normalize(bisect);
                        ++count;
                    }
                }
            }
        }

        if (count > 0 && glm::length(avgNormal) > 0.001f)
            return glm::normalize(avgNormal);
        return { 0.f, 1.f };
    }

    glm::vec2 SpriteEditor::ComputeEdgeNormal(Edge edge) const
    {
        if (!edge.Valid()) return { 0.f, 1.f };
        glm::vec2 a = shape_.vertices[edge.a].position;
        glm::vec2 b = shape_.vertices[edge.b].position;
        glm::vec2 dir = b - a;
        if (glm::length(dir) < 0.001f) return { 0.f, 1.f };
        return glm::normalize(glm::vec2(-dir.y, dir.x));
    }

    glm::vec2 SpriteEditor::ComputeEdgeTangent(Edge edge) const
    {
        if (!edge.Valid()) return { 1.f, 0.f };
        glm::vec2 a = shape_.vertices[edge.a].position;
        glm::vec2 b = shape_.vertices[edge.b].position;
        glm::vec2 dir = b - a;
        if (glm::length(dir) < 0.001f) return { 1.f, 0.f };
        return glm::normalize(dir);
    }

    // ── Vertex operations ────────────────────────────────────────────────────

    void SpriteEditor::AddVertexAtWorld(glm::vec2 worldPos)
    {
        if (snapEnabled_ && snapToGrid_)
        {
            worldPos.x = SnapValue(worldPos.x, snapSize_);
            worldPos.y = SnapValue(worldPos.y, snapSize_);
        }

        shape_.vertices.push_back({ worldPos, { 0.5f, 0.5f } });
        MarkCustom();
        int idx = static_cast<int>(shape_.vertices.size()) - 1;
        SelectVert(idx, false);
    }

    void SpriteEditor::DeleteVertex(int index)
    {
        if (index < 0 || index >= static_cast<int>(shape_.vertices.size())) return;

        shape_.vertices.erase(shape_.vertices.begin() + index);

        std::vector<unsigned int> newIndices;
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            unsigned int a = shape_.indices[i];
            unsigned int b = shape_.indices[i + 1];
            unsigned int c = shape_.indices[i + 2];
            if (a == static_cast<unsigned int>(index) ||
                b == static_cast<unsigned int>(index) ||
                c == static_cast<unsigned int>(index))
                continue;
            if (a > static_cast<unsigned int>(index)) --a;
            if (b > static_cast<unsigned int>(index)) --b;
            if (c > static_cast<unsigned int>(index)) --c;
            newIndices.push_back(a);
            newIndices.push_back(b);
            newIndices.push_back(c);
        }
        shape_.indices = newIndices;
        MarkCustom();

        RemapSelectionAfterDelete(index);
    }

    void SpriteEditor::DeleteSelectedVertex()
    {
        DeleteVertex(selectedVert_);
    }

    void SpriteEditor::DeleteSelectedVerts()
    {
        // Delete in reverse order to keep indices valid
        std::vector<int> sorted(selectedVerts_.begin(), selectedVerts_.end());
        std::sort(sorted.rbegin(), sorted.rend());
        for (int idx : sorted)
            DeleteVertex(idx);
        ClearSelection();
    }

    // ── Geometry operations ──────────────────────────────────────────────────

    void SpriteEditor::ExtrudeVertex(int index, glm::vec2 direction)
    {
        if (index < 0 || index >= static_cast<int>(shape_.vertices.size())) return;

        auto& src = shape_.vertices[index];
        SpriteVertex newVert = { src.position + direction, src.uv };
        shape_.vertices.push_back(newVert);
        int newIdx = static_cast<int>(shape_.vertices.size()) - 1;
        selectedVert_ = newIdx;
        selectedVerts_.clear();
        selectedVerts_.insert(newIdx);
        extrudeNewVerts_.push_back(newIdx);
        MarkCustom();
    }

    void SpriteEditor::ExtrudeEdge(Edge edge, glm::vec2 direction)
    {
        if (!edge.Valid()) return;
        int n = static_cast<int>(shape_.vertices.size());
        if (edge.a >= n || edge.b >= n) return;

        SpriteVertex va = shape_.vertices[edge.a];
        SpriteVertex vb = shape_.vertices[edge.b];
        va.position += direction;
        vb.position += direction;
        shape_.vertices.push_back(va);
        shape_.vertices.push_back(vb);

        int newA = static_cast<int>(shape_.vertices.size()) - 2;
        int newB = static_cast<int>(shape_.vertices.size()) - 1;

        shape_.indices.push_back(static_cast<unsigned int>(edge.a));
        shape_.indices.push_back(static_cast<unsigned int>(edge.b));
        shape_.indices.push_back(static_cast<unsigned int>(newB));

        shape_.indices.push_back(static_cast<unsigned int>(edge.a));
        shape_.indices.push_back(static_cast<unsigned int>(newB));
        shape_.indices.push_back(static_cast<unsigned int>(newA));

        selectedEdge_ = Edge::Make(newA, newB);
        extrudeNewVerts_.push_back(newA);
        extrudeNewVerts_.push_back(newB);
        MarkCustom();
    }

    void SpriteEditor::SubdivideEdge(Edge edge)
    {
        if (!edge.Valid()) return;
        int n = static_cast<int>(shape_.vertices.size());
        if (edge.a >= n || edge.b >= n) return;

        auto& va = shape_.vertices[edge.a];
        auto& vb = shape_.vertices[edge.b];
        SpriteVertex mid;
        mid.position = (va.position + vb.position) * 0.5f;
        mid.uv = (va.uv + vb.uv) * 0.5f;
        shape_.vertices.push_back(mid);
        int midIdx = static_cast<int>(shape_.vertices.size()) - 1;

        std::vector<unsigned int> newIndices;
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            int ia = static_cast<int>(shape_.indices[i]);
            int ib = static_cast<int>(shape_.indices[i + 1]);
            int ic = static_cast<int>(shape_.indices[i + 2]);

            int edgePos = -1;
            if (Edge::Make(ia, ib) == edge) edgePos = 0;
            else if (Edge::Make(ib, ic) == edge) edgePos = 1;
            else if (Edge::Make(ic, ia) == edge) edgePos = 2;

            if (edgePos < 0)
            {
                newIndices.push_back(shape_.indices[i]);
                newIndices.push_back(shape_.indices[i + 1]);
                newIndices.push_back(shape_.indices[i + 2]);
            }
            else
            {
                int v0, v1, v2;
                if (edgePos == 0) { v0 = ia; v1 = ib; v2 = ic; }
                else if (edgePos == 1) { v0 = ib; v1 = ic; v2 = ia; }
                else { v0 = ic; v1 = ia; v2 = ib; }

                newIndices.push_back(static_cast<unsigned int>(v0));
                newIndices.push_back(static_cast<unsigned int>(midIdx));
                newIndices.push_back(static_cast<unsigned int>(v2));

                newIndices.push_back(static_cast<unsigned int>(midIdx));
                newIndices.push_back(static_cast<unsigned int>(v1));
                newIndices.push_back(static_cast<unsigned int>(v2));
            }
        }
        shape_.indices = newIndices;
        SelectVert(midIdx, false);
        editMode_ = EditMode::Vertex;
        MarkCustom();
    }

    void SpriteEditor::MergeVertices(int a, int b)
    {
        if (a < 0 || b < 0) return;
        int n = static_cast<int>(shape_.vertices.size());
        if (a >= n || b >= n || a == b) return;

        shape_.vertices[a].position = (shape_.vertices[a].position + shape_.vertices[b].position) * 0.5f;
        shape_.vertices[a].uv = (shape_.vertices[a].uv + shape_.vertices[b].uv) * 0.5f;

        for (auto& idx : shape_.indices)
        {
            if (idx == static_cast<unsigned int>(b)) idx = static_cast<unsigned int>(a);
        }

        std::vector<unsigned int> newIndices;
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            unsigned int i0 = shape_.indices[i], i1 = shape_.indices[i+1], i2 = shape_.indices[i+2];
            if (i0 != i1 && i1 != i2 && i0 != i2)
            {
                newIndices.push_back(i0);
                newIndices.push_back(i1);
                newIndices.push_back(i2);
            }
        }
        shape_.indices = newIndices;

        shape_.vertices.erase(shape_.vertices.begin() + b);
        for (auto& idx : shape_.indices)
        {
            if (idx > static_cast<unsigned int>(b)) --idx;
        }
        if (a > b) --a;

        SelectVert(a, false);
        MarkCustom();
    }

    void SpriteEditor::FlipEdge(Edge edge)
    {
        if (!edge.Valid()) return;

        int tri1 = -1, tri2 = -1;
        int opp1 = -1, opp2 = -1;

        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            int ia = static_cast<int>(shape_.indices[i]);
            int ib = static_cast<int>(shape_.indices[i + 1]);
            int ic = static_cast<int>(shape_.indices[i + 2]);

            bool hasA = (ia == edge.a || ib == edge.a || ic == edge.a);
            bool hasB = (ia == edge.b || ib == edge.b || ic == edge.b);

            if (hasA && hasB)
            {
                int opp = -1;
                if (ia != edge.a && ia != edge.b) opp = ia;
                else if (ib != edge.a && ib != edge.b) opp = ib;
                else opp = ic;

                if (tri1 < 0) { tri1 = static_cast<int>(i); opp1 = opp; }
                else { tri2 = static_cast<int>(i); opp2 = opp; break; }
            }
        }

        if (tri1 < 0 || tri2 < 0 || opp1 < 0 || opp2 < 0) return;

        shape_.indices[tri1]     = static_cast<unsigned int>(opp1);
        shape_.indices[tri1 + 1] = static_cast<unsigned int>(opp2);
        shape_.indices[tri1 + 2] = static_cast<unsigned int>(edge.a);

        shape_.indices[tri2]     = static_cast<unsigned int>(opp1);
        shape_.indices[tri2 + 1] = static_cast<unsigned int>(opp2);
        shape_.indices[tri2 + 2] = static_cast<unsigned int>(edge.b);

        selectedEdge_ = Edge::Make(opp1, opp2);
        MarkCustom();
    }

    void SpriteEditor::DissolveVertex(int index)
    {
        if (index < 0 || index >= static_cast<int>(shape_.vertices.size())) return;
        DeleteVertex(index);
    }

    void SpriteEditor::DissolveSelectedVerts()
    {
        // Delete in reverse index order so removals don't invalidate earlier indices
        std::vector<int> sorted(selectedVerts_.begin(), selectedVerts_.end());
        std::sort(sorted.rbegin(), sorted.rend());
        for (int idx : sorted)
            DissolveVertex(idx);
        ClearSelection();
    }

    void SpriteEditor::DissolveEdge(Edge edge)
    {
        if (!edge.Valid()) return;

        std::vector<unsigned int> newIndices;
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            int ia = static_cast<int>(shape_.indices[i]);
            int ib = static_cast<int>(shape_.indices[i + 1]);
            int ic = static_cast<int>(shape_.indices[i + 2]);

            bool hasA = (ia == edge.a || ib == edge.a || ic == edge.a);
            bool hasB = (ia == edge.b || ib == edge.b || ic == edge.b);

            if (!(hasA && hasB))
            {
                newIndices.push_back(shape_.indices[i]);
                newIndices.push_back(shape_.indices[i + 1]);
                newIndices.push_back(shape_.indices[i + 2]);
            }
        }
        shape_.indices = newIndices;
        selectedEdge_ = Edge::None();
        MarkCustom();
    }

    // ── Auto-merge / edge-clip on extrude confirm ────────────────────────────

    int SpriteEditor::FindNearestVertex(glm::vec2 pos, float maxDist, const std::set<int>& exclude) const
    {
        int best = -1;
        float bestDist = maxDist;
        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
        {
            if (exclude.count(i)) continue;
            float d = glm::length(shape_.vertices[i].position - pos);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        return best;
    }

    int SpriteEditor::SplitEdgeAtPoint(Edge edge, glm::vec2 point)
    {
        if (!edge.Valid()) return -1;
        int n = static_cast<int>(shape_.vertices.size());
        if (edge.a >= n || edge.b >= n) return -1;

        // Create a new vertex at `point`, interpolating UV
        auto& va = shape_.vertices[edge.a];
        auto& vb = shape_.vertices[edge.b];
        glm::vec2 ab = vb.position - va.position;
        float lenSq = glm::dot(ab, ab);
        float t = (lenSq > 1e-8f) ? glm::clamp(glm::dot(point - va.position, ab) / lenSq, 0.f, 1.f) : 0.5f;

        SpriteVertex mid;
        mid.position = point;
        mid.uv = va.uv + (vb.uv - va.uv) * t;
        shape_.vertices.push_back(mid);
        int midIdx = static_cast<int>(shape_.vertices.size()) - 1;

        // Replace all triangles containing this edge with two sub-triangles
        std::vector<unsigned int> newIndices;
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            int ia = static_cast<int>(shape_.indices[i]);
            int ib = static_cast<int>(shape_.indices[i + 1]);
            int ic = static_cast<int>(shape_.indices[i + 2]);

            int edgePos = -1;
            if (Edge::Make(ia, ib) == edge) edgePos = 0;
            else if (Edge::Make(ib, ic) == edge) edgePos = 1;
            else if (Edge::Make(ic, ia) == edge) edgePos = 2;

            if (edgePos < 0)
            {
                newIndices.push_back(shape_.indices[i]);
                newIndices.push_back(shape_.indices[i + 1]);
                newIndices.push_back(shape_.indices[i + 2]);
            }
            else
            {
                int v0, v1, v2;
                if (edgePos == 0) { v0 = ia; v1 = ib; v2 = ic; }
                else if (edgePos == 1) { v0 = ib; v1 = ic; v2 = ia; }
                else { v0 = ic; v1 = ia; v2 = ib; }

                newIndices.push_back(static_cast<unsigned int>(v0));
                newIndices.push_back(static_cast<unsigned int>(midIdx));
                newIndices.push_back(static_cast<unsigned int>(v2));

                newIndices.push_back(static_cast<unsigned int>(midIdx));
                newIndices.push_back(static_cast<unsigned int>(v1));
                newIndices.push_back(static_cast<unsigned int>(v2));
            }
        }
        shape_.indices = newIndices;
        return midIdx;
    }

    void SpriteEditor::AutoMergeExtruded()
    {
        if (!autoMerge_ || extrudeNewVerts_.empty()) return;

        // Build the set of new vert indices for exclusion during nearest-search
        std::set<int> newSet(extrudeNewVerts_.begin(), extrudeNewVerts_.end());

        // Process each new vertex: try to snap to an existing vertex or edge
        // We process in forward order; each merge can shift indices so we track carefully
        for (size_t vi = 0; vi < extrudeNewVerts_.size(); ++vi)
        {
            int newIdx = extrudeNewVerts_[vi];
            if (newIdx < 0 || newIdx >= static_cast<int>(shape_.vertices.size())) continue;

            glm::vec2 pos = shape_.vertices[newIdx].position;

            // 1. Check for nearby existing vertex
            int nearVert = FindNearestVertex(pos, mergeDist_, newSet);
            if (nearVert >= 0)
            {
                // Merge newIdx INTO nearVert (keep nearVert position)
                // Remap all references of newIdx -> nearVert
                for (auto& idx : shape_.indices)
                {
                    if (idx == static_cast<unsigned int>(newIdx))
                        idx = static_cast<unsigned int>(nearVert);
                }

                // Remove degenerate triangles
                std::vector<unsigned int> cleaned;
                for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
                {
                    unsigned int i0 = shape_.indices[i], i1 = shape_.indices[i+1], i2 = shape_.indices[i+2];
                    if (i0 != i1 && i1 != i2 && i0 != i2)
                    {
                        cleaned.push_back(i0);
                        cleaned.push_back(i1);
                        cleaned.push_back(i2);
                    }
                }
                shape_.indices = cleaned;

                // Remove the vertex
                shape_.vertices.erase(shape_.vertices.begin() + newIdx);
                for (auto& idx : shape_.indices)
                {
                    if (idx > static_cast<unsigned int>(newIdx)) --idx;
                }

                // Remap subsequent new vert indices
                for (size_t j = vi + 1; j < extrudeNewVerts_.size(); ++j)
                {
                    if (extrudeNewVerts_[j] == newIdx) extrudeNewVerts_[j] = -1;
                    else if (extrudeNewVerts_[j] > newIdx) --extrudeNewVerts_[j];
                }
                // Also remap nearVert if needed
                if (nearVert > newIdx) --nearVert;

                // Update newSet
                newSet.clear();
                for (size_t j = vi + 1; j < extrudeNewVerts_.size(); ++j)
                    if (extrudeNewVerts_[j] >= 0) newSet.insert(extrudeNewVerts_[j]);

                continue;
            }

            // 2. Check for nearby existing edge — split it and merge
            auto edges = CollectEdges();
            float bestDist = mergeDist_;
            Edge bestEdge = Edge::None();
            float bestT = 0.f;

            for (const auto& edge : edges)
            {
                // Skip edges that involve any new vertex
                if (newSet.count(edge.a) || newSet.count(edge.b)) continue;

                float t;
                float d = PointToEdgeDistWorld(pos,
                    shape_.vertices[edge.a].position,
                    shape_.vertices[edge.b].position, t);
                // Only split interior of edge (not endpoints — those are caught by vertex merge)
                if (d < bestDist && t > 0.05f && t < 0.95f)
                {
                    bestDist = d;
                    bestEdge = edge;
                    bestT = t;
                }
            }

            if (bestEdge.Valid())
            {
                // Split the edge, creating a new midpoint vertex
                int splitVert = SplitEdgeAtPoint(bestEdge, pos);
                if (splitVert >= 0)
                {
                    // Now merge newIdx with splitVert (snap newIdx to the split point)
                    // The split vert is at the position we want, so just remap
                    for (auto& idx : shape_.indices)
                    {
                        if (idx == static_cast<unsigned int>(newIdx))
                            idx = static_cast<unsigned int>(splitVert);
                    }

                    std::vector<unsigned int> cleaned;
                    for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
                    {
                        unsigned int i0 = shape_.indices[i], i1 = shape_.indices[i+1], i2 = shape_.indices[i+2];
                        if (i0 != i1 && i1 != i2 && i0 != i2)
                        {
                            cleaned.push_back(i0);
                            cleaned.push_back(i1);
                            cleaned.push_back(i2);
                        }
                    }
                    shape_.indices = cleaned;

                    shape_.vertices.erase(shape_.vertices.begin() + newIdx);
                    for (auto& idx : shape_.indices)
                    {
                        if (idx > static_cast<unsigned int>(newIdx)) --idx;
                    }

                    for (size_t j = vi + 1; j < extrudeNewVerts_.size(); ++j)
                    {
                        if (extrudeNewVerts_[j] == newIdx) extrudeNewVerts_[j] = -1;
                        else if (extrudeNewVerts_[j] > newIdx) --extrudeNewVerts_[j];
                    }

                    newSet.clear();
                    for (size_t j = vi + 1; j < extrudeNewVerts_.size(); ++j)
                        if (extrudeNewVerts_[j] >= 0) newSet.insert(extrudeNewVerts_[j]);
                }
            }
        }

        extrudeNewVerts_.clear();
        MarkCustom();
    }

    // ── Main draw ────────────────────────────────────────────────────────────

    void SpriteEditor::Draw(const std::shared_ptr<Engine>& engine)
    {
        if (!isOpen) return;
        frameEngine_ = engine;

        ImGui::SetNextWindowSize(ImVec2(1200, 650), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Sprite Editor", &isOpen))
        {
            ImGui::End();
            frameEngine_ = nullptr;
            return;
        }

        DrawToolbar();
        ImGui::Separator();

        // ── DockSpace inside Sprite Editor ──────────────────────────────
        dockspaceId_ = ImGui::GetID("##SpriteEditorDock");

        // Build the default dock layout BEFORE the DockSpace() call.
        // DockBuilderGetNode returns NULL when the node doesn't exist yet
        // (first frame, or after the editor was closed and imgui.ini cleared).
        if (ImGui::DockBuilderGetNode(dockspaceId_) == nullptr)
        {
            ImGui::DockBuilderAddNode(dockspaceId_,
                                      ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId_,
                                          ImGui::GetContentRegionAvail());

            // Split: left 40% = vertex canvas, right remainder
            ImGuiID dockLeft, dockRight;
            ImGui::DockBuilderSplitNode(dockspaceId_, ImGuiDir_Left, 0.40f,
                                        &dockLeft, &dockRight);

            // Split right into center (UV) and right inspector
            ImGuiID dockCenter, dockInspector;
            ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Right, 0.35f,
                                        &dockInspector, &dockCenter);

            // Split inspector top/bottom for organization
            ImGuiID dockInspTop, dockInspBottom;
            ImGui::DockBuilderSplitNode(dockInspector, ImGuiDir_Up, 0.55f,
                                        &dockInspTop, &dockInspBottom);

            ImGui::DockBuilderDockWindow("Vertex Canvas",    dockLeft);
            ImGui::DockBuilderDockWindow("UV Canvas",        dockCenter);
            ImGui::DockBuilderDockWindow("Material Preview", dockInspTop);
            ImGui::DockBuilderDockWindow("Shape Presets",    dockInspTop);
            ImGui::DockBuilderDockWindow("Snap Config",      dockInspTop);
            ImGui::DockBuilderDockWindow("Geometry Ops",     dockInspBottom);
            ImGui::DockBuilderDockWindow("Vertex Inspector", dockInspBottom);
            ImGui::DockBuilderDockWindow("Shape Info",       dockInspBottom);

            ImGui::DockBuilderFinish(dockspaceId_);
        }

        ImGui::DockSpace(dockspaceId_, ImVec2(0, 0),
                         ImGuiDockNodeFlags_PassthruCentralNode);

        // ── Dockable panels ─────────────────────────────────────────────
        if (ImGui::Begin("Vertex Canvas"))
        {
            ImGui::TextDisabled("Dbl-click: add  [E]xtrude  [X]/Del  [S]ubdiv  [F]lip  [D]issolve  [B]ox");
            ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            DrawVertexCanvas(canvasPos, canvasSize);
        }
        ImGui::End();

        if (ImGui::Begin("UV Canvas"))
        {
            ImGui::TextDisabled("UVs");
            ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            DrawUVCanvas(canvasPos, canvasSize);
        }
        ImGui::End();

        if (ImGui::Begin("Material Preview"))
            DrawMaterialPreview();
        ImGui::End();

        if (ImGui::Begin("Shape Presets"))
            DrawShapePresets();
        ImGui::End();

        if (ImGui::Begin("Snap Config"))
            DrawSnapConfig();
        ImGui::End();

        if (ImGui::Begin("Geometry Ops"))
            DrawGeometryOps();
        ImGui::End();

        if (ImGui::Begin("Vertex Inspector"))
            DrawVertexInspector();
        ImGui::End();

        if (ImGui::Begin("Shape Info"))
            DrawShapeInfo();
        ImGui::End();

        ImGui::End(); // Sprite Editor
        frameEngine_ = nullptr;
    }

    // ── Toolbar ──────────────────────────────────────────────────────────────

    void SpriteEditor::DrawToolbar()
    {
        bool isVertMode = (editMode_ == EditMode::Vertex);
        bool isEdgeMode = (editMode_ == EditMode::Edge);

        if (ImGui::RadioButton("Vertex [1]", isVertMode))
        {
            editMode_ = EditMode::Vertex;
            selectedEdge_ = Edge::None();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Edge [2]", isEdgeMode))
        {
            editMode_ = EditMode::Edge;
            selectedVert_ = -1;
            selectedVerts_.clear();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (ImGui::Button("+ Vertex"))
            AddVertexAtWorld({ 0.f, 0.f });
        ImGui::SameLine();
        if (ImGui::Button("- Vertex"))
        {
            if (selectedVerts_.size() > 1) DeleteSelectedVerts();
            else DeleteSelectedVertex();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (ImGui::Button("+ Triangle (auto)"))
        {
            int n = static_cast<int>(shape_.vertices.size());
            if (n >= 3)
            {
                shape_.indices.push_back(static_cast<unsigned int>(n - 3));
                shape_.indices.push_back(static_cast<unsigned int>(n - 2));
                shape_.indices.push_back(static_cast<unsigned int>(n - 1));
                MarkCustom();
            }
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::Button("Drag to Viewport");
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            const SpriteShape* shapePtr = &shape_;
            ImGui::SetDragDropPayload("SPRITE_SHAPE", &shapePtr, sizeof(SpriteShape*));
            ImGui::Text("Drop onto viewport to spawn sprite");
            ImGui::EndDragDropSource();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(80.f);
        ImGui::DragFloat("Zoom", &canvasZoom_, 1.f, 20.f, 500.f, "%.0f");

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::Checkbox("Auto-merge", &autoMerge_);
        if (autoMerge_)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.f);
            ImGui::DragFloat("##MergeDist", &mergeDist_, 0.005f, 0.01f, 0.5f, "%.3f");
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Local / Global toggle (shared concept with editor viewport gizmo)
        const char* spaceLabel = useLocalSpace_ ? "Local" : "Global";
        if (ImGui::Button(spaceLabel))
            useLocalSpace_ = !useLocalSpace_;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Gizmo orientation: Local (normals/tangents) or Global (world X/Y)");
    }

    // ── Material preview ────────────────────────────────────────────────────

    void SpriteEditor::DrawMaterialPreview()
    {
        ImGui::SeparatorText("Material Preview");

        // Material slot: show current material or "drop here"
        std::string displayName = previewTexturePath_.empty()
            ? "None (drop material)" : previewTexturePath_;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, previewTexturePath_.empty()
            ? ImVec4(0.15f, 0.15f, 0.15f, 1.f)
            : ImVec4(0.2f, 0.15f, 0.05f, 1.f));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##matpreview", displayName.data(), displayName.size(),
                         ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();

        // Accept material drag-drop
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_ASSET"))
            {
                std::string matPath(static_cast<const char*>(payload->Data),
                                    payload->DataSize - 1);
                previewTexturePath_ = matPath;
                previewTextureHandle_ = 0; // force reload below
            }
            ImGui::EndDragDropTarget();
        }

        // Also allow browse for a texture directly
        if (widgets::PathField("Texture", previewTexturePath_, false,
                               {"Images", "*.png *.jpg *.jpeg *.bmp *.tga"}))
        {
            previewTextureHandle_ = 0; // force reload
        }

        if (!previewTexturePath_.empty() && previewTextureHandle_ == 0)
        {
            auto globals = GetDependency(Globals);
            auto cache = GetDependency(ResourceCache);
            if (globals && cache)
            {
                std::string absPath = previewTexturePath_;

                // Check if it's a .material file — resolve texture from it
                auto ext = std::filesystem::path(absPath).extension().string();
                if (ext == ".material")
                {
                    std::string absMatPath = globals->GetWorkingFolder() + absPath;
                    std::ifstream ifs(absMatPath);
                    if (ifs.is_open())
                    {
                        try {
                            nlohmann::json j;
                            ifs >> j;
                            absPath = j.value("texture", "");
                        } catch (...) {}
                    }
                }

                // Resolve relative path
                if (!absPath.empty() && absPath[0] != '/' && absPath[0] != '\\' &&
                    absPath.find(':') == std::string::npos)
                {
                    absPath = globals->GetWorkingFolder() + absPath;
                }

                if (!absPath.empty())
                    previewTextureHandle_ = cache->GetTexture(absPath);
            }
        }

        // Draw thumbnail
        if (previewTextureHandle_ != 0)
        {
            float previewSize = ImGui::GetContentRegionAvail().x;
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(previewTextureHandle_)),
                         ImVec2(previewSize, previewSize));
        }
        else
        {
            ImGui::TextDisabled("No texture loaded");
        }

        // Clear button
        if (!previewTexturePath_.empty())
        {
            if (ImGui::SmallButton("Clear##matpreview"))
            {
                previewTexturePath_.clear();
                previewTextureHandle_ = 0;
            }
        }
    }

    // ── Shape presets ────────────────────────────────────────────────────────

    void SpriteEditor::DrawShapePresets()
    {
        ImGui::SeparatorText("Shape Presets");

        if (ImGui::Button("Quad", ImVec2(-1, 0)))
        {
            shape_ = SpriteShape::MakeQuad();
            ClearSelection();
        }
        if (ImGui::Button("Triangle", ImVec2(-1, 0)))
        {
            shape_ = SpriteShape::MakeTriangle();
            ClearSelection();
        }
        if (ImGui::Button("Circle", ImVec2(-1, 0)))
        {
            shape_ = SpriteShape::MakeCircle(shape_.circleSegments);
            ClearSelection();
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragInt("Segments", &shape_.circleSegments, 1, 3, 128))
        {
            if (shape_.preset == SpriteShape::Preset::Circle)
            {
                shape_ = SpriteShape::MakeCircle(shape_.circleSegments);
                ClearSelection();
            }
        }
    }

    // ── Snap configuration ───────────────────────────────────────────────────

    void SpriteEditor::DrawSnapConfig()
    {
        ImGui::SeparatorText("Snap");

        ImGui::Checkbox("Snap Enabled", &snapEnabled_);

        if (snapEnabled_)
        {
            ImGui::Checkbox("Snap to Grid", &snapToGrid_);
            if (snapToGrid_)
            {
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Grid Size", &snapSize_, 0.01f, 0.01f, 2.0f, "%.3f");
            }

            ImGui::Checkbox("Snap to Vertices", &snapToVertices_);
            if (snapToVertices_)
            {
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("Snap Dist", &snapVertexDist_, 0.01f, 0.01f, 1.0f, "%.3f");
            }
        }
    }

    // ── Geometry operations panel ────────────────────────────────────────────

    void SpriteEditor::DrawGeometryOps()
    {
        ImGui::SeparatorText("Geometry");

        if (editMode_ == EditMode::Vertex)
        {
            bool hasVert = selectedVert_ >= 0 && selectedVert_ < static_cast<int>(shape_.vertices.size());
            bool hasMulti = selectedVerts_.size() > 1;

            if (ImGui::Button("Extrude [E]", ImVec2(-1, 0)) && hasVert)
            {
                glm::vec2 n = ComputeVertexNormal(selectedVert_) * 0.25f;
                ExtrudeVertex(selectedVert_, n);
            }
            if (!hasVert && ImGui::IsItemHovered())
                ImGui::SetTooltip("Select a vertex first");

            if (hasMulti)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "Dissolve Selected (%d) [D]", static_cast<int>(selectedVerts_.size()));
                if (ImGui::Button(buf, ImVec2(-1, 0)))
                    DissolveSelectedVerts();
            }
            else
            {
                if (ImGui::Button("Dissolve [D]", ImVec2(-1, 0)) && hasVert)
                    DissolveVertex(selectedVert_);
                if (!hasVert && ImGui::IsItemHovered())
                    ImGui::SetTooltip("Select a vertex first");
            }

            bool canMerge = hasVert && shape_.vertices.size() >= 2;
            if (ImGui::Button("Merge with Nearest [M]", ImVec2(-1, 0)) && canMerge)
            {
                float bestDist = 1e10f;
                int bestIdx = -1;
                glm::vec2 pos = shape_.vertices[selectedVert_].position;
                for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
                {
                    if (i == selectedVert_) continue;
                    float d = glm::length(shape_.vertices[i].position - pos);
                    if (d < bestDist) { bestDist = d; bestIdx = i; }
                }
                if (bestIdx >= 0) MergeVertices(selectedVert_, bestIdx);
            }
            if (!canMerge && ImGui::IsItemHovered())
                ImGui::SetTooltip("Select a vertex (need 2+ vertices)");
        }
        else
        {
            bool hasEdge = selectedEdge_.Valid();

            if (ImGui::Button("Extrude [E]", ImVec2(-1, 0)) && hasEdge)
            {
                glm::vec2 n = ComputeEdgeNormal(selectedEdge_) * 0.25f;
                ExtrudeEdge(selectedEdge_, n);
            }
            if (!hasEdge && ImGui::IsItemHovered())
                ImGui::SetTooltip("Select an edge first");

            if (ImGui::Button("Subdivide [S]", ImVec2(-1, 0)) && hasEdge)
                SubdivideEdge(selectedEdge_);
            if (!hasEdge && ImGui::IsItemHovered())
                ImGui::SetTooltip("Select an edge first");

            if (ImGui::Button("Flip [F]", ImVec2(-1, 0)) && hasEdge)
                FlipEdge(selectedEdge_);
            if (!hasEdge && ImGui::IsItemHovered())
                ImGui::SetTooltip("Select an edge first (needs 2 adjacent triangles)");

            if (ImGui::Button("Dissolve [D]", ImVec2(-1, 0)) && hasEdge)
                DissolveEdge(selectedEdge_);
            if (!hasEdge && ImGui::IsItemHovered())
                ImGui::SetTooltip("Select an edge first");
        }
    }

    // ── Vertex inspector ─────────────────────────────────────────────────────

    void SpriteEditor::DrawVertexInspector()
    {
        ImGui::SeparatorText("Properties");

        if (editMode_ == EditMode::Edge)
        {
            if (!selectedEdge_.Valid())
            {
                ImGui::TextDisabled("No edge selected");
            }
            else
            {
                ImGui::Text("Edge: %d - %d", selectedEdge_.a, selectedEdge_.b);
                auto& va = shape_.vertices[selectedEdge_.a];
                auto& vb = shape_.vertices[selectedEdge_.b];
                float len = glm::length(vb.position - va.position);
                ImGui::Text("Length: %.3f", len);

                glm::vec2 n = ComputeEdgeNormal(selectedEdge_);
                glm::vec2 t = ComputeEdgeTangent(selectedEdge_);
                ImGui::Text("Normal:  (%.2f, %.2f)", n.x, n.y);
                ImGui::Text("Tangent: (%.2f, %.2f)", t.x, t.y);

                ImGui::Text("A: (%.2f, %.2f)", va.position.x, va.position.y);
                ImGui::Text("B: (%.2f, %.2f)", vb.position.x, vb.position.y);
            }
        }
        else
        {
            if (selectedVerts_.size() > 1)
            {
                ImGui::Text("%d vertices selected", static_cast<int>(selectedVerts_.size()));
            }
            else if (selectedVert_ < 0 || selectedVert_ >= static_cast<int>(shape_.vertices.size()))
            {
                ImGui::TextDisabled("No vertex selected");
            }
            else
            {
                auto& vert = shape_.vertices[selectedVert_];
                char label[64];
                snprintf(label, sizeof(label), "Vertex %d", selectedVert_);
                ImGui::Text("%s", label);

                ImGui::SetNextItemWidth(-1);
                bool changed = false;
                changed |= ImGui::DragFloat2("Position", &vert.position.x, 0.01f);
                ImGui::SetNextItemWidth(-1);
                changed |= ImGui::DragFloat2("UV", &vert.uv.x, 0.01f);

                if (changed) MarkCustom();

                glm::vec2 n = ComputeVertexNormal(selectedVert_);
                ImGui::Text("Normal: (%.2f, %.2f)", n.x, n.y);
            }
        }

        ImGui::Spacing();

        ImGui::SeparatorText("All Vertices");
        ImGui::BeginChild("##VertList", ImVec2(0, 120), true);
        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
        {
            char buf[80];
            snprintf(buf, sizeof(buf), "[%d] (%.2f, %.2f) uv(%.2f, %.2f)",
                     i,
                     shape_.vertices[i].position.x,
                     shape_.vertices[i].position.y,
                     shape_.vertices[i].uv.x,
                     shape_.vertices[i].uv.y);
            bool selected = IsVertSelected(i);
            if (ImGui::Selectable(buf, selected))
            {
                bool shift = ImGui::GetIO().KeyShift;
                SelectVert(i, shift);
                editMode_ = EditMode::Vertex;
            }
        }
        ImGui::EndChild();
    }

    // ── Shape info ───────────────────────────────────────────────────────────

    void SpriteEditor::DrawShapeInfo()
    {
        ImGui::SeparatorText("Info");
        ImGui::Text("Shape: %s", shape_.name.c_str());
        ImGui::Text("Vertices: %d", static_cast<int>(shape_.vertices.size()));
        ImGui::Text("Triangles: %d", static_cast<int>(shape_.indices.size() / 3));
        ImGui::Text("Indices: %d", static_cast<int>(shape_.indices.size()));
        ImGui::Text("Edges: %d", static_cast<int>(CollectEdges().size()));
        ImGui::Text("Mode: %s", editMode_ == EditMode::Vertex ? "Vertex" : "Edge");
        ImGui::Text("Selected: %d", static_cast<int>(selectedVerts_.size()));

        if (extruding_) ImGui::TextColored(ImVec4(1, .8f, .2f, 1), "EXTRUDING... (click confirm, Esc cancel)");
        if (boxSelecting_) ImGui::TextColored(ImVec4(.5f, .8f, 1, 1), "BOX SELECT...");
    }

    // ── Mini-gizmo (local-space axes at selection) ───────────────────────────

    void SpriteEditor::DrawMiniGizmo(ImDrawList* dl, ImVec2 origin, ImVec2 canvasPos, ImVec2 canvasSize)
    {
        constexpr float HANDLE_LEN = 45.f;
        constexpr float HIT_R      = 10.f;

        glm::vec2 localX(1.f, 0.f), localY(0.f, 1.f);

        if (useLocalSpace_)
        {
            if (editMode_ == EditMode::Edge && selectedEdge_.Valid())
            {
                localX = ComputeEdgeTangent(selectedEdge_);
                localY = ComputeEdgeNormal(selectedEdge_);
            }
            else if (editMode_ == EditMode::Vertex && selectedVert_ >= 0)
            {
                glm::vec2 n = ComputeVertexNormal(selectedVert_);
                localY = n;
                localX = glm::vec2(n.y, -n.x);
            }
        }
        // else: global mode — localX/localY stay as world X/Y

        ImVec2 xTip = { origin.x + localX.x * HANDLE_LEN, origin.y - localX.y * HANDLE_LEN };
        ImVec2 yTip = { origin.x + localY.x * HANDLE_LEN, origin.y - localY.y * HANDLE_LEN };

        ImVec2 mp = ImGui::GetMousePos();

        auto distSeg = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
            float dx = b.x - a.x, dy = b.y - a.y;
            float len2 = dx*dx + dy*dy;
            if (len2 < 1e-6f) return sqrtf((p.x-a.x)*(p.x-a.x)+(p.y-a.y)*(p.y-a.y));
            float t = glm::clamp(((p.x-a.x)*dx + (p.y-a.y)*dy) / len2, 0.f, 1.f);
            float cx2 = a.x + t*dx, cy2 = a.y + t*dy;
            return sqrtf((p.x-cx2)*(p.x-cx2)+(p.y-cy2)*(p.y-cy2));
        };

        float distO = sqrtf((mp.x-origin.x)*(mp.x-origin.x) + (mp.y-origin.y)*(mp.y-origin.y));

        if (gizmoDragging_ == GizmoAxis::None)
        {
            gizmoHovered_ = GizmoAxis::None;
            if (distO < HIT_R)
                gizmoHovered_ = GizmoAxis::XY;
            else if (distSeg(mp, origin, xTip) < HIT_R)
                gizmoHovered_ = GizmoAxis::X;
            else if (distSeg(mp, origin, yTip) < HIT_R)
                gizmoHovered_ = GizmoAxis::Y;
        }

        constexpr ImU32 HIGHLIGHT = IM_COL32(255, 230, 60, 255);

        auto isActive = [&](GizmoAxis a) {
            return gizmoHovered_ == a || gizmoDragging_ == a;
        };

        ImU32 cX = isActive(GizmoAxis::X) || isActive(GizmoAxis::XY) ? HIGHLIGHT : IM_COL32(50, 200, 220, 255);
        dl->AddLine(origin, xTip, cX, 2.f);
        dl->AddTriangleFilled(
            { xTip.x + (localX.x * 7.f), xTip.y - (localX.y * 7.f) },
            { xTip.x + (-localX.y * 4.f), xTip.y - (localX.x * 4.f) },
            { xTip.x - (-localX.y * 4.f), xTip.y + (localX.x * 4.f) },
            cX);

        ImU32 cY = isActive(GizmoAxis::Y) || isActive(GizmoAxis::XY) ? HIGHLIGHT : IM_COL32(220, 50, 200, 255);
        dl->AddLine(origin, yTip, cY, 2.f);
        dl->AddTriangleFilled(
            { yTip.x + (localY.x * 7.f), yTip.y - (localY.y * 7.f) },
            { yTip.x + (-localY.y * 4.f), yTip.y - (localY.x * 4.f) },
            { yTip.x - (-localY.y * 4.f), yTip.y + (localY.x * 4.f) },
            cY);

        ImU32 cC = isActive(GizmoAxis::XY) ? HIGHLIGHT : IM_COL32(220, 220, 220, 255);
        dl->AddCircleFilled(origin, 5.f, cC);
        dl->AddCircle(origin, 5.f, IM_COL32(60, 60, 60, 200));

        dl->AddText({ xTip.x + 4.f, xTip.y - 12.f }, cX, useLocalSpace_ ? "T" : "X");
        dl->AddText({ yTip.x + 4.f, yTip.y - 12.f }, cY, useLocalSpace_ ? "N" : "Y");

        // ── Gizmo drag interaction ───────────────────────────────────────
        if (gizmoHovered_ != GizmoAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            gizmoDragging_ = gizmoHovered_;
            gizmoDragStart_ = mp;
        }

        if (gizmoDragging_ != GizmoAxis::None && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float dxPx = mp.x - gizmoDragStart_.x;
            float dyPx = mp.y - gizmoDragStart_.y;
            gizmoDragStart_ = mp;

            float worldPerPx = 1.f / canvasZoom_;
            glm::vec2 pixelDir(dxPx, -dyPx);

            glm::vec2 move(0.f);
            if (gizmoDragging_ == GizmoAxis::X)
            {
                float proj = glm::dot(pixelDir, localX);
                move = localX * proj * worldPerPx;
            }
            else if (gizmoDragging_ == GizmoAxis::Y)
            {
                float proj = glm::dot(pixelDir, localY);
                move = localY * proj * worldPerPx;
            }
            else
            {
                move = pixelDir * worldPerPx;
            }

            // Apply to all selected verts
            if (editMode_ == EditMode::Vertex)
            {
                for (int idx : selectedVerts_)
                {
                    if (idx >= 0 && idx < static_cast<int>(shape_.vertices.size()))
                        shape_.vertices[idx].position += move;
                }
                MarkCustom();
            }
            else if (editMode_ == EditMode::Edge && selectedEdge_.Valid())
            {
                shape_.vertices[selectedEdge_.a].position += move;
                shape_.vertices[selectedEdge_.b].position += move;
                MarkCustom();
            }
        }

        if (gizmoDragging_ != GizmoAxis::None && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            gizmoDragging_ = GizmoAxis::None;
    }

    // ── Vertex canvas ────────────────────────────────────────────────────────

    void SpriteEditor::DrawVertexCanvas(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(canvasPos,
                          ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                          IM_COL32(30, 30, 30, 255));

        dl->PushClipRect(canvasPos,
                         ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

        // ── Grid ─────────────────────────────────────────────────────────
        {
            float gridStep = snapEnabled_ && snapToGrid_ ? snapSize_ : 0.25f;
            float pixelStep = gridStep * canvasZoom_;
            if (pixelStep > 4.f)
            {
                ImVec2 origin = WorldToCanvas({ 0.f, 0.f }, canvasPos, canvasSize);
                ImU32 gridCol = IM_COL32(50, 50, 50, 255);

                float startX = std::fmod(origin.x - canvasPos.x, pixelStep);
                if (startX < 0) startX += pixelStep;
                for (float x = startX; x < canvasSize.x; x += pixelStep)
                    dl->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                                ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), gridCol);

                float startY = std::fmod(origin.y - canvasPos.y, pixelStep);
                if (startY < 0) startY += pixelStep;
                for (float y = startY; y < canvasSize.y; y += pixelStep)
                    dl->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), gridCol);
            }

            ImVec2 origin = WorldToCanvas({ 0.f, 0.f }, canvasPos, canvasSize);
            dl->AddLine(ImVec2(canvasPos.x, origin.y),
                        ImVec2(canvasPos.x + canvasSize.x, origin.y),
                        IM_COL32(80, 80, 80, 200));
            dl->AddLine(ImVec2(origin.x, canvasPos.y),
                        ImVec2(origin.x, canvasPos.y + canvasSize.y),
                        IM_COL32(80, 80, 80, 200));
        }

        // ── Draw filled triangles ────────────────────────────────────────
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            auto p0 = WorldToCanvas(shape_.vertices[shape_.indices[i]].position, canvasPos, canvasSize);
            auto p1 = WorldToCanvas(shape_.vertices[shape_.indices[i + 1]].position, canvasPos, canvasSize);
            auto p2 = WorldToCanvas(shape_.vertices[shape_.indices[i + 2]].position, canvasPos, canvasSize);
            dl->AddTriangleFilled(p0, p1, p2, IM_COL32(60, 90, 140, 100));
        }

        // ── Draw edges ───────────────────────────────────────────────────
        auto edges = CollectEdges();
        ImVec2 mp = ImGui::GetMousePos();
        const float edgeHitDist = 8.f;

        for (const auto& edge : edges)
        {
            auto a = WorldToCanvas(shape_.vertices[edge.a].position, canvasPos, canvasSize);
            auto b = WorldToCanvas(shape_.vertices[edge.b].position, canvasPos, canvasSize);

            ImU32 edgeCol;
            float thickness;

            if (edge == selectedEdge_)
            {
                edgeCol = IM_COL32(255, 200, 50, 255);
                thickness = 3.0f;
            }
            else if (edge == hoveredEdge_ && editMode_ == EditMode::Edge)
            {
                edgeCol = IM_COL32(200, 200, 100, 220);
                thickness = 2.5f;
            }
            else
            {
                edgeCol = IM_COL32(100, 150, 220, 200);
                thickness = 1.5f;
            }

            dl->AddLine(a, b, edgeCol, thickness);
        }

        // ── Draw vertices ────────────────────────────────────────────────
        const float vertRadius = 6.f;
        const float hitRadius  = 10.f;

        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
        {
            ImVec2 sp = WorldToCanvas(shape_.vertices[i].position, canvasPos, canvasSize);

            ImU32 col;
            if (IsVertSelected(i))
                col = IM_COL32(255, 200, 50, 255);
            else if (i == hoveredVert_)
                col = IM_COL32(255, 255, 150, 255);
            else
                col = IM_COL32(220, 220, 220, 255);

            dl->AddCircleFilled(sp, vertRadius, col);
            dl->AddCircle(sp, vertRadius, IM_COL32(0, 0, 0, 200), 0, 1.5f);

            char idx[8];
            snprintf(idx, sizeof(idx), "%d", i);
            dl->AddText(ImVec2(sp.x + 8, sp.y - 12), IM_COL32(180, 180, 180, 200), idx);
        }

        // ── Draw box selection rectangle ─────────────────────────────────
        if (boxSelecting_)
        {
            ImVec2 bMin = { std::min(boxStart_.x, boxEnd_.x), std::min(boxStart_.y, boxEnd_.y) };
            ImVec2 bMax = { std::max(boxStart_.x, boxEnd_.x), std::max(boxStart_.y, boxEnd_.y) };
            ImU32 fillCol = boxDeselectMode_ ? IM_COL32(255, 100, 100, 40)  : IM_COL32(100, 150, 255, 40);
            ImU32 lineCol = boxDeselectMode_ ? IM_COL32(255, 100, 100, 200) : IM_COL32(100, 150, 255, 200);
            dl->AddRectFilled(bMin, bMax, fillCol);
            dl->AddRect(bMin, bMax, lineCol, 0.f, 0, 1.5f);
        }

        // ── Draw mini-gizmo at selection ─────────────────────────────────
        bool hasGizmoTarget = false;
        ImVec2 gizmoOrigin = {};

        if (editMode_ == EditMode::Vertex && !selectedVerts_.empty() && !extruding_ && !boxSelecting_)
        {
            // Gizmo at centroid of selection
            glm::vec2 centroid(0.f);
            int count = 0;
            for (int idx : selectedVerts_)
            {
                if (idx >= 0 && idx < static_cast<int>(shape_.vertices.size()))
                {
                    centroid += shape_.vertices[idx].position;
                    ++count;
                }
            }
            if (count > 0)
            {
                centroid /= static_cast<float>(count);
                gizmoOrigin = WorldToCanvas(centroid, canvasPos, canvasSize);
                hasGizmoTarget = true;
            }
        }
        else if (editMode_ == EditMode::Edge && selectedEdge_.Valid() && !extruding_ && !boxSelecting_)
        {
            glm::vec2 mid = (shape_.vertices[selectedEdge_.a].position +
                             shape_.vertices[selectedEdge_.b].position) * 0.5f;
            gizmoOrigin = WorldToCanvas(mid, canvasPos, canvasSize);
            hasGizmoTarget = true;
        }

        if (hasGizmoTarget)
            DrawMiniGizmo(dl, gizmoOrigin, canvasPos, canvasSize);

        // ── Interaction: invisible button ────────────────────────────────
        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("##vertCanvas", canvasSize,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        bool canvasHovered = ImGui::IsItemHovered();
        bool canvasActive  = ImGui::IsItemActive();

        // ── Hover detection ──────────────────────────────────────────────
        hoveredVert_ = -1;
        hoveredEdge_ = Edge::None();

        if (canvasHovered)
        {
            float closestDist = hitRadius;
            for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
            {
                ImVec2 sp = WorldToCanvas(shape_.vertices[i].position, canvasPos, canvasSize);
                float dx = mp.x - sp.x, dy = mp.y - sp.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < closestDist) { closestDist = dist; hoveredVert_ = i; }
            }

            if (hoveredVert_ < 0 && editMode_ == EditMode::Edge)
            {
                float closestEdgeDist = edgeHitDist;
                for (const auto& edge : edges)
                {
                    auto a = WorldToCanvas(shape_.vertices[edge.a].position, canvasPos, canvasSize);
                    auto b = WorldToCanvas(shape_.vertices[edge.b].position, canvasPos, canvasSize);
                    float d = PointToEdgeDist(mp, a, b);
                    if (d < closestEdgeDist)
                    {
                        closestEdgeDist = d;
                        hoveredEdge_ = edge;
                    }
                }
            }
        }

        // ── Modal extrude ────────────────────────────────────────────────
        if (extruding_)
        {
            glm::vec2 worldPos = CanvasToWorld(mp, canvasPos, canvasSize);

            if (editMode_ == EditMode::Vertex && selectedVert_ >= 0)
            {
                if (snapEnabled_ && snapToGrid_)
                {
                    worldPos.x = SnapValue(worldPos.x, snapSize_);
                    worldPos.y = SnapValue(worldPos.y, snapSize_);
                }
                shape_.vertices[selectedVert_].position = worldPos;
                MarkCustom();

                if (extrudeAnchor_ >= 0 && extrudeAnchor_ < static_cast<int>(shape_.vertices.size()))
                {
                    ImVec2 anchorSp = WorldToCanvas(shape_.vertices[extrudeAnchor_].position, canvasPos, canvasSize);
                    ImVec2 curSp = WorldToCanvas(worldPos, canvasPos, canvasSize);
                    dl->AddLine(anchorSp, curSp, IM_COL32(255, 200, 50, 150), 1.5f);
                }
            }
            else if (editMode_ == EditMode::Edge && selectedEdge_.Valid())
            {
                float dxPx = mp.x - extrudeStartMouse_.x;
                float dyPx = mp.y - extrudeStartMouse_.y;
                extrudeStartMouse_ = mp;

                glm::vec2 pixelDir(dxPx, -dyPx);
                glm::vec2 move = pixelDir / canvasZoom_;

                shape_.vertices[selectedEdge_.a].position += move;
                shape_.vertices[selectedEdge_.b].position += move;
                MarkCustom();
            }

            // Confirm
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                extruding_ = false;
                // Auto-merge newly extruded verts with existing geometry
                AutoMergeExtruded();
                extrudeAnchor_ = -1;
                extrudeEdgeAnchor_ = Edge::None();
            }

            // Cancel
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                if (editMode_ == EditMode::Vertex && selectedVert_ >= 0)
                {
                    DeleteVertex(selectedVert_);
                    selectedVert_ = extrudeAnchor_;
                    if (extrudeAnchor_ >= 0) SelectVert(extrudeAnchor_, false);
                }
                else if (editMode_ == EditMode::Edge && selectedEdge_.Valid())
                {
                    int higher = std::max(selectedEdge_.a, selectedEdge_.b);
                    int lower  = std::min(selectedEdge_.a, selectedEdge_.b);
                    DeleteVertex(higher);
                    DeleteVertex(lower);
                    selectedEdge_ = extrudeEdgeAnchor_;
                }
                extruding_ = false;
                extrudeAnchor_ = -1;
                extrudeEdgeAnchor_ = Edge::None();
                extrudeNewVerts_.clear();
            }
        }
        // ── Box selection mode ───────────────────────────────────────────
        else if (boxSelecting_)
        {
            boxEnd_ = mp;

            // Middle-mouse toggles deselect mode (Blender-style)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
                boxDeselectMode_ = !boxDeselectMode_;

            // Update selection: all verts inside the box
            ImVec2 bMin = { std::min(boxStart_.x, boxEnd_.x), std::min(boxStart_.y, boxEnd_.y) };
            ImVec2 bMax = { std::max(boxStart_.x, boxEnd_.x), std::max(boxStart_.y, boxEnd_.y) };

            bool shift = ImGui::GetIO().KeyShift;
            if (!shift && !boxDeselectMode_) selectedVerts_.clear();

            for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
            {
                ImVec2 sp = WorldToCanvas(shape_.vertices[i].position, canvasPos, canvasSize);
                if (sp.x >= bMin.x && sp.x <= bMax.x && sp.y >= bMin.y && sp.y <= bMax.y)
                {
                    if (boxDeselectMode_)
                        selectedVerts_.erase(i);
                    else
                        selectedVerts_.insert(i);
                }
            }

            if (!selectedVerts_.empty())
                selectedVert_ = *selectedVerts_.rbegin();
            else if (!boxDeselectMode_)
                selectedVert_ = -1;

            // Finish on mouse release
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                boxSelecting_ = false;
                boxDeselectMode_ = false;
            }

            // Cancel
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                boxSelecting_ = false;
                boxDeselectMode_ = false;
                if (!shift) ClearSelection();
            }
        }
        // ── Normal interaction ───────────────────────────────────────────
        else
        {
            // ── Double-click to add vertex ───────────────────────────────
            if (canvasHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
                && gizmoDragging_ == GizmoAxis::None)
            {
                if (hoveredVert_ < 0)
                {
                    glm::vec2 worldPos = CanvasToWorld(mp, canvasPos, canvasSize);
                    AddVertexAtWorld(worldPos);
                }
            }
            // ── Single click to select ───────────────────────────────────
            else if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                     && gizmoDragging_ == GizmoAxis::None && gizmoHovered_ == GizmoAxis::None)
            {
                bool shift = ImGui::GetIO().KeyShift;

                if (editMode_ == EditMode::Vertex)
                {
                    if (hoveredVert_ >= 0)
                    {
                        SelectVert(hoveredVert_, shift);
                        draggingVert_ = true;
                    }
                    else
                    {
                        // Blender-style: defer to clickPending; drag = box select, release = deselect
                        clickPending_ = true;
                        clickPendingPos_ = mp;
                    }
                }
                else
                {
                    if (hoveredVert_ >= 0)
                    {
                        SelectVert(hoveredVert_, shift);
                        draggingVert_ = true;
                    }
                    else
                    {
                        selectedEdge_ = hoveredEdge_;
                        if (!shift)
                        {
                            selectedVert_ = -1;
                            selectedVerts_.clear();
                        }
                    }
                }
            }

            // ── Drag selected vertex/vertices ────────────────────────────
            if (draggingVert_ && selectedVert_ >= 0 && canvasActive
                && gizmoDragging_ == GizmoAxis::None
                && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                glm::vec2 worldPos = CanvasToWorld(mp, canvasPos, canvasSize);

                if (snapEnabled_)
                {
                    if (snapToGrid_)
                    {
                        worldPos.x = SnapValue(worldPos.x, snapSize_);
                        worldPos.y = SnapValue(worldPos.y, snapSize_);
                    }
                    if (snapToVertices_)
                    {
                        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
                        {
                            if (IsVertSelected(i)) continue;
                            glm::vec2 diff = worldPos - shape_.vertices[i].position;
                            if (glm::length(diff) < snapVertexDist_)
                            {
                                worldPos = shape_.vertices[i].position;
                                break;
                            }
                        }
                    }
                }

                // If multi-select, move all selected verts by delta
                if (selectedVerts_.size() > 1)
                {
                    glm::vec2 oldPos = shape_.vertices[selectedVert_].position;
                    glm::vec2 delta = worldPos - oldPos;
                    for (int idx : selectedVerts_)
                    {
                        if (idx >= 0 && idx < static_cast<int>(shape_.vertices.size()))
                            shape_.vertices[idx].position += delta;
                    }
                }
                else
                {
                    shape_.vertices[selectedVert_].position = worldPos;
                }
                MarkCustom();
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && draggingVert_)
            {
                draggingVert_ = false;

                // Auto-merge: merge dragged vertices into nearby ones (Blender-style)
                if (autoMerge_)
                {
                    // Collect the verts we just dragged (sorted descending so
                    // deletion doesn't invalidate later indices).
                    std::vector<int> dragged(selectedVerts_.begin(), selectedVerts_.end());
                    if (dragged.empty() && selectedVert_ >= 0)
                        dragged.push_back(selectedVert_);
                    std::sort(dragged.rbegin(), dragged.rend());

                    std::set<int> draggedSet(dragged.begin(), dragged.end());

                    for (int idx : dragged)
                    {
                        if (idx < 0 || idx >= static_cast<int>(shape_.vertices.size())) continue;
                        glm::vec2 pos = shape_.vertices[idx].position;
                        int nearest = FindNearestVertex(pos, mergeDist_, draggedSet);
                        if (nearest < 0) continue;

                        // Snap dragged vert onto target, then merge (erases idx).
                        // MergeVertices(a, b) erases b -- so pass (nearest, idx) to
                        // keep nearest's position and erase idx.
                        shape_.vertices[idx].position = shape_.vertices[nearest].position;
                        shape_.vertices[idx].uv       = shape_.vertices[nearest].uv;
                        MergeVertices(nearest, idx);

                        // Update draggedSet after removal of idx
                        draggedSet.erase(idx);
                        std::set<int> remapped;
                        for (int v : draggedSet)
                            remapped.insert(v > idx ? v - 1 : v);
                        draggedSet = remapped;
                    }
                }
            }

            // ── Resolve click-pending (Blender: click = deselect, drag = box select)
            if (clickPending_)
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    // User dragged — start box selection
                    bool shift = ImGui::GetIO().KeyShift;
                    boxSelecting_ = true;
                    boxStart_ = clickPendingPos_;
                    boxEnd_ = mp;
                    if (!shift)
                    {
                        selectedVerts_.clear();
                        selectedVert_ = -1;
                    }
                    clickPending_ = false;
                    editMode_ = EditMode::Vertex;
                }
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    // User clicked without dragging — just deselect
                    bool shift = ImGui::GetIO().KeyShift;
                    if (!shift)
                        ClearSelection();
                    clickPending_ = false;
                }
            }

            // ── Context menu ─────────────────────────────────────────────
            if (canvasHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)
                && !ImGui::IsMouseDragging(ImGuiMouseButton_Right, 5.f))
            {
                contextVert_ = hoveredVert_;
                contextEdge_ = hoveredEdge_;
                wantsContextMenu_ = true;
            }

            if (wantsContextMenu_)
            {
                ImGui::OpenPopup("##SpriteCtx");
                wantsContextMenu_ = false;
            }

            if (ImGui::BeginPopup("##SpriteCtx"))
            {
                // Multi-selection operations
                if (selectedVerts_.size() > 1)
                {
                    char hdr[48];
                    snprintf(hdr, sizeof(hdr), "%d vertices selected", static_cast<int>(selectedVerts_.size()));
                    ImGui::SeparatorText(hdr);

                    if (ImGui::MenuItem("Dissolve Selected", "D"))
                        DissolveSelectedVerts();
                    if (ImGui::MenuItem("Delete Selected", "X"))
                        DeleteSelectedVerts();
                }

                if (contextVert_ >= 0 && contextVert_ < static_cast<int>(shape_.vertices.size()))
                {
                    char hdr[32];
                    snprintf(hdr, sizeof(hdr), "Vertex %d", contextVert_);
                    ImGui::SeparatorText(hdr);

                    if (ImGui::MenuItem("Delete", "X"))
                        DeleteVertex(contextVert_);
                    if (ImGui::MenuItem("Extrude", "E"))
                    {
                        glm::vec2 n = ComputeVertexNormal(contextVert_) * 0.001f;
                        SelectVert(contextVert_, false);
                        editMode_ = EditMode::Vertex;
                        extrudeNewVerts_.clear();
                        ExtrudeVertex(contextVert_, n);
                        extruding_ = true;
                        extrudeAnchor_ = contextVert_;
                        extrudeStartMouse_ = mp;
                    }
                    if (ImGui::MenuItem("Dissolve", "D"))
                        DissolveVertex(contextVert_);

                    bool canMerge = shape_.vertices.size() >= 2;
                    if (ImGui::MenuItem("Merge with Nearest", "M", false, canMerge))
                    {
                        float bestDist = 1e10f;
                        int bestIdx = -1;
                        glm::vec2 pos = shape_.vertices[contextVert_].position;
                        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
                        {
                            if (i == contextVert_) continue;
                            float d = glm::length(shape_.vertices[i].position - pos);
                            if (d < bestDist) { bestDist = d; bestIdx = i; }
                        }
                        if (bestIdx >= 0) MergeVertices(contextVert_, bestIdx);
                    }
                }

                if (contextEdge_.Valid() && editMode_ == EditMode::Edge)
                {
                    char hdr[32];
                    snprintf(hdr, sizeof(hdr), "Edge %d-%d", contextEdge_.a, contextEdge_.b);
                    ImGui::SeparatorText(hdr);

                    if (ImGui::MenuItem("Subdivide", "S"))
                        SubdivideEdge(contextEdge_);
                    if (ImGui::MenuItem("Extrude", "E"))
                    {
                        glm::vec2 n = ComputeEdgeNormal(contextEdge_) * 0.001f;
                        selectedEdge_ = contextEdge_;
                        extrudeNewVerts_.clear();
                        ExtrudeEdge(contextEdge_, n);
                        extruding_ = true;
                        extrudeEdgeAnchor_ = contextEdge_;
                        extrudeStartMouse_ = mp;
                    }
                    if (ImGui::MenuItem("Flip", "F"))
                        FlipEdge(contextEdge_);
                    if (ImGui::MenuItem("Dissolve", "D"))
                        DissolveEdge(contextEdge_);
                }

                if (contextVert_ < 0 && !contextEdge_.Valid())
                {
                    if (ImGui::MenuItem("Add Vertex Here"))
                    {
                        glm::vec2 worldPos = CanvasToWorld(mp, canvasPos, canvasSize);
                        AddVertexAtWorld(worldPos);
                    }
                }

                ImGui::EndPopup();
            }

            // ── Hotkeys ──────────────────────────────────────────────────
            if (canvasHovered)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_1))
                {
                    editMode_ = EditMode::Vertex;
                    selectedEdge_ = Edge::None();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_2))
                {
                    editMode_ = EditMode::Edge;
                    selectedVert_ = -1;
                    selectedVerts_.clear();
                }

                // B = start box select
                if (ImGui::IsKeyPressed(ImGuiKey_B))
                {
                    boxSelecting_ = true;
                    boxStart_ = mp;
                    boxEnd_ = mp;
                    editMode_ = EditMode::Vertex;
                }

                // A = select all / deselect all
                if (ImGui::IsKeyPressed(ImGuiKey_A))
                {
                    if (selectedVerts_.size() == shape_.vertices.size())
                        ClearSelection();
                    else
                    {
                        selectedVerts_.clear();
                        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
                            selectedVerts_.insert(i);
                        if (!selectedVerts_.empty())
                            selectedVert_ = *selectedVerts_.rbegin();
                        editMode_ = EditMode::Vertex;
                    }
                }

                // E = Extrude
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                {
                    if (editMode_ == EditMode::Vertex && selectedVert_ >= 0)
                    {
                        int anchor = selectedVert_;
                        glm::vec2 n = ComputeVertexNormal(selectedVert_) * 0.001f;
                        extrudeNewVerts_.clear();
                        ExtrudeVertex(selectedVert_, n);
                        extruding_ = true;
                        extrudeAnchor_ = anchor;
                        extrudeStartMouse_ = mp;
                    }
                    else if (editMode_ == EditMode::Edge && selectedEdge_.Valid())
                    {
                        Edge anchor = selectedEdge_;
                        glm::vec2 n = ComputeEdgeNormal(selectedEdge_) * 0.001f;
                        extrudeNewVerts_.clear();
                        ExtrudeEdge(selectedEdge_, n);
                        extruding_ = true;
                        extrudeEdgeAnchor_ = anchor;
                        extrudeStartMouse_ = mp;
                    }
                }

                // X or Delete
                if (ImGui::IsKeyPressed(ImGuiKey_X) || ImGui::IsKeyPressed(ImGuiKey_Delete))
                {
                    if (editMode_ == EditMode::Vertex)
                    {
                        if (selectedVerts_.size() > 1)
                            DeleteSelectedVerts();
                        else
                        {
                            int toDelete = (hoveredVert_ >= 0) ? hoveredVert_ : selectedVert_;
                            if (toDelete >= 0) DeleteVertex(toDelete);
                        }
                    }
                    else if (editMode_ == EditMode::Edge && selectedEdge_.Valid())
                        DissolveEdge(selectedEdge_);
                }

                // S = Subdivide
                if (ImGui::IsKeyPressed(ImGuiKey_S) && editMode_ == EditMode::Edge && selectedEdge_.Valid())
                    SubdivideEdge(selectedEdge_);

                // F = Flip
                if (ImGui::IsKeyPressed(ImGuiKey_F) && editMode_ == EditMode::Edge && selectedEdge_.Valid())
                    FlipEdge(selectedEdge_);

                // D = Dissolve
                if (ImGui::IsKeyPressed(ImGuiKey_D))
                {
                    if (editMode_ == EditMode::Vertex)
                    {
                        if (selectedVerts_.size() > 1)
                            DissolveSelectedVerts();
                        else if (selectedVert_ >= 0)
                            DissolveVertex(selectedVert_);
                    }
                    else if (editMode_ == EditMode::Edge && selectedEdge_.Valid())
                        DissolveEdge(selectedEdge_);
                }

                // M = Merge
                if (ImGui::IsKeyPressed(ImGuiKey_M) && editMode_ == EditMode::Vertex
                    && selectedVert_ >= 0 && shape_.vertices.size() >= 2)
                {
                    float bestDist = 1e10f;
                    int bestIdx = -1;
                    glm::vec2 pos = shape_.vertices[selectedVert_].position;
                    for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
                    {
                        if (i == selectedVert_) continue;
                        float d = glm::length(shape_.vertices[i].position - pos);
                        if (d < bestDist) { bestDist = d; bestIdx = i; }
                    }
                    if (bestIdx >= 0) MergeVertices(selectedVert_, bestIdx);
                }
            }

            // ── Right-click drag to pan ──────────────────────────────────
            if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 5.f))
            {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                canvasOffset_.x += delta.x / canvasZoom_;
                canvasOffset_.y -= delta.y / canvasZoom_;
            }
        }

        // Scroll to zoom
        if (canvasHovered)
        {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.f)
            {
                canvasZoom_ *= (wheel > 0.f) ? 1.1f : 0.9f;
                canvasZoom_ = glm::clamp(canvasZoom_, 20.f, 500.f);
            }
        }

        dl->PopClipRect();
    }

    // ── UV canvas ────────────────────────────────────────────────────────────

    void SpriteEditor::DrawUVCanvas(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(canvasPos,
                          ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                          IM_COL32(25, 25, 35, 255));

        dl->PushClipRect(canvasPos,
                         ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

        ImVec2 uvMin = UVToCanvas({ 0.f, 0.f }, canvasPos, canvasSize);
        ImVec2 uvMax = UVToCanvas({ 1.f, 1.f }, canvasPos, canvasSize);
        dl->AddRect(uvMin, uvMax, IM_COL32(100, 100, 100, 200), 0.f, 0, 1.5f);

        float gridStep = 0.25f;
        for (float u = 0.f; u <= 1.001f; u += gridStep)
        {
            ImVec2 a = UVToCanvas({ u, 0.f }, canvasPos, canvasSize);
            ImVec2 b = UVToCanvas({ u, 1.f }, canvasPos, canvasSize);
            dl->AddLine(a, b, IM_COL32(50, 50, 60, 200));
        }
        for (float v = 0.f; v <= 1.001f; v += gridStep)
        {
            ImVec2 a = UVToCanvas({ 0.f, v }, canvasPos, canvasSize);
            ImVec2 b = UVToCanvas({ 1.f, v }, canvasPos, canvasSize);
            dl->AddLine(a, b, IM_COL32(50, 50, 60, 200));
        }

        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            auto p0 = UVToCanvas(shape_.vertices[shape_.indices[i]].uv, canvasPos, canvasSize);
            auto p1 = UVToCanvas(shape_.vertices[shape_.indices[i + 1]].uv, canvasPos, canvasSize);
            auto p2 = UVToCanvas(shape_.vertices[shape_.indices[i + 2]].uv, canvasPos, canvasSize);
            dl->AddTriangleFilled(p0, p1, p2, IM_COL32(80, 120, 60, 80));
            dl->AddTriangle(p0, p1, p2, IM_COL32(120, 180, 80, 200), 1.5f);
        }

        const float vertRadius = 6.f;
        const float hitRadius  = 10.f;
        ImVec2 mp = ImGui::GetMousePos();

        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
        {
            ImVec2 sp = UVToCanvas(shape_.vertices[i].uv, canvasPos, canvasSize);
            ImU32 col = IsVertSelected(i)
                        ? IM_COL32(255, 200, 50, 255)
                        : IM_COL32(180, 220, 140, 255);
            dl->AddCircleFilled(sp, vertRadius, col);
            dl->AddCircle(sp, vertRadius, IM_COL32(0, 0, 0, 200), 0, 1.5f);

            char idx[8];
            snprintf(idx, sizeof(idx), "%d", i);
            dl->AddText(ImVec2(sp.x + 8, sp.y - 12), IM_COL32(180, 180, 180, 200), idx);
        }

        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("##uvCanvas", canvasSize,
                               ImGuiButtonFlags_MouseButtonLeft);
        bool canvasHovered = ImGui::IsItemHovered();
        bool canvasActive  = ImGui::IsItemActive();

        if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            int closest = -1;
            float closestDist = hitRadius;
            for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
            {
                ImVec2 sp = UVToCanvas(shape_.vertices[i].uv, canvasPos, canvasSize);
                float dx = mp.x - sp.x, dy = mp.y - sp.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < closestDist) { closestDist = dist; closest = i; }
            }
            if (closest >= 0)
            {
                bool shift = ImGui::GetIO().KeyShift;
                SelectVert(closest, shift);
                draggingUV_ = true;
            }
        }

        if (draggingUV_ && selectedVert_ >= 0 && canvasActive &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            glm::vec2 uv = CanvasToUV(mp, canvasPos, canvasSize);
            uv.x = glm::clamp(uv.x, 0.f, 1.f);
            uv.y = glm::clamp(uv.y, 0.f, 1.f);

            if (snapEnabled_ && snapToGrid_)
            {
                uv.x = SnapValue(uv.x, snapSize_);
                uv.y = SnapValue(uv.y, snapSize_);
            }

            shape_.vertices[selectedVert_].uv = uv;
            MarkCustom();
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            draggingUV_ = false;

        dl->PopClipRect();
    }

} // namespace ettycc
