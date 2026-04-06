#include <UI/SpriteEditor.hpp>
#include <Engine.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

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

    // ── Main draw ────────────────────────────────────────────────────────────

    void SpriteEditor::Draw(const std::shared_ptr<Engine>& engine)
    {
        if (!isOpen) return;

        ImGui::SetNextWindowSize(ImVec2(1100, 620), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Sprite Editor", &isOpen))
        {
            ImGui::End();
            return;
        }

        DrawToolbar();
        ImGui::Separator();

        // ── 3-column layout: Vertices | UVs | Inspector ─────────────────
        float inspectorWidth = 240.f;
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float canvasWidth = (avail.x - inspectorWidth - 16.f) * 0.5f;

        // Left: vertex canvas
        ImGui::BeginChild("##VertexPanel", ImVec2(canvasWidth, 0), true);
        {
            ImGui::TextDisabled("Vertices");
            ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            DrawVertexCanvas(canvasPos, canvasSize);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Center: UV canvas
        ImGui::BeginChild("##UVPanel", ImVec2(canvasWidth, 0), true);
        {
            ImGui::TextDisabled("UVs");
            ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            DrawUVCanvas(canvasPos, canvasSize);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right: inspector
        ImGui::BeginChild("##SpriteInspector", ImVec2(inspectorWidth, 0), true);
        {
            DrawShapePresets();
            ImGui::Separator();
            DrawSnapConfig();
            ImGui::Separator();
            DrawVertexInspector();
            ImGui::Separator();
            DrawShapeInfo();
        }
        ImGui::EndChild();

        ImGui::End();
    }

    // ── Toolbar ──────────────────────────────────────────────────────────────

    void SpriteEditor::DrawToolbar()
    {
        // Add / Remove vertex
        if (ImGui::Button("+ Vertex"))
        {
            shape_.vertices.push_back({ { 0.f, 0.f }, { 0.5f, 0.5f } });
            shape_.preset = SpriteShape::Preset::Custom;
            shape_.name = "Custom";
            selectedVert_ = static_cast<int>(shape_.vertices.size()) - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("- Vertex") && selectedVert_ >= 0 &&
            selectedVert_ < static_cast<int>(shape_.vertices.size()))
        {
            shape_.vertices.erase(shape_.vertices.begin() + selectedVert_);
            // Remove indices that reference this vertex and remap others
            std::vector<unsigned int> newIndices;
            for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
            {
                unsigned int a = shape_.indices[i];
                unsigned int b = shape_.indices[i + 1];
                unsigned int c = shape_.indices[i + 2];
                if (a == static_cast<unsigned int>(selectedVert_) ||
                    b == static_cast<unsigned int>(selectedVert_) ||
                    c == static_cast<unsigned int>(selectedVert_))
                    continue;
                if (a > static_cast<unsigned int>(selectedVert_)) --a;
                if (b > static_cast<unsigned int>(selectedVert_)) --b;
                if (c > static_cast<unsigned int>(selectedVert_)) --c;
                newIndices.push_back(a);
                newIndices.push_back(b);
                newIndices.push_back(c);
            }
            shape_.indices = newIndices;
            shape_.preset = SpriteShape::Preset::Custom;
            shape_.name = "Custom";
            selectedVert_ = -1;
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Add triangle from last 3 selected vertices
        if (ImGui::Button("+ Triangle (auto)"))
        {
            int n = static_cast<int>(shape_.vertices.size());
            if (n >= 3)
            {
                shape_.indices.push_back(static_cast<unsigned int>(n - 3));
                shape_.indices.push_back(static_cast<unsigned int>(n - 2));
                shape_.indices.push_back(static_cast<unsigned int>(n - 1));
                shape_.preset = SpriteShape::Preset::Custom;
                shape_.name = "Custom";
            }
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Drag-drop source: drag this shape onto the viewport
        ImGui::Button("Drag to Viewport");
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            // Store shape pointer for the drop target
            const SpriteShape* shapePtr = &shape_;
            ImGui::SetDragDropPayload("SPRITE_SHAPE",
                                      &shapePtr, sizeof(SpriteShape*));
            ImGui::Text("Drop onto viewport to spawn sprite");
            ImGui::EndDragDropSource();
        }

        ImGui::SameLine();

        // Zoom control
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f);
        ImGui::DragFloat("Zoom", &canvasZoom_, 1.f, 20.f, 500.f, "%.0f");
    }

    // ── Shape presets ────────────────────────────────────────────────────────

    void SpriteEditor::DrawShapePresets()
    {
        ImGui::SeparatorText("Shape Presets");

        if (ImGui::Button("Quad", ImVec2(-1, 0)))
        {
            shape_ = SpriteShape::MakeQuad();
            selectedVert_ = -1;
        }
        if (ImGui::Button("Triangle", ImVec2(-1, 0)))
        {
            shape_ = SpriteShape::MakeTriangle();
            selectedVert_ = -1;
        }
        if (ImGui::Button("Circle", ImVec2(-1, 0)))
        {
            shape_ = SpriteShape::MakeCircle(shape_.circleSegments);
            selectedVert_ = -1;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragInt("Segments", &shape_.circleSegments, 1, 3, 128))
        {
            if (shape_.preset == SpriteShape::Preset::Circle)
            {
                shape_ = SpriteShape::MakeCircle(shape_.circleSegments);
                selectedVert_ = -1;
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

    // ── Vertex inspector ─────────────────────────────────────────────────────

    void SpriteEditor::DrawVertexInspector()
    {
        ImGui::SeparatorText("Vertex Properties");

        if (selectedVert_ < 0 || selectedVert_ >= static_cast<int>(shape_.vertices.size()))
        {
            ImGui::TextDisabled("No vertex selected");
            return;
        }

        auto& vert = shape_.vertices[selectedVert_];
        char label[64];
        snprintf(label, sizeof(label), "Vertex %d", selectedVert_);
        ImGui::Text("%s", label);

        ImGui::SetNextItemWidth(-1);
        bool changed = false;
        changed |= ImGui::DragFloat2("Position", &vert.position.x, 0.01f);
        ImGui::SetNextItemWidth(-1);
        changed |= ImGui::DragFloat2("UV", &vert.uv.x, 0.01f);

        if (changed)
        {
            shape_.preset = SpriteShape::Preset::Custom;
            shape_.name = "Custom";
        }

        ImGui::Spacing();

        // Show all vertices in a scrollable list
        ImGui::SeparatorText("All Vertices");
        ImGui::BeginChild("##VertList", ImVec2(0, 150), true);
        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
        {
            char buf[80];
            snprintf(buf, sizeof(buf), "[%d] (%.2f, %.2f) uv(%.2f, %.2f)",
                     i,
                     shape_.vertices[i].position.x,
                     shape_.vertices[i].position.y,
                     shape_.vertices[i].uv.x,
                     shape_.vertices[i].uv.y);
            if (ImGui::Selectable(buf, selectedVert_ == i))
                selectedVert_ = i;
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
    }

    // ── Vertex canvas ────────────────────────────────────────────────────────

    void SpriteEditor::DrawVertexCanvas(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Background
        dl->AddRectFilled(canvasPos,
                          ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                          IM_COL32(30, 30, 30, 255));

        // Clip to canvas
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

            // Axes
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
            dl->AddTriangle(p0, p1, p2, IM_COL32(100, 150, 220, 200), 1.5f);
        }

        // ── Draw edges ───────────────────────────────────────────────────
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            unsigned int idx[3] = { shape_.indices[i], shape_.indices[i+1], shape_.indices[i+2] };
            for (int e = 0; e < 3; ++e)
            {
                auto a = WorldToCanvas(shape_.vertices[idx[e]].position, canvasPos, canvasSize);
                auto b = WorldToCanvas(shape_.vertices[idx[(e+1)%3]].position, canvasPos, canvasSize);
                dl->AddLine(a, b, IM_COL32(100, 150, 220, 200), 1.0f);
            }
        }

        // ── Draw vertices ────────────────────────────────────────────────
        const float vertRadius = 6.f;
        const float hitRadius  = 10.f;
        ImVec2 mp = ImGui::GetMousePos();

        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
        {
            ImVec2 sp = WorldToCanvas(shape_.vertices[i].position, canvasPos, canvasSize);
            ImU32 col = (i == selectedVert_)
                        ? IM_COL32(255, 200, 50, 255)
                        : IM_COL32(220, 220, 220, 255);
            dl->AddCircleFilled(sp, vertRadius, col);
            dl->AddCircle(sp, vertRadius, IM_COL32(0, 0, 0, 200), 0, 1.5f);

            // Vertex index label
            char idx[8];
            snprintf(idx, sizeof(idx), "%d", i);
            dl->AddText(ImVec2(sp.x + 8, sp.y - 12), IM_COL32(180, 180, 180, 200), idx);
        }

        // ── Interaction: invisible button over canvas ────────────────────
        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("##vertCanvas", canvasSize,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        bool canvasHovered = ImGui::IsItemHovered();
        bool canvasActive  = ImGui::IsItemActive();

        // Click to select vertex
        if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            int closest = -1;
            float closestDist = hitRadius;
            for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
            {
                ImVec2 sp = WorldToCanvas(shape_.vertices[i].position, canvasPos, canvasSize);
                float dx = mp.x - sp.x, dy = mp.y - sp.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < closestDist) { closestDist = dist; closest = i; }
            }
            selectedVert_ = closest;
            if (closest >= 0)
                draggingVert_ = true;
        }

        // Drag selected vertex
        if (draggingVert_ && selectedVert_ >= 0 && canvasActive &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            glm::vec2 worldPos = CanvasToWorld(mp, canvasPos, canvasSize);

            // Apply snap
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
                        if (i == selectedVert_) continue;
                        glm::vec2 diff = worldPos - shape_.vertices[i].position;
                        if (glm::length(diff) < snapVertexDist_)
                        {
                            worldPos = shape_.vertices[i].position;
                            break;
                        }
                    }
                }
            }

            shape_.vertices[selectedVert_].position = worldPos;
            shape_.preset = SpriteShape::Preset::Custom;
            shape_.name = "Custom";
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            draggingVert_ = false;

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

        // Right-click drag to pan
        if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            canvasOffset_.x += delta.x / canvasZoom_;
            canvasOffset_.y -= delta.y / canvasZoom_;
        }

        dl->PopClipRect();
    }

    // ── UV canvas ────────────────────────────────────────────────────────────

    void SpriteEditor::DrawUVCanvas(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Background
        dl->AddRectFilled(canvasPos,
                          ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                          IM_COL32(25, 25, 35, 255));

        dl->PushClipRect(canvasPos,
                         ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

        // UV space boundary [0,1]
        ImVec2 uvMin = UVToCanvas({ 0.f, 0.f }, canvasPos, canvasSize);
        ImVec2 uvMax = UVToCanvas({ 1.f, 1.f }, canvasPos, canvasSize);
        dl->AddRect(uvMin, uvMax, IM_COL32(100, 100, 100, 200), 0.f, 0, 1.5f);

        // UV grid
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

        // Draw UV triangles
        for (size_t i = 0; i + 2 < shape_.indices.size(); i += 3)
        {
            auto p0 = UVToCanvas(shape_.vertices[shape_.indices[i]].uv, canvasPos, canvasSize);
            auto p1 = UVToCanvas(shape_.vertices[shape_.indices[i + 1]].uv, canvasPos, canvasSize);
            auto p2 = UVToCanvas(shape_.vertices[shape_.indices[i + 2]].uv, canvasPos, canvasSize);
            dl->AddTriangleFilled(p0, p1, p2, IM_COL32(80, 120, 60, 80));
            dl->AddTriangle(p0, p1, p2, IM_COL32(120, 180, 80, 200), 1.5f);
        }

        // Draw UV vertices
        const float vertRadius = 6.f;
        const float hitRadius  = 10.f;
        ImVec2 mp = ImGui::GetMousePos();

        for (int i = 0; i < static_cast<int>(shape_.vertices.size()); ++i)
        {
            ImVec2 sp = UVToCanvas(shape_.vertices[i].uv, canvasPos, canvasSize);
            ImU32 col = (i == selectedVert_)
                        ? IM_COL32(255, 200, 50, 255)
                        : IM_COL32(180, 220, 140, 255);
            dl->AddCircleFilled(sp, vertRadius, col);
            dl->AddCircle(sp, vertRadius, IM_COL32(0, 0, 0, 200), 0, 1.5f);

            char idx[8];
            snprintf(idx, sizeof(idx), "%d", i);
            dl->AddText(ImVec2(sp.x + 8, sp.y - 12), IM_COL32(180, 180, 180, 200), idx);
        }

        // ── UV interaction ───────────────────────────────────────────────
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
            selectedVert_ = closest;
            if (closest >= 0) draggingUV_ = true;
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
            shape_.preset = SpriteShape::Preset::Custom;
            shape_.name = "Custom";
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            draggingUV_ = false;

        dl->PopClipRect();
    }

} // namespace ettycc
