#include <UI/DevEditor.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <imgui_internal.h>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Dependency.hpp>
#include <Dependencies/Resources.hpp>
#include <Input/Controls/EditorCamera.hpp>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <portable-file-dialogs.h>

#undef max
#undef min

/* MURO DE LA FUNA (TODOS)
 * - fix that stupid shit that makes the headers from windows affect std headers, this comes from the portable file dialogs (isolate headers???)
 * - refactor when this file is above 4444 lines (just because i said so)
 */

namespace ettycc
{
    static int viewportNumber = 1;
    static bool frameBufferErrorShown = false;

    DevEditor::DevEditor(const std::shared_ptr<Engine>& engine)
        : engineInstance_(engine), uiConsoleOpen_(false)
    {
        // Register early so logs emitted during Engine::Init() are captured.
        // DebugConsole::AddLog only uses malloc/ImVector — no ImGui context needed.
        auto consoleSink = std::make_shared<ImGuiConsoleSink_mt>(&uiConsole);
        spdlog::default_logger()->sinks().push_back(consoleSink);
    }

    DevEditor::~DevEditor() = default;

    // -------------------------------------------------------------------------
    // ASSET BROWSER HELPERS
    // -------------------------------------------------------------------------

    DevEditor::AssetType DevEditor::GetAssetType(const std::filesystem::path& p) const
    {
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".json") {
            std::string ps = p.string();
            if (ps.find("templates") != std::string::npos) return AssetType::Template;
            if (ps.find("scenes")    != std::string::npos) return AssetType::Scene;
            return AssetType::Config;
        }
        if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc")
            return AssetType::Code;
        if (ext == ".glsl" || ext == ".vert" || ext == ".frag")
            return AssetType::Shader;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
            return AssetType::Image;
        return AssetType::Unknown;
    }

    ImVec4 DevEditor::GetAssetColor(AssetType t) const
    {
        switch (t) {
            case AssetType::Template: return {0.27f, 0.51f, 0.71f, 1.f}; // steel blue
            case AssetType::Scene:    return {0.24f, 0.70f, 0.44f, 1.f}; // sea green
            case AssetType::Config:   return {0.63f, 0.63f, 0.63f, 1.f}; // gray
            case AssetType::Code:     return {0.90f, 0.55f, 0.10f, 1.f}; // orange
            case AssetType::Shader:   return {0.58f, 0.44f, 0.86f, 1.f}; // purple
            case AssetType::Image:    return {0.13f, 0.70f, 0.67f, 1.f}; // teal
            default:                  return {0.35f, 0.35f, 0.35f, 1.f};
        }
    }

    const char* DevEditor::GetAssetLabel(AssetType t) const
    {
        switch (t) {
            case AssetType::Template: return "TPL";
            case AssetType::Scene:    return "SCN";
            case AssetType::Config:   return "CFG";
            case AssetType::Code:     return "C++";
            case AssetType::Shader:   return "SHD";
            case AssetType::Image:    return "IMG";
            default:                  return "???";
        }
    }

    const char* DevEditor::GetAssetTypeName(AssetType t) const
    {
        switch (t) {
            case AssetType::Template: return "Prefab Template";
            case AssetType::Scene:    return "Scene";
            case AssetType::Config:   return "Config";
            case AssetType::Code:     return "Source Code";
            case AssetType::Shader:   return "Shader";
            case AssetType::Image:    return "Image";
            default:                  return "Unknown";
        }
    }

    void DevEditor::ScanAssets()
    {
        assetEntries_.clear();
        const std::string root = engineInstance_->engineResources_->GetWorkingFolder();
        if (root.empty()) return;

        // Default selected folder to root on first scan
        if (currentFolder_.empty())
            currentFolder_ = root;

        try {
            for (const auto& e : std::filesystem::recursive_directory_iterator(root)) {
                if (!e.is_regular_file()) continue;
                AssetEntry ae;
                ae.path = e.path().string();
                ae.name = e.path().filename().string();
                ae.type = GetAssetType(e.path());
                assetEntries_.push_back(ae);
            }
        } catch (const std::exception& ex) {
            spdlog::error("[DevEditor] ScanAssets failed: {}", ex.what());
        }

        assetsScanned_ = true;
        spdlog::info("[DevEditor] Scanned {} assets", assetEntries_.size());
    }

    // Recursive folder tree (left panel)
    void DevEditor::RenderFolderTree(const std::filesystem::path& path)
    {
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(path, ec))
        {
            if (ec || !e.is_directory()) continue;

            bool hasSubDirs = false;
            for (const auto& sub : std::filesystem::directory_iterator(e.path(), ec))
                if (!ec && sub.is_directory()) { hasSubDirs = true; break; }

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                                     | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!hasSubDirs)
                flags |= ImGuiTreeNodeFlags_Leaf;
            if (currentFolder_ == e.path().string())
                flags |= ImGuiTreeNodeFlags_Selected;

            bool open = ImGui::TreeNodeEx(e.path().filename().string().c_str(), flags);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                currentFolder_ = e.path().string();

            if (open)
            {
                RenderFolderTree(e.path());
                ImGui::TreePop();
            }
        }
    }

    // File icon grid called either for a folder view or for search results
    void DevEditor::RenderAssetGrid(const std::string& searchQuery)
    {
        const float iconSize = 58.f;
        const float cellW    = iconSize + 14.f;
        const float panelW   = ImGui::GetContentRegionAvail().x;
        const int   columns  = std::max(1, static_cast<int>(panelW / cellW));
        const bool  isSearch = !searchQuery.empty();

        ImGui::Columns(columns, "##asset_grid", false);

        int visibleIdx = 0;
        for (const auto& entry : assetEntries_)
        {
            // Search mode: match anywhere in name
            if (isSearch)
            {
                if (entry.name.find(searchQuery) == std::string::npos) continue;
            }
            else
            {
                // Folder mode: only show direct children of currentFolder_
                auto parent = std::filesystem::path(entry.path).parent_path().string();
                if (parent != currentFolder_) continue;
            }

            ImGui::PushID(visibleIdx++);

            ImVec4 col   = GetAssetColor(entry.type);
            ImU32  col32 = ImGui::ColorConvertFloat4ToU32(col);
            const char* lbl = GetAssetLabel(entry.type);

            ImVec2 iconPos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Colored icon background
            dl->AddRectFilled(iconPos,
                              ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                              col32, 6.f);

            // Type label centered
            ImVec2 txtSz = ImGui::CalcTextSize(lbl);
            dl->AddText(ImVec2(iconPos.x + (iconSize - txtSz.x) * 0.5f,
                               iconPos.y + (iconSize - txtSz.y) * 0.5f),
                        IM_COL32(255, 255, 255, 230), lbl);

            ImGui::InvisibleButton("##icon", ImVec2(iconSize, iconSize));

            // Hover highlight
            if (ImGui::IsItemHovered())
                dl->AddRect(iconPos,
                            ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
                            IM_COL32(255, 255, 255, 190), 6.f, 0, 2.f);

            // LEFT-CLICK
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                selectedAsset_.path     = entry.path;
                selectedAsset_.name     = entry.name;
                selectedAsset_.type     = entry.type;
                selectedAsset_.active   = true;
                try { selectedAsset_.fileSize = std::filesystem::file_size(entry.path); }
                catch (...) { selectedAsset_.fileSize = 0; }
                inspectorSource_ = InspectorSource::Asset;
                selectedNodes_.clear();
            }

            // Drag source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("ASSET_ENTRY",
                                          entry.path.c_str(),
                                          entry.path.size() + 1);
                ImGui::TextColored(col, "%s", lbl);
                ImGui::SameLine();
                ImGui::Text("%s", entry.name.c_str());
                if (entry.type == AssetType::Template)
                    ImGui::TextDisabled("Drop onto viewport to spawn");
                ImGui::EndDragDropSource();
            }

            ImGui::TextWrapped("%s", entry.name.c_str());
            ImGui::Spacing();

            ImGui::PopID();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::TextDisabled("%d item(s)  |  click = inspect  |  drag onto viewport = spawn (TPL)",
                            visibleIdx);
    }

    // -------------------------------------------------------------------------
    // DOCK SPACE / MENU BAR
    // -------------------------------------------------------------------------

    void DevEditor::ShowDockSpace()
    {
        static ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGuiID dsId = ImGui::DockSpaceOverViewport(NULL, dockFlags);
        // Default layout
        // TODO: Make an multi layout system
        static bool layoutBuilt = false;
        if (!layoutBuilt)
        {
            layoutBuilt = true;

            ImGui::DockBuilderRemoveNode(dsId);
            ImGui::DockBuilderAddNode(dsId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dsId, ImGui::GetMainViewport()->Size);

            // ── Split left strip (Hierarchy) ─────────────────────────────────
            ImGuiID left, rest;
            ImGui::DockBuilderSplitNode(dsId, ImGuiDir_Left, 0.18f, &left, &rest);

            // ── Split right strip (Inspector) ────────────────────────────────
            ImGuiID centre, right;
            ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.24f, &right, &centre);

            // ── Split bottom strip (Debug + Assets) ──────────────────────────
            ImGuiID viewport_row, bottom;
            ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.26f, &bottom, &viewport_row);

            ImGuiID bottomLeft, bottomRight;
            ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.38f, &bottomLeft, &bottomRight);

            // ── Assign windows ───────────────────────────────────────────────
            ImGui::DockBuilderDockWindow("Scene Hierarchy", left);
            ImGui::DockBuilderDockWindow("Editor view",    viewport_row);
            ImGui::DockBuilderDockWindow("Game view",      viewport_row); // tab behind Editor view
            ImGui::DockBuilderDockWindow("Inspector",      right);
            ImGui::DockBuilderDockWindow("Debug",          bottomLeft);
            ImGui::DockBuilderDockWindow("Assets",         bottomRight);
            ImGui::DockBuilderDockWindow("Build",          bottomRight); // tab behind Assets

            ImGui::DockBuilderFinish(dsId);
        }
    }

    void DevEditor::ShowMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open", NULL))
                {
                    auto f = pfd::open_file("Choose a scene",
                        engineInstance_->engineResources_->GetWorkingFolder(),
                        { "Json files", "*.json", "All Files", "*" },
                        pfd::opt::force_overwrite);
                    spdlog::info("Opening file from dev editor...");
                    engineInstance_->LoadScene(f.result().at(0), false);
                }
                if (ImGui::MenuItem("Save", NULL))
                {
                    auto f = pfd::save_file("Select a path",
                        engineInstance_->engineResources_->GetWorkingFolder(),
                        { "Json files", "*.json", "All Files", "*" },
                        pfd::opt::none);
                    spdlog::info("Saving file from dev editor: " + f.result());
                    engineInstance_->StoreScene(f.result(), false);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Editor"))
            {
                ImGui::MenuItem("Configure", NULL);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::BeginMenu("Built-in"))
                {
                    ImGui::MenuItem("View port", NULL);
                    ImGui::MenuItem("Debug", NULL);
                    ImGui::MenuItem("Scene", NULL);
                    ImGui::MenuItem("Assets", NULL);
                    ImGui::MenuItem("Inspector", NULL);
                    ImGui::MenuItem("Game view", NULL);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    // -------------------------------------------------------------------------
    // VIEWPORT
    // -------------------------------------------------------------------------

    void DevEditor::ShowEditorViewPort()
    {
        ImGui::Begin("Editor view");

        auto showPlaceholder = [](const char* msg) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 textSize = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f, (avail.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", msg);
        };

        if (const auto framebuffer = GetDependency(Engine)->renderEngine_.GetViewPortFrameBuffer())
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            framebuffer->SetSize(glm::ivec2(static_cast<int>(avail.x), static_cast<int>(avail.y)));
            ImGui::SetCursorPos(ImGui::GetCursorPos());

            ImGui::Image(reinterpret_cast<void*>(
                static_cast<intptr_t>(framebuffer->GetTextureId())),
                avail, ImVec2(0, 1), ImVec2(1, 0));

            // Screen-space top-left of the rendered image (= content area origin)
            const ImVec2 imgMin = ImGui::GetItemRectMin();
            const ImVec2 imgMax = { imgMin.x + avail.x, imgMin.y + avail.y };
            const ImVec2 mp     = ImGui::GetMousePos();

            // ─── Prefab drag-drop (unchanged) ────────────────────────────────
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ENTRY"))
                {
                    std::string path(static_cast<const char*>(payload->Data), payload->DataSize - 1);
                    AssetType dropType = GetAssetType(std::filesystem::path(path));
                    if (dropType == AssetType::Template)
                    {
                        for (auto& node : assetBuilder_->BuildFromTemplate(path))
                        {
                            engineInstance_->mainScene_->root_node_->AddChild(node);
                            spdlog::info("[DevEditor] Spawned '{}' from prefab '{}'",
                                         node->GetName(), path);
                        }
                    }
                    else
                        spdlog::warn("[DevEditor] Drop type '{}' not handled yet",
                                     GetAssetTypeName(dropType));
                }
                ImGui::EndDragDropTarget();
            }

            // ─── Gizmo persistent state ───────────────────────────────────────
            static int gizmoMode = 0; // 0=Translate  1=Rotate  2=Scale

            enum GizmoAxis : int {
                AXIS_NONE = -1, AXIS_XY = 0, AXIS_X = 1, AXIS_Y = 2,
                AXIS_ROTATE = 3, AXIS_SX = 4, AXIS_SY = 5, AXIS_SXY = 6
            };
            static GizmoAxis hovered  = AXIS_NONE;
            static GizmoAxis dragging = AXIS_NONE;
            static ImVec2    dragStartMouse = {};
            static glm::vec3 dragStartPos   = {};
            static glm::vec3 dragStartRot   = {}; // stored degrees
            static glm::vec3 dragStartScale = {};
            static float     dragStartAngle = 0.f;

            // ─── Resolve selected RenderableNode ──────────────────────────────
            std::shared_ptr<RenderableNode> selRN;
            if (!selectedNodes_.empty() && inspectorSource_ == InspectorSource::SceneNode)
            {
                auto comp = selectedNodes_.back()->GetComponentByName(RenderableNode::componentType);
                selRN = std::dynamic_pointer_cast<RenderableNode>(comp);
            }

            // ─── Gizmo hover detection (runs before click tests) ──────────────
            // Must run every frame so click-to-select knows if a handle is under
            // the cursor before registering a select.
            auto distSeg = [](ImVec2 p, ImVec2 a, ImVec2 b) -> float {
                float dx = b.x - a.x, dy = b.y - a.y;
                float len2 = dx*dx + dy*dy;
                if (len2 < 1e-6f) return sqrtf((p.x-a.x)*(p.x-a.x)+(p.y-a.y)*(p.y-a.y));
                float t  = glm::clamp(((p.x-a.x)*dx + (p.y-a.y)*dy) / len2, 0.f, 1.f);
                float cx = a.x + t*dx, cy = a.y + t*dy;
                return sqrtf((p.x-cx)*(p.x-cx)+(p.y-cy)*(p.y-cy));
            };

            ImVec2 gizmoOrigin = {};
            ImVec2 gizmoXTip   = {};
            ImVec2 gizmoYTip   = {};
            float  worldPerPixel = 1.f;
            bool   mouseInVP  = ImGui::IsMouseHoveringRect(imgMin, imgMax);

            constexpr float HANDLE_LEN = 60.f;
            constexpr float HIT_R      = 10.f;
            constexpr float RING_R     = 52.f;
            constexpr float SQ_HALF    = 6.f;

            if (selRN && selRN->renderable_)
            {
                auto& cam = engineInstance_->editorCamera_->editorCameraControl_;
                glm::mat4 view = cam->ComputeViewMatrix(0.f);
                glm::mat4 proj = cam->ComputeProjectionMatrix(0.f);
                worldPerPixel  = (2.f * EditorCamera::baseSize_) / (cam->zoom * avail.y);

                auto toScreen = [&](glm::vec3 wp) -> ImVec2 {
                    glm::vec4 c = proj * view * glm::vec4(wp, 1.f);
                    glm::vec3 n = glm::vec3(c) / c.w;
                    return { imgMin.x + (n.x * 0.5f + 0.5f) * avail.x,
                             imgMin.y + (1.f - (n.y * 0.5f + 0.5f)) * avail.y };
                };

                glm::vec3 wPos = selRN->renderable_->GetTransform().getGlobalPosition(); //TODO: fix this big pice of shit, transforms should come from the node not an specific component
                gizmoOrigin = toScreen(wPos);
                gizmoXTip   = { gizmoOrigin.x + HANDLE_LEN, gizmoOrigin.y };
                gizmoYTip   = { gizmoOrigin.x, gizmoOrigin.y - HANDLE_LEN };

                float distO = sqrtf((mp.x-gizmoOrigin.x)*(mp.x-gizmoOrigin.x) +
                                    (mp.y-gizmoOrigin.y)*(mp.y-gizmoOrigin.y));

                if (dragging == AXIS_NONE && mouseInVP)
                {
                    hovered = AXIS_NONE;
                    if (gizmoMode == 0) // Translate
                    {
                        if (distO < HIT_R)                                   hovered = AXIS_XY;
                        else if (distSeg(mp, gizmoOrigin, gizmoXTip) < HIT_R) hovered = AXIS_X;
                        else if (distSeg(mp, gizmoOrigin, gizmoYTip) < HIT_R) hovered = AXIS_Y;
                    }
                    else if (gizmoMode == 1) // Rotate
                    {
                        if (fabsf(distO - RING_R) < 9.f)                     hovered = AXIS_ROTATE;
                    }
                    else // Scale
                    {
                        if (distO < HIT_R)
                            hovered = AXIS_SXY;
                        else if (fabsf(mp.x - gizmoXTip.x) < SQ_HALF + 3.f &&
                                 fabsf(mp.y - gizmoXTip.y) < SQ_HALF + 3.f)
                            hovered = AXIS_SX;
                        else if (fabsf(mp.x - gizmoYTip.x) < SQ_HALF + 3.f &&
                                 fabsf(mp.y - gizmoYTip.y) < SQ_HALF + 3.f)
                            hovered = AXIS_SY;
                        else if (distSeg(mp, gizmoOrigin, gizmoXTip) < HIT_R) hovered = AXIS_SX;
                        else if (distSeg(mp, gizmoOrigin, gizmoYTip) < HIT_R) hovered = AXIS_SY;
                    }
                }
                else if (!mouseInVP && dragging == AXIS_NONE)
                    hovered = AXIS_NONE;
            }
            else
            {
                hovered  = AXIS_NONE;
                dragging = AXIS_NONE;
            }

            // ─── Gizmo mode toolbar (T/R/S) ───────────────────────────────────
            bool toolbarConsumedClick = false;
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const float btnSz = 22.f, pad = 6.f, gap = 3.f;
                const char* labels[] = { "T", "R", "S" };
                for (int i = 0; i < 3; ++i)
                {
                    ImVec2 bMin = { imgMin.x + pad + i * (btnSz + gap), imgMin.y + pad };
                    ImVec2 bMax = { bMin.x + btnSz, bMin.y + btnSz };
                    bool active = (gizmoMode == i);
                    bool btnHov = ImGui::IsMouseHoveringRect(bMin, bMax);
                    ImU32 bgCol = active ? IM_COL32( 80, 140, 255, 220) :
                                  btnHov ? IM_COL32( 80,  80,  80, 200) :
                                           IM_COL32( 40,  40,  40, 180);
                    dl->AddRectFilled(bMin, bMax, bgCol, 3.f);
                    dl->AddRect(bMin, bMax, IM_COL32(120, 120, 120, 180), 3.f);
                    ImVec2 tsz = ImGui::CalcTextSize(labels[i]);
                    dl->AddText({ bMin.x + (btnSz - tsz.x) * 0.5f,
                                  bMin.y + (btnSz - tsz.y) * 0.5f },
                                IM_COL32(230, 230, 230, 255), labels[i]);
                    if (btnHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        gizmoMode           = i;
                        toolbarConsumedClick = true;
                    }
                    if (btnHov)
                        toolbarConsumedClick = true; // also block picker on hover
                }
            }

            // ─── Click-to-select via picker ────────────────────────────────────
            // Only fires when no gizmo handle is under the cursor and no toolbar
            // button has consumed the click.
            if (!toolbarConsumedClick &&
                dragging == AXIS_NONE &&
                hovered  == AXIS_NONE &&
                mouseInVP && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                auto pbSz = pickerBuffer_->size_;
                int px = glm::clamp((int)(mp.x - imgMin.x), 0, pbSz.x - 1);
                int py = glm::clamp((int)(mp.y - imgMin.y), 0, pbSz.y - 1);

                uint32_t pickedId = pickerBuffer_->ReadPixel(px, py);
                lastPickedId_     = pickedId;

                if (pickedId > 0)
                {
                    const auto& renderables = engineInstance_->renderEngine_.GetRenderables();
                    if (pickedId - 1 < renderables.size())
                    {
                        auto node = FindNodeByRenderable(
                            engineInstance_->mainScene_->root_node_,
                            renderables[pickedId - 1]);
                        if (node)
                        {
                            selectedNodes_.clear();
                            selectedNodes_.push_back(node);
                            inspectorSource_      = InspectorSource::SceneNode;
                            selectedAsset_.active = false;
                        }
                    }
                }
                else
                {
                    selectedNodes_.clear();
                    selectedAsset_.active = false;
                    inspectorSource_      = InspectorSource::None;
                }
            }

            // ─── Helper: get RigidBodyComponent on the selected node (may be null) ──
            auto getSelRB = [&]() -> std::shared_ptr<RigidBodyComponent> {
                if (selectedNodes_.empty()) return nullptr;
                auto comp = selectedNodes_.back()->GetComponentByName(RigidBodyComponent::componentType);
                return std::dynamic_pointer_cast<RigidBodyComponent>(comp);
            };

            // ─── Gizmo drag start ─────────────────────────────────────────────
            if (hovered != AXIS_NONE && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && selRN)
            {
                dragging       = hovered;
                dragStartMouse = mp;
                Transform t    = selRN->renderable_->GetTransform();
                dragStartPos   = t.getGlobalPosition();
                dragStartRot   = t.getStoredRotation();   // degrees
                dragStartScale = t.getGlobalScale();
                dragStartAngle = atan2f(mp.y - gizmoOrigin.y, mp.x - gizmoOrigin.x);

                // If the node has a rigid body, switch it to kinematic so the
                // physics simulation no longer fights the gizmo.
                if (auto rb = getSelRB()) rb->BeginManipulation();
            }

            // ─── Gizmo drag apply ─────────────────────────────────────────────
            if (dragging != AXIS_NONE && ImGui::IsMouseDown(ImGuiMouseButton_Left) && selRN)
            {
                float dxPx = mp.x - dragStartMouse.x;
                float dyPx = mp.y - dragStartMouse.y;

                // All three branches resolve to (pos, rot, scale) and then call
                // SetFromTRS which builds a correct T*R*S matrix, avoiding the
                // composition bugs in the individual setter chain.
                glm::vec3 pos   = dragStartPos;
                glm::quat rot   = glm::quat(glm::radians(dragStartRot));
                glm::vec3 scale = dragStartScale;

                if (dragging == AXIS_X || dragging == AXIS_Y || dragging == AXIS_XY)
                {
                    if (dragging != AXIS_Y) pos.x += dxPx * worldPerPixel;
                    if (dragging != AXIS_X) pos.y -= dyPx * worldPerPixel; // screen-Y is flipped
                }
                else if (dragging == AXIS_ROTATE)
                {
                    float curAngle = atan2f(mp.y - gizmoOrigin.y, mp.x - gizmoOrigin.x);
                    float deltaDeg = glm::degrees(curAngle - dragStartAngle);
                    glm::vec3 newEuler = dragStartRot;
                    newEuler.z -= deltaDeg;
                    rot = glm::quat(glm::radians(newEuler));
                }
                else // Scale
                {
                    constexpr float SENS = 0.012f;
                    if (dragging == AXIS_SX  || dragging == AXIS_SXY)
                        scale.x = glm::max(0.001f, scale.x + dxPx * SENS);
                    if (dragging == AXIS_SY  || dragging == AXIS_SXY)
                        scale.y = glm::max(0.001f, scale.y - dyPx * SENS);
                }

                Transform newT;
                newT.SetFromTRS(pos, rot, scale);
                selRN->renderable_->SetTransform(newT);

                // Keep the bullet body in sync with the visual while in kinematic mode
                if (auto rb = getSelRB()) rb->SyncFromRenderable();
            }

            // ─── Gizmo drag release ───────────────────────────────────────────
            if (dragging != AXIS_NONE && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // Restore dynamic physics before clearing the drag state
                if (auto rb = getSelRB()) rb->EndManipulation();
                dragging = AXIS_NONE;
            }

            // ─── Draw gizmo handles ───────────────────────────────────────────
            if (selRN && selRN->renderable_)
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();

                auto isActive = [&](GizmoAxis a) {
                    return hovered == a || dragging == a;
                };
                constexpr ImU32 HIGHLIGHT = IM_COL32(255, 230, 60, 255);

                if (gizmoMode == 0) // TRANSLATE
                {
                    ImU32 cX = isActive(AXIS_X) || isActive(AXIS_XY) ? HIGHLIGHT : IM_COL32(220, 50, 50, 255);
                    ImU32 cY = isActive(AXIS_Y) || isActive(AXIS_XY) ? HIGHLIGHT : IM_COL32( 50,200, 50, 255);

                    dl->AddLine(gizmoOrigin, gizmoXTip, cX, 2.f);
                    dl->AddLine(gizmoOrigin, gizmoYTip, cY, 2.f);
                    dl->AddTriangleFilled({ gizmoXTip.x+7.f, gizmoXTip.y },
                                         { gizmoXTip.x-3.f, gizmoXTip.y-4.f },
                                         { gizmoXTip.x-3.f, gizmoXTip.y+4.f }, cX);
                    dl->AddTriangleFilled({ gizmoYTip.x,     gizmoYTip.y-7.f },
                                         { gizmoYTip.x-4.f, gizmoYTip.y+3.f },
                                         { gizmoYTip.x+4.f, gizmoYTip.y+3.f }, cY);
                    dl->AddText({ gizmoXTip.x + 9.f, gizmoXTip.y - 6.f }, cX, "X");
                    dl->AddText({ gizmoYTip.x + 4.f, gizmoYTip.y - 14.f }, cY, "Y");

                    ImU32 cC = isActive(AXIS_XY) ? HIGHLIGHT : IM_COL32(220, 220, 220, 255);
                    dl->AddCircleFilled(gizmoOrigin, 6.f, cC);
                    dl->AddCircle(gizmoOrigin, 6.f, IM_COL32(60, 60, 60, 200));
                }
                else if (gizmoMode == 1) // ROTATE
                {
                    bool active = isActive(AXIS_ROTATE);
                    ImU32 cR = active ? HIGHLIGHT : IM_COL32(220, 130, 50, 220);
                    dl->AddCircle(gizmoOrigin, RING_R, cR, 64, active ? 3.f : 2.f);
                    dl->AddCircleFilled(gizmoOrigin, 4.f, IM_COL32(220, 220, 220, 255));
                }
                else // SCALE
                {
                    ImU32 cX = isActive(AXIS_SX)  ? HIGHLIGHT : IM_COL32(220,  50,  50, 255);
                    ImU32 cY = isActive(AXIS_SY)  ? HIGHLIGHT : IM_COL32( 50, 200,  50, 255);
                    ImU32 cC = isActive(AXIS_SXY) ? HIGHLIGHT : IM_COL32(220, 220, 220, 255);

                    dl->AddLine(gizmoOrigin, gizmoXTip, cX, 2.f);
                    dl->AddLine(gizmoOrigin, gizmoYTip, cY, 2.f);
                    dl->AddRectFilled({ gizmoXTip.x-SQ_HALF, gizmoXTip.y-SQ_HALF },
                                     { gizmoXTip.x+SQ_HALF, gizmoXTip.y+SQ_HALF }, cX);
                    dl->AddRectFilled({ gizmoYTip.x-SQ_HALF, gizmoYTip.y-SQ_HALF },
                                     { gizmoYTip.x+SQ_HALF, gizmoYTip.y+SQ_HALF }, cY);
                    dl->AddText({ gizmoXTip.x + 9.f, gizmoXTip.y - 6.f }, cX, "X");
                    dl->AddText({ gizmoYTip.x + 4.f, gizmoYTip.y - 14.f }, cY, "Y");
                    // Center diamond = uniform scale
                    dl->AddQuadFilled({ gizmoOrigin.x,       gizmoOrigin.y - 7.f },
                                     { gizmoOrigin.x + 7.f, gizmoOrigin.y       },
                                     { gizmoOrigin.x,       gizmoOrigin.y + 7.f },
                                     { gizmoOrigin.x - 7.f, gizmoOrigin.y       }, cC);
                }
            }

            // ─── Camera pan / focus ───────────────────────────────────────────
            static bool    isViewportFocused = false;
            static ImVec2  lockedCursorPos;

            bool gizmoOccupied = (dragging != AXIS_NONE) || (hovered != AXIS_NONE);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_None))
            {
                // Disable camera pan while a gizmo handle is active
                engineInstance_->editorCamera_->editorCameraControl_->enabled = !gizmoOccupied;

                if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    if (!isViewportFocused) {
                        isViewportFocused = true;
                        lockedCursorPos   = ImGui::GetMousePos();
                    }
                    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
                    ImGui::GetIO().MousePos = lockedCursorPos;
                }
                else
                    isViewportFocused = false;
            }
            else
            {
                isViewportFocused = false;
                engineInstance_->editorCamera_->editorCameraControl_->enabled = false;
            }
        }
        else
        {
            showPlaceholder("Viewport framebuffer not ready");
        }

        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // FIND NODE BY RENDERABLE
    // -------------------------------------------------------------------------

    std::shared_ptr<SceneNode> DevEditor::FindNodeByRenderable(
        const std::shared_ptr<SceneNode>& node,
        const std::shared_ptr<Renderable>& renderable) const
    {
        // RenderableNode (sprites, cameras, …)
        auto comp = node->GetComponentByName(RenderableNode::componentType);
        if (comp)
        {
            auto rn = std::dynamic_pointer_cast<RenderableNode>(comp);
            if (rn && rn->renderable_ == renderable)
                return node;
        }

        // SoftBodyComponent owns its renderable directly (no RenderableNode wrapper)
        auto sbComp = node->GetComponentByName(SoftBodyComponent::componentType);
        if (sbComp)
        {
            auto sb = std::dynamic_pointer_cast<SoftBodyComponent>(sbComp);
            if (sb && sb->GetRenderable() == renderable)
                return node;
        }

        for (const auto& child : node->children_)
        {
            auto found = FindNodeByRenderable(child, renderable);
            if (found) return found;
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // GAME VIEW
    // -------------------------------------------------------------------------

    void DevEditor::ShowGameView() const
    {
        ImGui::Begin("Game view");

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 textSize = ImGui::CalcTextSize("No game camera in scene");
        ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f, (avail.y - textSize.y) * 0.5f));
        ImGui::TextDisabled("No game camera in scene");

        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // INSPECTOR
    // -------------------------------------------------------------------------

    namespace {
        struct TransformUI {
            float pos[3];
            float rot[3];
            float scale[3];
            bool initialized = false;
        };

        void SetTransformUIFromNode(const std::shared_ptr<SceneNode>& node, TransformUI& uiTransform) {
            auto spriteComponent = node->GetComponentByName(RenderableNode::componentType);
            if (spriteComponent) {
                auto renderableNode = std::dynamic_pointer_cast<RenderableNode>(spriteComponent);
                if (renderableNode && renderableNode->renderable_) {
                    Transform t = renderableNode->renderable_->GetTransform();
                    const auto pos   = t.getGlobalPosition();
                    const auto rot   = t.getGlobalRotation();
                    const auto scale = t.getGlobalScale();
                    uiTransform.pos[0]   = pos.x;   uiTransform.pos[1]   = pos.y;   uiTransform.pos[2]   = pos.z;
                    uiTransform.rot[0]   = rot.x;   uiTransform.rot[1]   = rot.y;   uiTransform.rot[2]   = rot.z;
                    uiTransform.scale[0] = scale.x; uiTransform.scale[1] = scale.y; uiTransform.scale[2] = scale.z;
                }
            }
        }
    }

    void DevEditor::ShowInspector()
    {
        ImGui::Begin("Inspector");

        // --- ASSET SELECTED ---
        if (inspectorSource_ == InspectorSource::Asset && selectedAsset_.active)
        {
            ImGui::TextColored(GetAssetColor(selectedAsset_.type), "%s", GetAssetLabel(selectedAsset_.type));
            ImGui::SameLine();
            ImGui::Text("%s", selectedAsset_.name.c_str());
            ImGui::Separator();

            ImGui::TextDisabled("Type");
            ImGui::SameLine(80);
            ImGui::Text("%s", GetAssetTypeName(selectedAsset_.type));

            ImGui::TextDisabled("Size");
            ImGui::SameLine(80);
            if (selectedAsset_.fileSize < 1024)
                ImGui::Text("%llu B", static_cast<unsigned long long>(selectedAsset_.fileSize));
            else
                ImGui::Text("%.1f KB", static_cast<double>(selectedAsset_.fileSize) / 1024.0);

            ImGui::TextDisabled("Path");
            ImGui::SameLine(80);
            ImGui::TextWrapped("%s", selectedAsset_.path.c_str());

            if (selectedAsset_.type == AssetType::Template)
            {
                ImGui::Spacing();
                ImGui::TextWrapped("Drag onto the viewport to spawn this prefab into the scene.");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored({1.0f, 0.6f, 0.15f, 1.0f}, "[ Source: Asset ]");
            ImGui::End();
            return;
        }

        // --- NO SCENE SELECTION ---
        if (selectedNodes_.empty())
        {
            ImGui::TextDisabled("No selection");
            ImGui::Separator();
            ImGui::TextWrapped("Select a node in the Scene Hierarchy or right-click an asset.");
            ImGui::End();
            return;
        }

        // --- MULTIPLE NODES SELECTED ---
        if (selectedNodes_.size() > 1)
        {
            ImGui::Text("Multiple Selection (%zu)", selectedNodes_.size());
            ImGui::Separator();
            ImGui::TextWrapped("Multi-object editing is not yet supported.");
            ImGui::Separator();
            ImGui::TextColored({0.3f, 0.9f, 0.3f, 1.0f}, "[ Source: Scene Node ]");
            ImGui::End();
            return;
        }

        // --- SINGLE NODE SELECTED ---
        auto selectedNode = selectedNodes_.back();

        // ── Node name ──────────────────────────────────────────────────────────
        char nameBuf[128] = "";
        {
            std::string name = selectedNode->GetName();
            strncpy(nameBuf, name.empty() ? "UNNAMED" : name.c_str(), sizeof(nameBuf) - 1);
        }
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("##name", nameBuf, IM_ARRAYSIZE(nameBuf)))
            selectedNode->SetName(nameBuf);

        ImGui::Spacing();

        // ── Transform section ─────────────────────────────────────────────────
        // Rehydrate the UI cache once per selection; after that the user drives it.
        static std::unordered_map<const void*, TransformUI> transformCache;
        const void* nodeKey = selectedNode.get();
        TransformUI& uiTransform = transformCache[nodeKey];
        if (!uiTransform.initialized)
        {
            SetTransformUIFromNode(selectedNode, uiTransform);
            uiTransform.initialized = true;
        }

        // Find the renderable and (optionally) the sibling RigidBodyComponent once.
        RenderableNode*      renderableNode = nullptr;
        RigidBodyComponent*  rigidBody      = nullptr;
        {
            auto rc = selectedNode->GetComponentByName(RenderableNode::componentType);
            if (rc) renderableNode = dynamic_cast<RenderableNode*>(rc.get());

            auto rb = selectedNode->GetComponentByName(RigidBodyComponent::componentType);
            if (rb) rigidBody = dynamic_cast<RigidBodyComponent*>(rb.get());
        }

        bool transformChanged  = false;
        bool transformActivated  = false;
        bool transformDeactivated = false;

        // Helper: render one labeled DragFloat3 row, track drag events.
        auto dragRow = [&](const char* label, float* v, float speed)
        {
            const float avail  = ImGui::GetContentRegionAvail().x;
            const float lw     = ImMax(60.f, avail * 0.28f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine(lw);
            ImGui::SetNextItemWidth(avail - lw);
            char id[32]; snprintf(id, sizeof(id), "##t_%s", label);
            transformChanged     |= ImGui::DragFloat3(id, v, speed, 0.f, 0.f, "%.3f");
            transformActivated   |= ImGui::IsItemActivated();
            transformDeactivated |= ImGui::IsItemDeactivated();
        };

        ImGui::SeparatorText("Transform");
        dragRow("Position", uiTransform.pos,   0.05f);
        dragRow("Rotation", uiTransform.rot,   0.5f);
        dragRow("Scale",    uiTransform.scale, 0.01f);

        // ── Apply transform changes + sync physics ─────────────────────────────
        if (transformActivated && rigidBody)
            rigidBody->BeginManipulation();

        if (transformChanged && renderableNode && renderableNode->renderable_)
        {
            Transform t;
            t.setGlobalPosition({uiTransform.pos[0],   uiTransform.pos[1],   uiTransform.pos[2]});
            t.setGlobalRotation({uiTransform.rot[0],   uiTransform.rot[1],   uiTransform.rot[2]});
            t.setGlobalScale   ({uiTransform.scale[0], uiTransform.scale[1], uiTransform.scale[2]});
            renderableNode->renderable_->SetTransform(t);

            if (rigidBody && rigidBody->IsManipulated())
                rigidBody->SyncFromRenderable();
        }

        if (transformDeactivated && rigidBody && rigidBody->IsManipulated())
            rigidBody->EndManipulation();

        ImGui::Spacing();

        // ── Components ────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::SmallButton("Add Component"))
                ImGui::OpenPopup("add_component_popup");

            if (ImGui::BeginPopup("add_component_popup"))
            {
                if (ImGui::MenuItem("Camera")) AddComponentFromTemplate(selectedNode, "Camera");
                if (ImGui::MenuItem("Sprite")) AddComponentFromTemplate(selectedNode, "Sprite");
                ImGui::EndPopup();
            }

            ImGui::Spacing();

            for (auto& [channel, compList] : selectedNode->components_)
            {
                for (auto& comp : compList)
                {
                    if (!comp) continue;

                    auto info = comp->GetComponentInfo();

                    // Channel badge: tiny colored text before the component name
                    const bool isRender = (channel == ProcessingChannel::RENDERING);
                    ImVec4 badgeCol = isRender
                        ? ImVec4(0.35f, 0.75f, 1.f,  1.f)
                        : ImVec4(1.f,   0.75f, 0.2f, 1.f);

                    char headerLabel[128];
                    snprintf(headerLabel, sizeof(headerLabel),
                             "%s##comp_%llu", info.name.c_str(), (unsigned long long)info.id);

                    // Render the badge inline before the header
                    ImGui::PushStyleColor(ImGuiCol_Text, badgeCol);
                    ImGui::Bullet();
                    ImGui::PopStyleColor();
                    ImGui::SameLine();

                    bool open = ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen);
                    if (!open) continue;

                    ImGui::PushID(static_cast<int>(info.id));
                    ImGui::Indent(12.f);
                    ImGui::Spacing();

                    EditorPropertyVisitor visitor;
                    comp->InspectProperties(visitor);

                    if (visitor.propertyCount == 0)
                        ImGui::TextDisabled("No exposed properties");

                    ImGui::Spacing();
                    ImGui::Unindent(12.f);
                    ImGui::PopID();
                }
            }
        }

        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // SCENE HIERARCHY
    // -------------------------------------------------------------------------

    void DevEditor::ShowSceneHierarchy()
    {
        ImGui::Begin("Scene Hierarchy");
        RenderSceneTree();
        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // ASSETS VIEW
    // -------------------------------------------------------------------------

    void DevEditor::ShowAssetsView()
    {
        ImGui::Begin("Assets");

        if (!assetsScanned_)
            ScanAssets();

        // --- Top bar: search + refresh ---
        static char searchQuery[128] = "";
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.f);
        ImGui::InputText("##search", searchQuery, IM_ARRAYSIZE(searchQuery));
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            ScanAssets();

        ImGui::Separator();

        if (strlen(searchQuery) > 0)
        {
            // --- SEARCH MODE: flat grid of all matches ---
            RenderAssetGrid(searchQuery);
        }
        else
        {
            // --- BROWSE MODE: folder tree left | file grid right ---
            const float totalH      = ImGui::GetContentRegionAvail().y;
            const float folderPanelW = 150.f;
            const float filePanelW   = ImGui::GetContentRegionAvail().x - folderPanelW - 6.f;

            // Left: folder tree
            ImGui::BeginChild("##folder_tree", ImVec2(folderPanelW, totalH), true);
            {
                // Root entry (clicking goes back to root)
                const std::string root = engineInstance_->engineResources_->GetWorkingFolder();
                ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow
                                            | ImGuiTreeNodeFlags_DefaultOpen
                                            | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (currentFolder_ == root)
                    rootFlags |= ImGuiTreeNodeFlags_Selected;

                bool rootOpen = ImGui::TreeNodeEx("[root]", rootFlags);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    currentFolder_ = root;
                if (rootOpen)
                {
                    RenderFolderTree(root);
                    ImGui::TreePop();
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // Right: file icons in the selected folder
            ImGui::BeginChild("##file_grid", ImVec2(filePanelW, totalH), false);
            {
                // Breadcrumb path relative to root
                const std::string root = engineInstance_->engineResources_->GetWorkingFolder();
                std::string rel = currentFolder_.size() > root.size()
                                ? currentFolder_.substr(root.size() + 1)
                                : "[root]";
                ImGui::TextDisabled("%s", rel.c_str());
                ImGui::Separator();
                RenderAssetGrid("");
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // SCENE CONTEXT MENU / NODES
    // -------------------------------------------------------------------------

    static bool showPopup = false;
    void DevEditor::ShowSceneContextMenu(const std::shared_ptr<SceneNode>& node)
    {
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::BeginMenu("Add"))
            {
                if (ImGui::MenuItem("Node"))
                    showPopup = true;
                if (ImGui::BeginMenu("Components"))
                {
                    if (ImGui::MenuItem("Camera", NULL))
                        AddComponentFromTemplate(node, "Camera");
                    if (ImGui::MenuItem("Sprite", NULL))
                        AddComponentFromTemplate(node, "Sprite");
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Remove"))   { /* TODO */ }
            if (ImGui::MenuItem("Duplicate")){ /* TODO */ }
            if (ImGui::MenuItem("Persist"))  { /* TODO */ }
            ImGui::EndPopup();
        }
    }

    void DevEditor::AddNode(const std::shared_ptr<SceneNode>& selectedNode)
    {
        static char node_name[128] = "";

        if (showPopup)
        {
            ImGui::OpenPopup("new node");
            showPopup = false;
        }

        if (ImGui::BeginPopupModal("new node", NULL))
        {
            ImGui::Text("Enter node name:");
            auto pressed = ImGui::InputText("##node_name", node_name, IM_ARRAYSIZE(node_name),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Apply") || pressed)
            {
                selectedNode->AddChild(std::make_shared<SceneNode>(node_name));
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    static int positionIndex = 2;
    void DevEditor::AddComponentFromTemplate(const std::shared_ptr<SceneNode>& selectedNode, const char* templateName)
    {
        const char* notFoundTexturePath = "D:/repos2/ALPHA_V1/assets/images/not_found_texture.png";//NOOOOO

        if (strcmp("Camera", templateName) == 0)
            spdlog::info("Camera spawning not yet implemented");

        if (strcmp("Sprite", templateName) == 0)
        {
            auto notFoundSprite = std::make_shared<Sprite>(notFoundTexturePath);
            notFoundSprite->underylingTransform.setGlobalPosition({(float)positionIndex, (float)positionIndex, -5.f});
            selectedNode->AddComponent(std::make_shared<RenderableNode>(notFoundSprite));
        }

        positionIndex += positionIndex;
    }

    void DevEditor::RenderSceneNode(const std::shared_ptr<SceneNode>& rootNode,
                                    std::vector<std::shared_ptr<SceneNode>>& selectedNodes,
                                    int depth)
    {
        auto treeNodeName = rootNode->GetName();
        const char* nodeName = treeNodeName.empty() ? "UNNAMED" : treeNodeName.c_str();

        bool isNodeOpen = ImGui::TreeNodeEx(nodeName);
        bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), rootNode) != selectedNodes.end();

        if (ImGui::IsItemClicked(0))
        {
            if (!ImGui::GetIO().KeyShift)
                selectedNodes.clear();
            if (!isSelected)
                selectedNodes.push_back(rootNode);
            else
                selectedNodes.erase(std::remove(selectedNodes.begin(), selectedNodes.end(), rootNode),
                                    selectedNodes.end());

            // Scene node takes over the inspector
            inspectorSource_ = InspectorSource::SceneNode;
            selectedAsset_.active = false;
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("SCENE_NODE", &rootNode, sizeof(std::shared_ptr<SceneNode>));
            ImGui::TextUnformatted(nodeName);
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
            {
                IM_ASSERT(payload->DataSize == sizeof(std::shared_ptr<SceneNode>));
                // TODO: reparenting logic
            }
            ImGui::EndDragDropTarget();
        }

        ShowSceneContextMenu(rootNode);

        if (isNodeOpen)
        {
            if (!rootNode->children_.empty())
                ImGui::Separator();
            for (auto& child : rootNode->children_)
                RenderSceneNode(child, selectedNodes, depth + 1);
            ImGui::TreePop();
        }
    }

    void DevEditor::RenderSceneTree()
    {
        ImGui::SameLine();
        static char search[32] = "Object name...";
        ImGui::InputText("##Search", search, IM_ARRAYSIZE(search));
        ImGui::SameLine();
        ImGui::Separator();
        RenderSceneNode(GetDependency(Engine)->mainScene_->root_node_, selectedNodes_, 0);
    }

    // -------------------------------------------------------------------------
    // DEBUGGER
    // -------------------------------------------------------------------------

    struct Point { ImVec2 pos; bool selected; };
    static std::vector<Point> curvePoints;

    static void CurveEditor()
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 start = window->DC.CursorPos;
        ImVec2 size(400, 200);
        ImVec2 end = ImVec2(start.x + size.x, start.y + size.y);

        ImGui::GetWindowDrawList()->AddRectFilled(start, end, IM_COL32(50, 50, 50, 255));
        ImGui::GetWindowDrawList()->AddRect(start, end, IM_COL32(255, 255, 255, 255));

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mouse_in_canvas = ImVec2(io.MousePos.x - start.x, io.MousePos.y - start.y);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(start, end))
            curvePoints.push_back({mouse_in_canvas, false});

        for (Point& p : curvePoints)
        {
            ImVec2 p1 = ImVec2(start.x + p.pos.x, start.y + p.pos.y);
            ImVec2 p2 = ImVec2(p1.x + 5, p1.y + 5);
            ImGui::GetWindowDrawList()->AddRectFilled(p1, p2,
                p.selected ? IM_COL32(255,0,0,255) : IM_COL32(255,255,255,255));
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(p1, p2))
                p.selected = !p.selected;
        }
    }

    static const int MAX_SAMPLES = 32;
    static float values[MAX_SAMPLES] = {0.0f};
    static int sampleIndex = 0;
    static float plotStep = 0.0f;

    void DevEditor::ShowDebugger()
    {
        auto engineInstance = GetDependency(Engine);
        ImGui::Begin("Debug");

        if (ImGui::BeginTabBar("tools", ImGuiTabBarFlags_Reorderable))
        {
            if (ImGui::BeginTabItem("Stats"))
            {
                auto mousepos = engineInstance->inputSystem_.GetMousePos();
                ImGui::Text("Delta time: %.4f",  engineInstance->appInstance_->GetDeltaTime());
                ImGui::Text("Mouse x: [%i] y:[%i]", mousepos.x, mousepos.y);
                ImGui::Text("FPS: %.4f", (1.0f / engineInstance->appInstance_->GetDeltaTime()));

                plotStep += engineInstance->appInstance_->GetDeltaTime();
                ImGui::PlotHistogram("##HISTOGRAM", values, IM_ARRAYSIZE(values));

                if (sampleIndex < MAX_SAMPLES && plotStep >= 1)
                {
                    values[sampleIndex] = 1.0f / engineInstance->appInstance_->GetDeltaTime();
                    sampleIndex++;
                    plotStep = 0;
                }
                else if (sampleIndex >= MAX_SAMPLES)
                {
                    for (int i = 0; i < MAX_SAMPLES; i++) values[i] = 0;
                    sampleIndex = 0;
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Editor Camera"))
            {
                auto& cam  = engineInstance->editorCamera_->editorCameraControl_;
                auto& proj = engineInstance->editorCamera_->ProjectionMatrix;

                ImGui::SeparatorText("Transform");
                ImGui::SliderFloat2("Position", &cam->position.x, -50.0f, 50.0f, "%.3f");
                ImGui::InputFloat2("Position (edit)", &cam->position.x, "%.3f");
                ImGui::SliderFloat("Zoom", &cam->zoom, 0.01f, 20.0f, "%.3f");
                ImGui::InputFloat("Zoom (edit)", &cam->zoom, 0.01f, 1.0f, "%.3f");

                ImGui::SeparatorText("Projection");
                bool isPersp = engineInstance->editorCamera_->isSetPerspective();
                ImGui::Text("Mode: %s", isPersp ? "Perspective" : "Orthographic");
                ImGui::Text("Matrix row 0: %.3f  %.3f  %.3f  %.3f", proj[0][0], proj[0][1], proj[0][2], proj[0][3]);
                ImGui::Text("Matrix row 1: %.3f  %.3f  %.3f  %.3f", proj[1][0], proj[1][1], proj[1][2], proj[1][3]);

                ImGui::SeparatorText("Framebuffer");
                auto fb = engineInstance->renderEngine_.GetViewPortFrameBuffer();
                if (fb)
                {
                    auto sz = fb->GetSize();
                    ImGui::Text("Size: %d x %d", sz.x, sz.y);
                    ImGui::Text("Texture ID: %u", fb->GetTextureId());
                }

                ImGui::SeparatorText("Object Picker (color ID buffer)");
                if (pickerBuffer_ && pickerBuffer_->initialized_)
                {
                    ImGui::Text("Last picked ID: %u", lastPickedId_);
                    ImVec2 previewSize(ImGui::GetContentRegionAvail().x, 140.0f);
                    ImGui::Image(
                        reinterpret_cast<void*>(static_cast<intptr_t>(pickerBuffer_->GetTextureId())),
                        previewSize, ImVec2(0, 1), ImVec2(1, 0));

                    // Click on the preview to read the ID at that position
                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        ImVec2 mousePos  = ImGui::GetMousePos();
                        ImVec2 itemPos   = ImGui::GetItemRectMin();
                        ImVec2 itemSize  = ImGui::GetItemRectSize();
                        auto   pbSize    = pickerBuffer_->size_;
                        int px = static_cast<int>((mousePos.x - itemPos.x) / itemSize.x * pbSize.x);
                        int py = static_cast<int>((mousePos.y - itemPos.y) / itemSize.y * pbSize.y);
                        lastPickedId_ = pickerBuffer_->ReadPixel(px, py);
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Console"))
            {
                uiConsole.Draw("Debug console", &uiConsoleOpen_);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("CURVE EDITOR[TEST]"))
            {
                CurveEditor();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Network"))
            {
                static char     netIpBuf[64]  = "127.0.0.1";
                static int      netPort       = 7777;
                static NetState prevNetState  = NetState::OFFLINE;

                auto& nm    = engineInstance_->networkManager_;
                auto  state = nm.GetState();

                // ── Auto-load on connect ───────────────────────────────────────
                // When state transitions to CONNECTED, load (or reload) the
                // network scene immediately so both peers start in sync.
                if (prevNetState != NetState::CONNECTED && state == NetState::CONNECTED)
                {
                    engineInstance_->LoadNetworkScene();
                    engineInstance_->InitEditorCamera();
                }
                prevNetState = state;

                // ── Status ────────────────────────────────────────────────────
                ImGui::SeparatorText("Status");
                {
                    ImVec4 col;
                    switch (state)
                    {
                    case NetState::CONNECTED:    col = {0.2f, 1.f,  0.4f, 1.f}; break;
                    case NetState::CONNECTING:   col = {1.f,  0.8f, 0.2f, 1.f}; break;
                    case NetState::DISCONNECTED: col = {1.f,  0.3f, 0.3f, 1.f}; break;
                    default:                     col = {0.5f, 0.5f, 0.5f, 1.f}; break;
                    }
                    const char* role = nm.IsHost() ? "Host" : "Client";
                    if (state == NetState::OFFLINE)
                        ImGui::TextColored(col, "● %s", nm.GetStateStr());
                    else
                        ImGui::TextColored(col, "● %s  [%s]  %s:%d",
                                           nm.GetStateStr(), role, netIpBuf, netPort);
                }
                if (state == NetState::DISCONNECTED)
                {
                    ImGui::Spacing();
                    ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f},
                                       "Connection lost or rejected by remote.");
                    ImGui::TextDisabled("Physics restored to local simulation.");
                }

                // ── Config ───────────────────────────────────────────────────
                ImGui::SeparatorText("Config");
                {
                    bool locked = (state == NetState::CONNECTED ||
                                   state == NetState::CONNECTING);
                    ImGui::BeginDisabled(locked);
                    ImGui::InputText("Server IP", netIpBuf, sizeof(netIpBuf));
                    ImGui::InputInt("Port",       &netPort);
                    netPort = std::max(1, std::min(netPort, 65535));
                    ImGui::EndDisabled();
                }

                // ── Actions ───────────────────────────────────────────────────
                ImGui::SeparatorText("Actions");

                bool canStart = (state == NetState::OFFLINE ||
                                 state == NetState::DISCONNECTED);

                ImGui::BeginDisabled(!canStart);
                if (ImGui::Button("Start Host", {120, 0}))
                {
                    nm.Shutdown();
                    nm.InitHost(static_cast<uint16_t>(netPort));
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!canStart);
                if (ImGui::Button("Connect", {120, 0}))
                {
                    nm.Shutdown();
                    nm.InitClient(netIpBuf, static_cast<uint16_t>(netPort));
                }
                ImGui::EndDisabled();

                ImGui::BeginDisabled(canStart);
                if (ImGui::Button(state == NetState::CONNECTING ? "Cancel" : "Disconnect",
                                  {120, 0}))
                    nm.Shutdown();
                ImGui::EndDisabled();

                // ── Bandwidth & stats ─────────────────────────────────────────
                ImGui::SeparatorText("Bandwidth");
                {
                    // Helper: format bytes as B / KB / MB
                    auto fmtBytes = [](uint64_t b, char* buf, int sz) {
                        if      (b < 1024ULL)        snprintf(buf, sz, "%llu B",   (unsigned long long)b);
                        else if (b < 1024ULL * 1024) snprintf(buf, sz, "%.2f KB",  b / 1024.0);
                        else                         snprintf(buf, sz, "%.2f MB",  b / (1024.0 * 1024.0));
                    };
                    auto fmtBps = [](float bps, char* buf, int sz) {
                        if      (bps < 1024.f)        snprintf(buf, sz, "%.1f B/s",  bps);
                        else if (bps < 1024.f * 1024) snprintf(buf, sz, "%.2f KB/s", bps / 1024.f);
                        else                          snprintf(buf, sz, "%.2f MB/s", bps / (1024.f * 1024.f));
                    };

                    const auto& bw = nm.GetBandwidthStats();
                    char tmp[64];

                    // Connection uptime
                    if (state == NetState::CONNECTED)
                    {
                        int h = (int)(bw.connectedForSecs / 3600);
                        int m = (int)(bw.connectedForSecs / 60) % 60;
                        int s = (int)(bw.connectedForSecs) % 60;
                        ImGui::Text("Uptime:  %02d:%02d:%02d", h, m, s);
                    }

                    // Current rates
                    fmtBps(bw.sendBps, tmp, sizeof(tmp));
                    ImGui::Text("Send:    %s", tmp);
                    ImGui::SameLine(180);
                    fmtBytes(bw.totalBytesSent, tmp, sizeof(tmp));
                    ImGui::TextDisabled("total: %s", tmp);

                    fmtBps(bw.recvBps, tmp, sizeof(tmp));
                    ImGui::Text("Receive: %s", tmp);
                    ImGui::SameLine(180);
                    fmtBytes(bw.totalBytesReceived, tmp, sizeof(tmp));
                    ImGui::TextDisabled("total: %s", tmp);

                    ImGui::Text("Packets sent / received:  %llu / %llu",
                                (unsigned long long)nm.GetTotalSent(),
                                (unsigned long long)nm.GetTotalReceived());

                    // ── Graphs ────────────────────────────────────────────────
                    ImGui::Spacing();
                    float avail = ImGui::GetContentRegionAvail().x;

                    // Find max across both histories for a shared Y scale
                    float maxBw = 1.f; // never show a zero-scale graph
                    for (float v : nm.GetSendHistory()) maxBw = std::max(maxBw, v);
                    for (float v : nm.GetRecvHistory()) maxBw = std::max(maxBw, v);

                    fmtBps(bw.sendBps, tmp, sizeof(tmp));
                    ImGui::TextColored({0.3f, 0.9f, 0.4f, 1.f}, "▲ Send  %s", tmp);
                    ImGui::PlotLines("##bwsend",
                        nm.GetSendHistory().data(),
                        NetworkManager::kBwHistorySize,
                        nm.GetBwHistoryOffset(),
                        nullptr, 0.f, maxBw,
                        ImVec2(avail, 55));

                    fmtBps(bw.recvBps, tmp, sizeof(tmp));
                    ImGui::TextColored({0.3f, 0.6f, 1.f, 1.f}, "▼ Recv  %s", tmp);
                    ImGui::PlotLines("##bwrecv",
                        nm.GetRecvHistory().data(),
                        NetworkManager::kBwHistorySize,
                        nm.GetBwHistoryOffset(),
                        nullptr, 0.f, maxBw,
                        ImVec2(avail, 55));
                }

                // ── Per-object table ──────────────────────────────────────────
                const auto& entries = nm.GetDebugEntries();
                if (!entries.empty())
                {
                    ImGui::SeparatorText("Objects");
                    if (ImGui::BeginTable("##netdbg", 5,
                            ImGuiTableFlags_Borders |
                            ImGuiTableFlags_RowBg   |
                            ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_SizingFixedFit,
                            ImVec2(0, 130)))
                    {
                        ImGui::TableSetupColumn("ID");
                        ImGui::TableSetupColumn("X");
                        ImGui::TableSetupColumn("Y");
                        ImGui::TableSetupColumn("Z");
                        ImGui::TableSetupColumn("Pkts");
                        ImGui::TableHeadersRow();

                        for (const auto& [id, e] : entries)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::Text("%u",   id);
                            ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", e.pos.x);
                            ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", e.pos.y);
                            ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", e.pos.z);
                            ImGui::TableSetColumnIndex(4); ImGui::Text("%llu", (unsigned long long)e.count);
                        }
                        ImGui::EndTable();
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // MAIN LOOP
    // -------------------------------------------------------------------------

    void DevEditor::DrawEditor()
    {
        // Run picker pass every frame (separate FBO, separate shader)
        if (pickerBuffer_)
        {
            auto& re = engineInstance_->renderEngine_;
            pickerBuffer_->SetSize(engineInstance_->editorCamera_->offScreenFrameBuffer->GetSize());
            pickerBuffer_->RenderPass(re.GetRenderingContext(), re.GetRenderables());
        }

        ShowMenuBar();
        ShowDockSpace();
        ShowDebugger();
        ShowEditorViewPort();
        ShowGameView();
        ShowInspector();
        ShowSceneHierarchy();
        ShowAssetsView();
        buildPanel_.Draw();
    }

    void DevEditor::Init()
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        engineInstance_->InitEditorCamera();
        //TODO: MOVE EVERYTHING BELOW TO THE ENGINE...
        assetLoader_  = std::make_shared<AssetLoader>();
        assetBuilder_ = std::make_shared<AssetBuilder>(assetLoader_);

        pickerBuffer_ = std::make_shared<PickerBuffer>(glm::ivec2(1200, 800));
        pickerBuffer_->Init();

        auto resources   = GetDependency(Resources);
        auto shadersPath = resources->GetWorkingFolder() + "/" + resources->Get("paths", "shaders");
        pickerBuffer_->InitShader(shadersPath);
    }

    void DevEditor::UpdateUI() { DrawEditor(); }
    void DevEditor::Update()   {}

    GLuint DevEditor::LoadTextureFromFile(const char* filePath)
    {
        int width, height, numChannels;
        unsigned char* image = stbi_load(filePath, &width, &height, &numChannels, 0);

        GLuint textureID = 0;
        if (image)
        {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(image);
        }
        else
        {
            spdlog::error("DevEditor: failed to load texture '{}': {}", filePath, stbi_failure_reason());
        }

        return textureID;
    }

} // namespace ettycc
