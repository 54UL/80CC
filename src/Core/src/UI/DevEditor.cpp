#include <UI/DevEditor.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <imgui_internal.h>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Scene/Components/AudioSourceComponent.hpp>
#include <Scene/Components/AudioListenerComponent.hpp>
#include <Scene/Components/GravityAttractorComponent.hpp>
#include <Networking/NetworkComponent.hpp>
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
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

    const DevEditor::ResolutionPreset DevEditor::kResolutionPresets[] = {
        // PC
        { "1920x1080 (FHD)",    1920, 1080 },
        { "1280x720  (HD)",     1280,  720 },
        { "2560x1440 (QHD)",    2560, 1440 },
        { "3840x2160 (4K)",     3840, 2160 },
        { "1366x768",          1366,  768 },
        // Mobile
        { "390x844   (iPhone 14)",       390,  844 },
        { "360x800   (Samsung Galaxy)",  360,  800 },
        { "393x873   (Pixel 7)",         393,  873 },
        { "1024x768  (iPad)",           1024,  768 },
        { "820x1180  (iPad Air)",        820, 1180 },
    };
    const int DevEditor::kNumPresets =
        static_cast<int>(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]));

    DevEditor::DevEditor(const std::shared_ptr<Engine>& engine)
        : engineInstance_(engine), uiConsoleOpen_(false),
          buildPanel_(configurationsWindow_.GetBuildConfig())
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
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac")
            return AssetType::Audio;
        if (ext == ".material")
            return AssetType::Material;
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
            case AssetType::Audio:    return {0.90f, 0.40f, 0.80f, 1.f}; // magenta
            case AssetType::Material: return {0.85f, 0.65f, 0.13f, 1.f}; // gold
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
            case AssetType::Audio:    return "SFX";
            case AssetType::Material: return "MAT";
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
            case AssetType::Audio:    return "Audio Clip";
            case AssetType::Material: return "Material";
            default:                  return "Unknown";
        }
    }

    void DevEditor::ScanAssets()
    {
        assetEntries_.clear();
        const std::string root = engineInstance_->globals_->GetWorkingFolder();
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
                // General asset payload
                ImGui::SetDragDropPayload("ASSET_ENTRY",
                                          entry.path.c_str(),
                                          entry.path.size() + 1);

                // Also emit a typed payload for materials so drop targets can be specific
                if (entry.type == AssetType::Material)
                {
                    // Convert absolute path to relative (materials/xxx.material)
                    auto globals = GetDependency(Globals);
                    std::string relPath = entry.path;
                    if (globals)
                    {
                        const auto& wf = globals->GetWorkingFolder();
                        if (relPath.find(wf) == 0)
                            relPath = relPath.substr(wf.size());
                    }
                    ImGui::SetDragDropPayload("MATERIAL_ASSET",
                                              relPath.c_str(),
                                              relPath.size() + 1);
                }

                ImGui::TextColored(col, "%s", lbl);
                ImGui::SameLine();
                ImGui::Text("%s", entry.name.c_str());
                if (entry.type == AssetType::Template)
                    ImGui::TextDisabled("Drop onto viewport to spawn");
                if (entry.type == AssetType::Material)
                    ImGui::TextDisabled("Drop onto sprite to assign");
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

            // ── Split left strip  ─────────────────────────────────
            ImGuiID left, rest;
            ImGui::DockBuilderSplitNode(dsId, ImGuiDir_Left, 0.18f, &left, &rest);

            // ── Split right strip ────────────────────────────────
            ImGuiID centre, right;
            ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.24f, &right, &centre);

            // ── Split bottom strip ──────────────────────────
            ImGuiID viewport_row, bottom;
            ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.26f, &bottom, &viewport_row);

            ImGuiID bottomLeft, bottomRight;
            ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.38f, &bottomLeft, &bottomRight);

            // ── Assign windows ───────────────────────────────────────────────
            ImGui::DockBuilderDockWindow("Scene Hierarchy", bottomLeft);
            ImGui::DockBuilderDockWindow("Editor view",    viewport_row);
            ImGui::DockBuilderDockWindow("Game view",      viewport_row);
            ImGui::DockBuilderDockWindow("Debug",          bottomRight);
            ImGui::DockBuilderDockWindow("Assets",         bottomRight);
            ImGui::DockBuilderDockWindow("Build",          right);
            ImGui::DockBuilderDockWindow("Inspector",      right);

            ImGui::DockBuilderFinish(dsId);
        }
    }

    void DevEditor::ShowMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New", NULL))
                {
                    NewScene();
                }
                if (ImGui::MenuItem("Open", NULL))
                {
                    auto f = pfd::open_file("Choose a scene",
                        engineInstance_->globals_->GetWorkingFolder(),
                        { "Json files", "*.json", "All Files", "*" },
                        pfd::opt::force_overwrite);
                    spdlog::info("Opening file from dev editor...");
                    engineInstance_->LoadScene(f.result().at(0), false);
                }
                if (ImGui::MenuItem("Save", NULL))
                {
                    auto f = pfd::save_file("Select a path",
                        engineInstance_->globals_->GetWorkingFolder(),
                        { "Json files", "*.json", "All Files", "*" },
                        pfd::opt::none);
                    spdlog::info("Saving file from dev editor: " + f.result());
                    engineInstance_->StoreScene(f.result(), false);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Editor"))
            {
                if (ImGui::MenuItem("Configure", NULL))
                    configurationsWindow_.Open();
                if (ImGui::MenuItem("Sprite Editor", NULL))
                    spriteEditor_.isOpen = true;
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

    void DevEditor::DrawGravityAttractorGizmos(ImVec2 imgMin, ImVec2 imgSize)
    {
        if (!engineInstance_->mainScene_) return;

        auto& cam = engineInstance_->editorCamera_->editorCameraControl_;
        glm::mat4 view = cam->ComputeViewMatrix(0.f);
        glm::mat4 proj = cam->ComputeProjectionMatrix(0.f);
        float worldPerPixel = (2.f * EditorCamera::baseSize_) / (cam->zoom * imgSize.y);

        auto toScreen = [&](glm::vec3 wp) -> ImVec2 {
            glm::vec4 c = proj * view * glm::vec4(wp, 1.f);
            glm::vec3 n = glm::vec3(c) / c.w;
            return { imgMin.x + (n.x * 0.5f + 0.5f) * imgSize.x,
                     imgMin.y + (1.f - (n.y * 0.5f + 0.5f)) * imgSize.y };
        };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        auto& registry = engineInstance_->mainScene_->registry_;

        for (ecs::Entity e : registry.View<GravityAttractorComponent>())
        {
            auto* attractor = registry.Get<GravityAttractorComponent>(e);
            if (!attractor) continue;

            ImVec2 center = toScreen(attractor->GetPosition());

            const float innerPx = attractor->GetInnerRadius() / worldPerPixel;
            const float outerPx = attractor->GetOuterRadius() / worldPerPixel;

            // Inner ring — full-strength zone (cyan)
            dl->AddCircle(center, innerPx,
                          IM_COL32(0, 200, 255, 200), 64, 1.5f);
            // Outer ring — max gravitational influence (magenta, faded)
            dl->AddCircle(center, outerPx,
                          IM_COL32(200, 60, 255, 130), 64, 1.0f);

            // Labels
            dl->AddText({ center.x + innerPx + 4.f, center.y - 8.f },
                        IM_COL32(0, 200, 255, 220), "inner");
            dl->AddText({ center.x + outerPx + 4.f, center.y - 8.f },
                        IM_COL32(200, 60, 255, 200), "outer");
        }
    }

    void DevEditor::DrawAudioGizmos(ImVec2 imgMin, ImVec2 imgSize)
    {
        if (!engineInstance_->mainScene_) return;

        auto& cam = engineInstance_->editorCamera_->editorCameraControl_;
        glm::mat4 view = cam->ComputeViewMatrix(0.f);
        glm::mat4 proj = cam->ComputeProjectionMatrix(0.f);
        float worldPerPixel = (2.f * EditorCamera::baseSize_) / (cam->zoom * imgSize.y);

        auto toScreen = [&](glm::vec3 wp) -> ImVec2 {
            glm::vec4 c = proj * view * glm::vec4(wp, 1.f);
            glm::vec3 n = glm::vec3(c) / c.w;
            return { imgMin.x + (n.x * 0.5f + 0.5f) * imgSize.x,
                     imgMin.y + (1.f - (n.y * 0.5f + 0.5f)) * imgSize.y };
        };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        auto& registry = engineInstance_->mainScene_->registry_;

        for (ecs::Entity e : registry.View<AudioSourceComponent>())
        {
            auto* src = registry.Get<AudioSourceComponent>(e);
            if (!src || src->GetMode() != AudioSourceComponent::AudioMode::Spatial) continue;

            auto* node = engineInstance_->mainScene_->GetNode(e);
            if (!node) continue;

            ImVec2 center = toScreen(node->transform_.getGlobalPosition());

            const float minPx = src->GetMinDistance() / worldPerPixel;
            const float maxPx = src->GetMaxDistance() / worldPerPixel;

            // Inner ring — full-volume zone (bright green)
            dl->AddCircle(center, minPx,
                          IM_COL32(80, 230, 80, 200), 64, 1.5f);
            // Outer ring — silence boundary (orange, faded)
            dl->AddCircle(center, maxPx,
                          IM_COL32(255, 140, 40, 130), 64, 1.0f);

            dl->AddText({ center.x + minPx + 4.f, center.y - 8.f },
                        IM_COL32(80, 230, 80, 220), "min");
            dl->AddText({ center.x + maxPx + 4.f, center.y - 8.f },
                        IM_COL32(255, 140, 40, 200), "max");
        }
    }

    void DevEditor::ShowEditorViewPort()
    {
        ImGui::Begin("Editor view");

        // ─── Playback + overlay toggles ─────────────────────────────────
        DrawPlaybackToolbar();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.92f, 0.016f, 1.f));
        ImGui::Checkbox("Colliders", &showColliderDebug_);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.9f, 0.9f, 1.f));
        ImGui::Checkbox("Gravity", &showGravityDebug_);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.9f, 0.31f, 1.f));
        ImGui::Checkbox("Audio", &showAudioDebug_);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.60f, 0.85f, 1.f));
        ImGui::Checkbox("Grid", &gameViewShowGrid_);
        ImGui::PopStyleColor();
        ImGui::Separator();

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

            // ─── Prefab drag-drop ────────────────────────────────────────────
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
                // ─── Sprite shape drag-drop from Sprite Editor ───────────
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SPRITE_SHAPE"))
                {
                    const SpriteShape* shape = *static_cast<const SpriteShape* const*>(payload->Data);
                    auto sprite = std::make_shared<Sprite>("images/not_found_texture.png");
                    sprite->SetShape(*shape);

                    auto node = std::make_shared<SceneNode>("Sprite_" + shape->name);
                    node->AddComponent(RenderableNode(sprite));

                    engineInstance_->mainScene_->root_node_->AddChild(node);
                    engineInstance_->renderEngine_.AddRenderable(sprite);
                    spdlog::info("[DevEditor] Spawned sprite with shape '{}'", shape->name);
                }
                // ─── Material drag-drop onto viewport ────────────────────
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_ASSET"))
                {
                    std::string matPath(static_cast<const char*>(payload->Data), payload->DataSize - 1);

                    // If a sprite is selected, assign the material to it
                    bool assigned = false;
                    if (!selectedNodes_.empty())
                    {
                        auto selNode = selectedNodes_.back();
                        if (auto* rn = selNode->GetComponent<RenderableNode>())
                        {
                            if (auto* spr = dynamic_cast<Sprite*>(rn->renderable_.get()))
                            {
                                spr->SetMaterialPath(matPath);
                                assigned = true;
                                spdlog::info("[DevEditor] Assigned material '{}' to '{}'",
                                             matPath, selNode->GetName());
                            }
                        }
                    }

                    // If no sprite selected, spawn a new sprite with the material
                    if (!assigned)
                    {
                        auto sprite = std::make_shared<Sprite>();
                        sprite->SetMaterialPath(matPath);

                        std::string name = "Sprite_" +
                            std::filesystem::path(matPath).stem().string();
                        auto node = std::make_shared<SceneNode>(name);
                        node->AddComponent(RenderableNode(sprite));

                        engineInstance_->mainScene_->root_node_->AddChild(node);
                        engineInstance_->renderEngine_.AddRenderable(sprite);
                        spdlog::info("[DevEditor] Spawned sprite with material '{}'", matPath);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // ─── Gizmo persistent state ───────────────────────────────────────
            static int gizmoMode = 0; // 0=Translate  1=Rotate  2=Scale
            static bool gizmoLocalSpace = false; // false=Global(world), true=Local(object)

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

            // ─── Resolve selected node ────────────────────────────────────────
            std::shared_ptr<SceneNode> selNode;
            if (!selectedNodes_.empty() && inspectorSource_ == InspectorSource::SceneNode)
                selNode = selectedNodes_.back();

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

            if (selNode)
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

                glm::vec3 wPos = selNode->transform_.getGlobalPosition();
                gizmoOrigin = toScreen(wPos);

                // Compute axis directions in screen space
                // In global mode: world X = right, world Y = up
                // In local mode: axes rotate with the object
                glm::vec2 axisXDir(1.f, 0.f);  // screen-space X direction
                glm::vec2 axisYDir(0.f, -1.f);  // screen-space Y direction (screen Y is flipped)

                if (gizmoLocalSpace)
                {
                    float rotZ = glm::radians(selNode->transform_.getStoredRotation().z);
                    float cosR = cosf(rotZ), sinR = sinf(rotZ);
                    axisXDir = glm::vec2( cosR, -sinR); // screen-space (Y flipped)
                    axisYDir = glm::vec2( sinR,  cosR); // screen-space (Y flipped)
                    // Normalize (should already be unit)
                    axisXDir = glm::normalize(axisXDir);
                    axisYDir = glm::normalize(axisYDir);
                }

                gizmoXTip = { gizmoOrigin.x + axisXDir.x * HANDLE_LEN,
                              gizmoOrigin.y + axisXDir.y * HANDLE_LEN };
                gizmoYTip = { gizmoOrigin.x + axisYDir.x * HANDLE_LEN,
                              gizmoOrigin.y + axisYDir.y * HANDLE_LEN };

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

            // ─── Gizmo mode toolbar (T/R/S + L/G) — row 2 ─────────────────────
            bool toolbarConsumedClick = false;
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const float btnSz = 22.f, pad = 6.f, gap = 3.f;
                const float row2Y = imgMin.y + pad + btnSz + gap; // below overlay row
                const char* labels[] = { "T", "R", "S" };
                for (int i = 0; i < 3; ++i)
                {
                    ImVec2 bMin = { imgMin.x + pad + i * (btnSz + gap), row2Y };
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

                // Separator gap then Local/Global toggle button
                float lgX = imgMin.x + pad + 3 * (btnSz + gap) + gap * 2;
                {
                    const char* lgLabel = gizmoLocalSpace ? "L" : "G";
                    ImVec2 bMin = { lgX, row2Y };
                    ImVec2 bMax = { bMin.x + btnSz, bMin.y + btnSz };
                    bool btnHov = ImGui::IsMouseHoveringRect(bMin, bMax);
                    ImU32 bgCol = gizmoLocalSpace
                        ? IM_COL32(200, 140,  50, 220)   // orange = local
                        : btnHov ? IM_COL32( 80,  80,  80, 200)
                                 : IM_COL32( 40,  40,  40, 180);
                    dl->AddRectFilled(bMin, bMax, bgCol, 3.f);
                    dl->AddRect(bMin, bMax, IM_COL32(120, 120, 120, 180), 3.f);
                    ImVec2 tsz = ImGui::CalcTextSize(lgLabel);
                    dl->AddText({ bMin.x + (btnSz - tsz.x) * 0.5f,
                                  bMin.y + (btnSz - tsz.y) * 0.5f },
                                IM_COL32(230, 230, 230, 255), lgLabel);
                    if (btnHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        gizmoLocalSpace     = !gizmoLocalSpace;
                        toolbarConsumedClick = true;
                    }
                    if (btnHov)
                        toolbarConsumedClick = true;
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

            // ─── Viewport right-click context menu ───────────────────────────
            if (mouseInVP && ImGui::BeginPopupContextWindow("##viewport_ctx", ImGuiPopupFlags_MouseButtonRight))
            {
                auto target = selNode
                    ? selNode
                    : engineInstance_->mainScene_->root_node_;
                DrawNodeContextMenu(target, true);
                ImGui::EndPopup();
            }

            // ─── Helper: get RigidBodyComponent on the selected node (may be null) ──
            auto getSelRB = [&]() -> RigidBodyComponent* {
                if (selectedNodes_.empty()) return nullptr;
                return selectedNodes_.back()->GetComponent<RigidBodyComponent>();
            };

            // ─── Gizmo drag start ─────────────────────────────────────────────
            if (hovered != AXIS_NONE && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && selNode)
            {
                dragging       = hovered;
                dragStartMouse = mp;
                dragStartPos   = selNode->transform_.getGlobalPosition();
                dragStartRot   = selNode->transform_.getStoredRotation();   // degrees
                dragStartScale = selNode->transform_.getGlobalScale();
                dragStartAngle = atan2f(mp.y - gizmoOrigin.y, mp.x - gizmoOrigin.x);

                // If the node has a rigid body, switch it to kinematic so the
                // physics simulation no longer fights the gizmo.
                if (auto rb = getSelRB()) rb->BeginManipulation();
            }

            // ─── Gizmo drag apply ─────────────────────────────────────────────
            if (dragging != AXIS_NONE && ImGui::IsMouseDown(ImGuiMouseButton_Left) && selNode)
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
                    if (gizmoLocalSpace)
                    {
                        // Project screen delta onto local axes
                        float rotZ = glm::radians(dragStartRot.z);
                        float cosR = cosf(rotZ), sinR = sinf(rotZ);
                        glm::vec2 localXWorld( cosR, sinR);   // local X in world space
                        glm::vec2 localYWorld(-sinR, cosR);   // local Y in world space
                        // Screen delta → world delta: dxPx * wpp along X, -dyPx * wpp along Y
                        glm::vec2 screenDeltaWorld(dxPx * worldPerPixel, -dyPx * worldPerPixel);
                        float projX = glm::dot(screenDeltaWorld, localXWorld);
                        float projY = glm::dot(screenDeltaWorld, localYWorld);
                        if (dragging == AXIS_X || dragging == AXIS_XY) {
                            pos.x += localXWorld.x * projX;
                            pos.y += localXWorld.y * projX;
                        }
                        if (dragging == AXIS_Y || dragging == AXIS_XY) {
                            pos.x += localYWorld.x * projY;
                            pos.y += localYWorld.y * projY;
                        }
                    }
                    else
                    {
                        if (dragging != AXIS_Y) pos.x += dxPx * worldPerPixel;
                        if (dragging != AXIS_X) pos.y -= dyPx * worldPerPixel; // screen-Y is flipped
                    }
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
                selNode->transform_ = newT;

                // Keep the bullet body in sync while in kinematic mode
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
            if (selNode)
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

            // ─── Debug overlay gizmos ─────────────────────────────────────────
            if (showColliderDebug_)  DrawColliderGizmos(imgMin, avail);
            if (showGravityDebug_)   DrawGravityAttractorGizmos(imgMin, avail);
            if (showAudioDebug_)     DrawAudioGizmos(imgMin, avail);
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
        if (auto* rn = node->GetComponent<RenderableNode>())
        {
            if (rn->renderable_ == renderable)
                return node;
        }

        // SoftBodyComponent owns its renderable directly (no RenderableNode wrapper)
        if (auto* sb = node->GetComponent<SoftBodyComponent>())
        {
            if (sb->GetRenderable() == renderable)
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
    // SHARED PLAYBACK TOOLBAR
    // -------------------------------------------------------------------------

    bool DevEditor::DrawPlaybackToolbar()
    {
        bool pressed = false;
        const bool isStopped = (playbackState_ == PlaybackState::Stopped);
        const bool isPlaying = (playbackState_ == PlaybackState::Playing);
        const bool isPaused  = (playbackState_ == PlaybackState::Paused);

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Helper: icon button drawn with ImDrawList shapes
        auto iconBtn = [&](const char* id, ImU32 col, auto drawIcon) -> bool {
            constexpr float SZ = 20.f;
            bool hit = ImGui::InvisibleButton(id, ImVec2(SZ, SZ));
            ImVec2 mn = ImGui::GetItemRectMin();
            ImVec2 mx = ImGui::GetItemRectMax();
            bool hov  = ImGui::IsItemHovered();
            dl->AddRectFilled(mn, mx,
                hov ? IM_COL32(70, 70, 70, 200) : IM_COL32(40, 40, 40, 160), 3.f);
            drawIcon(dl, mn, mx, col);
            return hit;
        };

        // ▶ Play / ⏸ Pause
        if (isStopped || isPaused)
        {
            if (iconBtn("##play", IM_COL32(80, 220, 90, 255),
                [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
                    float cx = (mn.x + mx.x) * 0.5f + 1.f, cy = (mn.y + mx.y) * 0.5f;
                    float h = (mx.y - mn.y) * 0.35f;
                    d->AddTriangleFilled({cx - h*0.6f, cy - h}, {cx + h*0.7f, cy},
                                         {cx - h*0.6f, cy + h}, c);
                }))
            {
                playbackState_ = PlaybackState::Playing;
                engineInstance_->simulationPaused_ = false;
                pressed = true;
            }
        }
        else
        {
            if (iconBtn("##pause", IM_COL32(255, 210, 40, 255),
                [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
                    float x0 = mn.x + 5.f, x1 = mx.x - 5.f;
                    float y0 = mn.y + 4.f,  y1 = mx.y - 4.f;
                    float w  = (x1 - x0) * 0.3f;
                    d->AddRectFilled({x0, y0}, {x0 + w, y1}, c);
                    d->AddRectFilled({x1 - w, y0}, {x1, y1}, c);
                }))
            {
                playbackState_ = PlaybackState::Paused;
                pressed = true;
            }
        }

        ImGui::SameLine();

        // ■ Stop
        {
            const bool canStop = !isStopped;
            if (!canStop) ImGui::BeginDisabled();
            if (iconBtn("##stop", IM_COL32(240, 70, 70, 255),
                [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
                    d->AddRectFilled({mn.x + 5.f, mn.y + 5.f},
                                     {mx.x - 5.f, mx.y - 5.f}, c);
                }))
            {
                ReloadScene();
                pressed = true;
            }
            if (!canStop) ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // ▶| Step
        {
            const bool canStep = isPaused || isStopped;
            if (!canStep) ImGui::BeginDisabled();
            if (iconBtn("##step", IM_COL32(120, 180, 255, 255),
                [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
                    float cx = (mn.x + mx.x) * 0.5f - 1.f, cy = (mn.y + mx.y) * 0.5f;
                    float h = (mx.y - mn.y) * 0.30f;
                    d->AddTriangleFilled({cx - h*0.6f, cy - h}, {cx + h*0.6f, cy},
                                         {cx - h*0.6f, cy + h}, c);
                    d->AddRectFilled({cx + h*0.7f, cy - h}, {cx + h*0.7f + 2.f, cy + h}, c);
                }))
            {
                stepRequested_ = true;
                playbackState_ = PlaybackState::Paused;
                pressed = true;
            }
            if (!canStep) ImGui::EndDisabled();
        }

        return pressed;
    }

    // -------------------------------------------------------------------------
    // COLLIDER WIREFRAME GIZMOS (ImGui overlay — editor only)
    // -------------------------------------------------------------------------

    void DevEditor::DrawColliderGizmos(ImVec2 imgMin, ImVec2 imgSize)
    {
        if (!engineInstance_->mainScene_) return;

        auto& cam = engineInstance_->editorCamera_->editorCameraControl_;
        glm::mat4 view = cam->ComputeViewMatrix(0.f);
        glm::mat4 proj = cam->ComputeProjectionMatrix(0.f);

        auto toScreen = [&](glm::vec3 wp) -> ImVec2 {
            glm::vec4 c = proj * view * glm::vec4(wp, 1.f);
            glm::vec3 n = glm::vec3(c) / c.w;
            return { imgMin.x + (n.x * 0.5f + 0.5f) * imgSize.x,
                     imgMin.y + (1.f - (n.y * 0.5f + 0.5f)) * imgSize.y };
        };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        constexpr ImU32 COL_COLLIDER = IM_COL32(255, 235, 4, 200);

        auto& registry = engineInstance_->mainScene_->registry_;

        // ── Rigid body colliders (box outlines matching real half-extents) ────
        auto rbEntities = registry.View<RigidBodyComponent>();
        for (ecs::Entity e : rbEntities)
        {
            auto* rb = registry.Get<RigidBodyComponent>(e);
            if (!rb || !rb->IsInitialized()) continue;

            glm::vec3 pos = rb->GetPosition();
            glm::quat rot = rb->GetRotation();
            glm::vec3 he  = rb->GetHalfExtents();

            // Compute 4 corners in world space (2D — Z = 0)
            glm::vec3 localCorners[4] = {
                { -he.x, -he.y, 0.f },
                {  he.x, -he.y, 0.f },
                {  he.x,  he.y, 0.f },
                { -he.x,  he.y, 0.f },
            };

            ImVec2 screenPts[4];
            for (int i = 0; i < 4; ++i)
            {
                glm::vec3 world = pos + rot * localCorners[i];
                screenPts[i] = toScreen(world);
            }

            // Draw closed polyline
            for (int i = 0; i < 4; ++i)
                dl->AddLine(screenPts[i], screenPts[(i + 1) % 4], COL_COLLIDER, 1.5f);
        }

        // ── Soft body outlines (trace actual mesh edges) ─────────────────────
        auto sbEntities = registry.View<SoftBodyComponent>();
        for (ecs::Entity e : sbEntities)
        {
            auto* sb = registry.Get<SoftBodyComponent>(e);
            if (!sb || !sb->IsInitialized()) continue;

            const btSoftBody* body = sb->GetBody();
            if (!body) continue;

            // Draw all face edges
            for (int f = 0; f < body->m_faces.size(); ++f)
            {
                const btSoftBody::Face& face = body->m_faces[f];
                for (int j = 0; j < 3; ++j)
                {
                    //TODO: THIS SHOULD BE ABSTRACT AND IMPLENETAITON DETAILS SHOULD BE KEPT ON THE RIGID BODY API IMPL
                    const btVector3& a = face.m_n[j]->m_x;
                    const btVector3& b = face.m_n[(j + 1) % 3]->m_x;
                    ImVec2 sa = toScreen({a.getX(), a.getY(), a.getZ()});
                    ImVec2 sb2 = toScreen({b.getX(), b.getY(), b.getZ()});
                    dl->AddLine(sa, sb2, COL_COLLIDER, 1.0f);
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // GAME VIEW
    // -------------------------------------------------------------------------

    std::shared_ptr<Camera> DevEditor::FindSceneCamera() const
    {
        // Walk scene renderables and return the first Camera that is NOT
        // the editor camera (editor camera is managed by Engine, not the scene).
        for (const auto& r : engineInstance_->renderEngine_.GetRenderables())
        {
            if (r == engineInstance_->editorCamera_) continue;
            if (auto cam = std::dynamic_pointer_cast<Camera>(r))
                return cam;
        }
        return nullptr;
    }

    void DevEditor::ShowGameView()
    {
        ImGui::Begin("Game view");

        // ─── Toolbar: shared playback + resolution + rendering layers ────
        DrawPlaybackToolbar();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Resolution preset
        ImGui::SetNextItemWidth(180.f);
        if (ImGui::BeginCombo("##gv_res", kResolutionPresets[resolutionIndex_].label))
        {
            ImGui::TextDisabled("  Desktop");
            for (int i = 0; i < 5; ++i)
            {
                bool selected = (resolutionIndex_ == i);
                if (ImGui::Selectable(kResolutionPresets[i].label, selected))
                    resolutionIndex_ = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            ImGui::TextDisabled("  Mobile");
            for (int i = 5; i < kNumPresets; ++i)
            {
                bool selected = (resolutionIndex_ == i);
                if (ImGui::Selectable(kResolutionPresets[i].label, selected))
                    resolutionIndex_ = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        // ─── Find scene camera and render into the game-view FBO ─────────
        auto sceneCam = FindSceneCamera();

        if (sceneCam && gameViewFBO_)
        {
            // Resize FBO to match selected resolution preset
            const auto& preset = kResolutionPresets[resolutionIndex_];
            glm::ivec2 targetSize(preset.width, preset.height);
            if (gameViewFBO_->GetSize() != targetSize)
                gameViewFBO_->SetSize(targetSize);

            // Compute the scene camera's projection & view matrices.
            // The camera stores its own ProjectionMatrix and uses its
            // transform for the view when no EditorCamera control is attached.
            glm::mat4 proj = sceneCam->ProjectionMatrix;
            glm::mat4 view = sceneCam->underylingTransform.GetMatrix();

            // If the scene camera happens to have an editor control, use that.
            if (sceneCam->editorCameraControl_)
            {
                proj = sceneCam->editorCameraControl_->ComputeProjectionMatrix(0.f);
                view = sceneCam->editorCameraControl_->ComputeViewMatrix(0.f);
            }

            // Render the scene from the game camera into the game-view FBO
            engineInstance_->renderEngine_.RenderToTarget(
                gameViewFBO_, proj, view, 0.f);

            // ─── Display the game-view FBO ───────────────────────────────
            const float targetAspect = static_cast<float>(preset.width)
                                     / static_cast<float>(preset.height);

            ImVec2 avail = ImGui::GetContentRegionAvail();
            float  viewW, viewH;

            if (avail.x / avail.y > targetAspect)
            { viewH = avail.y; viewW = viewH * targetAspect; }
            else
            { viewW = avail.x; viewH = viewW / targetAspect; }

            // Center with black letterbox bars
            float offsetX = (avail.x - viewW) * 0.5f;
            float offsetY = (avail.y - viewH) * 0.5f;
            ImVec2 cursor = ImGui::GetCursorPos();

            ImVec2 regionMin = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                regionMin, {regionMin.x + avail.x, regionMin.y + avail.y},
                IM_COL32(0, 0, 0, 255));

            ImGui::SetCursorPos(ImVec2(cursor.x + offsetX, cursor.y + offsetY));
            ImGui::Image(reinterpret_cast<void*>(
                static_cast<intptr_t>(gameViewFBO_->GetTextureId())),
                ImVec2(viewW, viewH), ImVec2(0, 1), ImVec2(1, 0));

            // Resolution label overlay
            char resLabel[64];
            snprintf(resLabel, sizeof(resLabel), "%dx%d", preset.width, preset.height);
            ImVec2 imgMin = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(imgMin.x + 6.f, imgMin.y + 4.f),
                IM_COL32(200, 200, 200, 140), resLabel);
        }
        else
        {
            // No scene camera — show helpful message
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* msg = "No scene camera found.\n"
                              "Add a Camera component to a scene node\n"
                              "to see the game preview.";
            ImVec2 textSize = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f,
                                       (avail.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", msg);
        }

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
            const auto pos   = node->transform_.getGlobalPosition();
            const auto rot   = node->transform_.getStoredRotation(); // degrees
            const auto scale = node->transform_.getGlobalScale();
            uiTransform.pos[0]   = pos.x;   uiTransform.pos[1]   = pos.y;   uiTransform.pos[2]   = pos.z;
            uiTransform.rot[0]   = rot.x;   uiTransform.rot[1]   = rot.y;   uiTransform.rot[2]   = rot.z;
            uiTransform.scale[0] = scale.x; uiTransform.scale[1] = scale.y; uiTransform.scale[2] = scale.z;
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

            if (selectedAsset_.type == AssetType::Audio)
            {
                ImGui::Spacing();
                ImGui::TextWrapped("Audio clip — add an Audio Source component to a scene node "
                                   "and set 'Clip Path' to this file's path.");
            }

            if (selectedAsset_.type == AssetType::Material)
            {
                ImGui::Spacing();
                ImGui::TextWrapped("Drag onto a sprite in the viewport or onto the inspector "
                                   "to assign this material.");

                // Try to show material preview (shader + texture info)
                if (assetBuilder_)
                {
                    auto mat = assetBuilder_->LoadMaterial(selectedAsset_.path);
                    if (mat.IsValid())
                    {
                        ImGui::Spacing();
                        ImGui::SeparatorText("Material Properties");
                        ImGui::TextDisabled("Shader");
                        ImGui::SameLine(80);
                        ImGui::Text("%s", mat.shader.c_str());
                        ImGui::TextDisabled("Texture");
                        ImGui::SameLine(80);
                        ImGui::Text("%s", mat.texture.c_str());

                        // Texture preview
                        auto globals = GetDependency(Globals);
                        auto cache = GetDependency(ResourceCache);
                        if (globals && cache && !mat.texture.empty())
                        {
                            std::string absPath = globals->GetWorkingFolder() + mat.texture;
                            GLuint texHandle = cache->GetTexture(absPath);
                            if (texHandle != 0)
                            {
                                float previewSize = ImGui::GetContentRegionAvail().x;
                                ImGui::Image(reinterpret_cast<ImTextureID>(
                                    static_cast<intptr_t>(texHandle)),
                                    ImVec2(previewSize, previewSize));
                            }
                        }
                    }
                }
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
            renderableNode = selectedNode->GetComponent<RenderableNode>();
            rigidBody      = selectedNode->GetComponent<RigidBodyComponent>();
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

        if (transformChanged)
        {
            Transform t;
            t.setGlobalPosition({uiTransform.pos[0],   uiTransform.pos[1],   uiTransform.pos[2]});
            t.setGlobalRotation({uiTransform.rot[0],   uiTransform.rot[1],   uiTransform.rot[2]});
            t.setGlobalScale   ({uiTransform.scale[0], uiTransform.scale[1], uiTransform.scale[2]});
            selectedNode->transform_ = t;

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
                DrawAddComponentMenu(selectedNode);
                ImGui::EndPopup();
            }

            ImGui::Spacing();

            {
                auto& scene = *engineInstance_->mainScene_;
                const ecs::Entity eid = selectedNode->GetId();
                // Copy the type names — removal during iteration would invalidate refs.
                const auto typeNames = scene.registry_.GetComponentTypes(eid);

                std::string pendingRemove; // deferred removal (safe outside loop)

                for (int i = 0; i < (int)typeNames.size(); ++i)
                {
                    const std::string& typeName = typeNames[i];

                    // Channel badge color derived from component type.
                    ImVec4 badgeCol;
                    if      (typeName == RenderableNode::componentType)          badgeCol = {0.35f, 0.75f, 1.f,   1.f}; // blue
                    else if (typeName == AudioSourceComponent::componentType ||
                             typeName == AudioListenerComponent::componentType)  badgeCol = {0.90f, 0.40f, 0.80f, 1.f}; // magenta
                    else                                                         badgeCol = {1.f,   0.75f, 0.2f,  1.f}; // yellow

                    char headerLabel[128];
                    snprintf(headerLabel, sizeof(headerLabel), "%s##comp_%d", typeName.c_str(), i);

                    ImGui::PushStyleColor(ImGuiCol_Text, badgeCol);
                    ImGui::Bullet();
                    ImGui::PopStyleColor();
                    ImGui::SameLine();

                    // Header + remove button on the same line
                    bool open = ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

                    // Draw X button aligned to the right of the header
                    {
                        ImGui::SameLine(ImGui::GetContentRegionMax().x - 20.f);
                        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.1f, 0.1f, 0.8f));
                        char btnId[64];
                        snprintf(btnId, sizeof(btnId), "x##rm_%d", i);
                        if (ImGui::SmallButton(btnId))
                            pendingRemove = typeName;
                        ImGui::PopStyleColor(3);
                    }

                    if (!open) continue;

                    ImGui::PushID(i);
                    ImGui::Indent(12.f);
                    ImGui::Spacing();

                    EditorPropertyVisitor visitor;

                    if (typeName == RenderableNode::componentType)
                    {
                        if (auto* c = selectedNode->GetComponent<RenderableNode>())
                            c->InspectProperties(visitor);
                    }
                    else if (typeName == RigidBodyComponent::componentType)
                    {
                        if (auto* c = selectedNode->GetComponent<RigidBodyComponent>())
                            c->InspectProperties(visitor);
                    }
                    else if (typeName == SoftBodyComponent::componentType)
                    {
                        if (auto* c = selectedNode->GetComponent<SoftBodyComponent>())
                            c->InspectProperties(visitor);
                    }
                    else if (typeName == GravityAttractorComponent::componentType)
                    {
                        if (auto* c = selectedNode->GetComponent<GravityAttractorComponent>())
                            c->InspectProperties(visitor);
                    }
                    else if (typeName == AudioSourceComponent::componentType)
                    {
                        if (auto* c = selectedNode->GetComponent<AudioSourceComponent>())
                            c->InspectProperties(visitor);
                    }
                    else if (typeName == AudioListenerComponent::componentType)
                    {
                        if (auto* c = selectedNode->GetComponent<AudioListenerComponent>())
                            c->InspectProperties(visitor);
                    }
                    else if (typeName == NetworkComponent::componentType)
                    {
                        if (auto* c = selectedNode->GetComponent<NetworkComponent>())
                            c->InspectProperties(visitor);
                    }

                    if (visitor.propertyCount == 0)
                        ImGui::TextDisabled("No exposed properties");

                    ImGui::Spacing();
                    ImGui::Unindent(12.f);
                    ImGui::PopID();
                }

                // Deferred removal — safe to mutate after the loop
                if (!pendingRemove.empty())
                    RemoveComponentByName(selectedNode, pendingRemove);
            }
        }

        // ── Material drag-drop target on the inspector window ────────────────
        // Dropping a material anywhere on the inspector assigns it to the first
        // Sprite renderable on the selected node (Unity-style).
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_ASSET"))
            {
                std::string matPath(static_cast<const char*>(payload->Data),
                                    payload->DataSize - 1);
                if (auto* rn = selectedNode->GetComponent<RenderableNode>())
                {
                    if (auto* spr = dynamic_cast<Sprite*>(rn->renderable_.get()))
                    {
                        spr->SetMaterialPath(matPath);
                        spdlog::info("[DevEditor] Material '{}' assigned to '{}' via inspector",
                                     matPath, selectedNode->GetName());
                    }
                }
            }
            ImGui::EndDragDropTarget();
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

        // Process the "new node" popup modal (triggered by context menu "Empty Node").
        auto parentNode = (!selectedNodes_.empty())
            ? selectedNodes_.back()
            : engineInstance_->mainScene_->root_node_;
        AddNode(parentNode);

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
                const std::string root = engineInstance_->globals_->GetWorkingFolder();
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
                const std::string root = engineInstance_->globals_->GetWorkingFolder();
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

    // ── Centralized Add-Component menu ──────────────────────────────────────
    // Shared popup content drawn by inspector, hierarchy, and viewport.
    void DevEditor::DrawAddComponentMenu(const std::shared_ptr<SceneNode>& node)
    {
        if (!node) return;
        auto& scene = *engineInstance_->mainScene_;
        auto  eid   = node->GetId();

        auto addIfMissing = [&](auto tag, const char* label) {
            using T = std::remove_pointer_t<decltype(tag)>;
            bool has = node->HasComponent<T>();
            if (ImGui::MenuItem(label, nullptr, false, !has))
            {
                node->AddComponent<T>(T{});
                scene.NotifyEntityAdded(eid, *engineInstance_);
            }
        };

        // Renderable (Sprite)
        if (ImGui::MenuItem("Sprite", nullptr, false, !node->HasComponent<RenderableNode>()))
        {
            auto globals = GetDependency(Globals);
            const std::string tex = globals->Get(gk::prefix::SPRITES, gk::key::SPRITE_NOT_FOUND);
            auto sprite = std::make_shared<Sprite>(tex);
            sprite->underylingTransform = node->transform_;
            node->AddComponent<RenderableNode>(RenderableNode(sprite));
            scene.NotifyEntityAdded(eid, *engineInstance_);
        }

        ImGui::Separator();
        addIfMissing(static_cast<RigidBodyComponent*>(nullptr),    "Rigid Body");
        addIfMissing(static_cast<SoftBodyComponent*>(nullptr),     "Soft Body");
        addIfMissing(static_cast<GravityAttractorComponent*>(nullptr), "Gravity Attractor");
        ImGui::Separator();
        addIfMissing(static_cast<AudioSourceComponent*>(nullptr),  "Audio Source");
        addIfMissing(static_cast<AudioListenerComponent*>(nullptr),"Audio Listener");
        ImGui::Separator();
        addIfMissing(static_cast<NetworkComponent*>(nullptr),      "Network");
    }

    // ── Centralized node context menu ────────────────────────────────────────
    static bool showPopup = false;
    void DevEditor::DrawNodeContextMenu(const std::shared_ptr<SceneNode>& node, bool editorExtras)
    {
        if (!node) return;

        if (ImGui::BeginMenu("Add"))
        {
            if (ImGui::MenuItem("Empty Node"))
                showPopup = true;
            ImGui::Separator();
            DrawAddComponentMenu(node);
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Duplicate"))
            DuplicateNode(node);

        bool isRoot = (node == engineInstance_->mainScene_->root_node_);
        if (ImGui::MenuItem("Remove", nullptr, false, !isRoot))
        {
            if (node->parent_)
            {
                CleanupNodeRenderables(node);
                node->parent_->RemoveNode(node->GetId());
                selectedNodes_.clear();
                inspectorSource_ = InspectorSource::None;
            }
        }

        if (editorExtras)
        {
            ImGui::Separator();
            if (ImGui::MenuItem("Reload Scene"))
                ReloadScene();
        }
    }

    // ── Remove component by type name ────────────────────────────────────────
    void DevEditor::RemoveComponentByName(const std::shared_ptr<SceneNode>& node, const std::string& typeName)
    {
        if (!node) return;

        if      (typeName == RenderableNode::componentType)
        {
            // Also remove from the render engine
            auto* rn = node->GetComponent<RenderableNode>();
            if (rn && rn->renderable_)
                engineInstance_->renderEngine_.RemoveRenderable(rn->renderable_);
            node->RemoveComponent<RenderableNode>();
        }
        else if (typeName == RigidBodyComponent::componentType)       node->RemoveComponent<RigidBodyComponent>();
        else if (typeName == SoftBodyComponent::componentType)        node->RemoveComponent<SoftBodyComponent>();
        else if (typeName == GravityAttractorComponent::componentType)node->RemoveComponent<GravityAttractorComponent>();
        else if (typeName == AudioSourceComponent::componentType)     node->RemoveComponent<AudioSourceComponent>();
        else if (typeName == AudioListenerComponent::componentType)   node->RemoveComponent<AudioListenerComponent>();
        else if (typeName == NetworkComponent::componentType)         node->RemoveComponent<NetworkComponent>();
        else spdlog::warn("[DevEditor] Unknown component type to remove: {}", typeName);
    }

    // ── Duplicate node ───────────────────────────────────────────────────────
    void DevEditor::DuplicateNode(const std::shared_ptr<SceneNode>& node)
    {
        if (!node || !node->parent_) return;

        auto& scene = *engineInstance_->mainScene_;
        auto clone = std::make_shared<SceneNode>(node->GetName() + " (copy)");
        clone->transform_ = node->transform_;

        // Clone components by type
        if (auto* rn = node->GetComponent<RenderableNode>())
        {
            if (auto srcSprite = std::dynamic_pointer_cast<Sprite>(rn->renderable_))
            {
                auto newSprite = std::make_shared<Sprite>(srcSprite->GetTexturePath());
                newSprite->SetMaterialPath(srcSprite->GetMaterialPath());
                newSprite->underylingTransform = srcSprite->underylingTransform;
                clone->AddComponent<RenderableNode>(RenderableNode(newSprite));
            }
        }
        if (auto* rb = node->GetComponent<RigidBodyComponent>())
            clone->AddComponent<RigidBodyComponent>(
                RigidBodyComponent{rb->GetMass(), rb->GetHalfExtents(), rb->GetPosition()});
        if (auto* ga = node->GetComponent<GravityAttractorComponent>())
            clone->AddComponent<GravityAttractorComponent>(
                GravityAttractorComponent{ga->GetPosition(), ga->GetStrength()});
        if (node->HasComponent<AudioSourceComponent>())
            clone->AddComponent<AudioSourceComponent>(AudioSourceComponent{});
        if (node->HasComponent<AudioListenerComponent>())
            clone->AddComponent<AudioListenerComponent>(AudioListenerComponent{});

        node->parent_->AddChild(clone);
        spdlog::info("[DevEditor] Duplicated node '{}' -> '{}'", node->GetName(), clone->GetName());
    }

    // ── Recursively remove renderables from the render engine ──────────────
    void DevEditor::CleanupNodeRenderables(const std::shared_ptr<SceneNode>& node)
    {
        if (!node) return;

        if (auto* rn = node->GetComponent<RenderableNode>())
        {
            if (rn->renderable_)
                engineInstance_->renderEngine_.RemoveRenderable(rn->renderable_);
        }

        for (auto& child : node->children_)
            CleanupNodeRenderables(child);
    }

    // ── New scene ─────────────────────────────────────────────────────────────
    void DevEditor::NewScene()
    {
        spdlog::info("[DevEditor] Creating new scene...");

        playbackState_ = PlaybackState::Stopped;
        engineInstance_->simulationPaused_ = true;
        selectedNodes_.clear();
        inspectorSource_ = InspectorSource::None;
        selectedAsset_.active = false;

        engineInstance_->CreateEmptyScene("untitled");
        engineInstance_->InitEditorCamera();

        spdlog::info("[DevEditor] New scene created");
    }

    // ── Reload scene ─────────────────────────────────────────────────────────
    void DevEditor::ReloadScene()
    {
        spdlog::info("[DevEditor] Reloading scene...");

        // Stop simulation
        playbackState_ = PlaybackState::Stopped;
        engineInstance_->simulationPaused_ = true;
        selectedNodes_.clear();

        // Reload from last loaded path, or fall back to default
        auto globals = engineInstance_->globals_;
        auto lastScene = globals->Get(gk::prefix::STATE, gk::key::STATE_LAST_SCENE);
        if (!lastScene.empty())
            engineInstance_->LoadScene(lastScene, false);
        else
            engineInstance_->LoadDefaultScene();

        // Re-init editor camera
        engineInstance_->InitEditorCamera();

        spdlog::info("[DevEditor] Scene reloaded");
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
                node_name[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
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
                auto draggedNode = *reinterpret_cast<const std::shared_ptr<SceneNode>*>(payload->Data);

                // Prevent reparenting onto itself, its current parent, or a descendant.
                bool valid = draggedNode && draggedNode != rootNode && draggedNode->parent_ != rootNode;
                if (valid)
                {
                    // Cycle check: walk up from rootNode to ensure draggedNode isn't an ancestor.
                    auto ancestor = rootNode->parent_;
                    while (ancestor)
                    {
                        if (ancestor == draggedNode) { valid = false; break; }
                        ancestor = ancestor->parent_;
                    }
                }

                if (valid)
                {
                    // Detach from old parent (without destroying registry/ECS data)
                    if (auto oldParent = draggedNode->parent_)
                    {
                        auto& siblings = oldParent->children_;
                        siblings.erase(
                            std::remove(siblings.begin(), siblings.end(), draggedNode),
                            siblings.end());
                    }

                    // Attach to new parent
                    draggedNode->parent_ = rootNode;
                    rootNode->children_.emplace_back(draggedNode);

                    spdlog::info("[DevEditor] Reparented '{}' under '{}'",
                                 draggedNode->GetName(), rootNode->GetName());
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem())
        {
            DrawNodeContextMenu(rootNode);
            ImGui::EndPopup();
        }

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
                // ── Camera properties via component inspector ───────────────
                if (engineInstance->editorCamera_)
                {
                    EditorPropertyVisitor visitor;
                    engineInstance->editorCamera_->Inspect(visitor);
                }

                // ── Framebuffer ─────────────────────────────────────────────
                ImGui::SeparatorText("Framebuffer");
                auto fb = engineInstance->renderEngine_.GetViewPortFrameBuffer();
                if (fb)
                {
                    auto sz = fb->GetSize();
                    ImGui::Text("Size: %d x %d", sz.x, sz.y);
                    ImGui::Text("Texture ID: %u", fb->GetTextureId());
                }

                // ── Object Picker ───────────────────────────────────────────
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

            // ─────────────────────────────────────────────────────────────────
            if (ImGui::BeginTabItem("Threads"))
            {
                auto& td  = engineInstance_->threadDebugInfo_;
                int   off = td.historyOffset;

                const float avail = ImGui::GetContentRegionAvail().x;

                // ── Per-channel stat table ────────────────────────────────────
                ImGui::SeparatorText("Channel Timings");

                struct Row { const char* label; const ChannelSample* s; ImVec4 col; };
                Row rows[] = {
                    { "Physics",     &td.physics,    {0.55f, 0.85f, 0.40f, 1.f} },
                    { "Network",     &td.network,    {0.40f, 0.90f, 0.80f, 1.f} },
                    { "MAIN",        &td.main,       {0.30f, 0.65f, 1.00f, 1.f} },
                    { "RENDERING",   &td.rendering,  {1.00f, 0.55f, 0.20f, 1.f} },
                    { "AUDIO",       &td.audio,      {0.90f, 0.40f, 0.85f, 1.f} },
                };

                if (ImGui::BeginTable("##threadtable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 100.f);
                    ImGui::TableSetupColumn("ms",      ImGuiTableColumnFlags_WidthFixed,  60.f);
                    ImGui::TableSetupColumn("Thread",  ImGuiTableColumnFlags_WidthFixed,  70.f);
                    ImGui::TableSetupColumn("Bar",     ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    // find max for bar scaling
                    float maxMs = 0.1f;
                    for (auto& r : rows) maxMs = std::max(maxMs, r.s->durationMs);

                    for (auto& r : rows)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(r.col, "%s", r.label);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.2f", r.s->durationMs);

                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled(r.s->async ? "worker" : "main");

                        ImGui::TableSetColumnIndex(3);
                        float barW = (r.s->durationMs / maxMs) *
                                     ImGui::GetContentRegionAvail().x;
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            p, { p.x + barW, p.y + 12.f },
                            ImGui::ColorConvertFloat4ToU32(r.col));
                        ImGui::Dummy({ ImGui::GetContentRegionAvail().x, 12.f });
                    }
                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Text("Update phase:  %.2f ms", td.updatePhaseMs);
                ImGui::SameLine(180);
                ImGui::Text("Present phase: %.2f ms", td.presentPhaseMs);
                ImGui::Text("Frame total:   %.2f ms  (%.1f fps)",
                            td.updatePhaseMs + td.presentPhaseMs,
                            td.updatePhaseMs + td.presentPhaseMs > 0.f
                                ? 1000.f / (td.updatePhaseMs + td.presentPhaseMs)
                                : 0.f);

                // ── Timeline (two horizontal rows: main / worker) ─────────────
                ImGui::SeparatorText("Frame Timeline  (this frame)");
                ImGui::TextDisabled("Main:       [Net][  MAIN  ] ... [  RENDERING  ]");
                ImGui::TextDisabled("Pool:                               [ PHYS ][ AUD ]");
                ImGui::TextDisabled("Net worker: [========= polling =========]");
                ImGui::Spacing();

                float total = td.updatePhaseMs + td.presentPhaseMs;
                if (total > 0.f)
                {
                    const float tlW  = avail - 8.f;
                    const float rowH = 18.f;
                    const float gap  = 4.f;

                    ImDrawList* dl   = ImGui::GetWindowDrawList();
                    ImVec2 origin    = ImGui::GetCursorScreenPos();

                    auto drawSegment = [&](float xStart, float dur, ImVec4 col,
                                          const char* lbl, float rowY)
                    {
                        float x0 = origin.x + (xStart / total) * tlW;
                        float x1 = origin.x + ((xStart + dur)  / total) * tlW;
                        if (x1 <= x0 + 1.f) x1 = x0 + 2.f;
                        ImU32 c = ImGui::ColorConvertFloat4ToU32(col);
                        dl->AddRectFilled({x0, rowY}, {x1, rowY + rowH}, c, 3.f);
                        dl->AddRect      ({x0, rowY}, {x1, rowY + rowH},
                                          IM_COL32(0,0,0,120), 3.f);
                        // label if wide enough
                        ImVec2 tsz = ImGui::CalcTextSize(lbl);
                        if (x1 - x0 > tsz.x + 4.f)
                            dl->AddText({x0 + (x1-x0-tsz.x)*0.5f,
                                         rowY + (rowH-tsz.y)*0.5f},
                                        IM_COL32(255,255,255,230), lbl);
                    };

                    float row0 = origin.y;
                    float row1 = origin.y + rowH + gap;

                    float row2 = row1 + rowH + gap;

                    // Main thread row: net-apply | MAIN | RENDERING
                    float netApplyStart = 0.f;
                    float mainStart     = netApplyStart + td.network.durationMs;
                    float renderOff     = td.updatePhaseMs; // rendering starts in PresentFrame
                    drawSegment(netApplyStart, td.network.durationMs,  {0.40f,0.90f,0.80f,0.9f}, "Net",     row0);
                    drawSegment(mainStart,     td.main.durationMs,     {0.30f,0.65f,1.00f,0.9f}, "MAIN",    row0);
                    drawSegment(renderOff,     td.rendering.durationMs,{1.00f,0.55f,0.20f,0.9f}, "REND",    row0);

                    // Worker row 1: physics step overlaps with rendering (pipelined)
                    drawSegment(renderOff,  td.physics.durationMs,    {0.55f,0.85f,0.40f,0.9f}, "PHYS",    row1);
                    // Worker row 1: audio also overlaps
                    drawSegment(renderOff + td.physics.durationMs, td.audio.durationMs, {0.90f,0.40f,0.85f,0.9f}, "AUDIO", row1);

                    // Worker row 2: network worker (persistent, always running)
                    // Show it as a thin continuous bar
                    drawSegment(0.f, total, {0.40f,0.90f,0.80f,0.4f}, "NET POLL", row2);

                    // Labels
                    dl->AddText({origin.x, row0 + rowH + 2.f},  IM_COL32(180,180,180,160), "main");
                    dl->AddText({origin.x, row1 + rowH + 2.f},  IM_COL32(180,180,180,160), "pool");
                    dl->AddText({origin.x, row2 + rowH + 2.f},  IM_COL32(180,180,180,160), "net worker");

                    // Advance cursor past the three rows + labels
                    ImGui::Dummy({tlW, rowH * 3.f + gap * 2.f + 14.f});
                }

                // ── Rolling sparkline histories ───────────────────────────────
                ImGui::SeparatorText("History");

                struct Plot { const char* lbl; const float* data; ImVec4 col; };
                Plot plots[] = {
                    { "MAIN (ms)",      td.mainHistory.data(),      {0.30f, 0.65f, 1.00f, 1.f} },
                    { "AUDIO (ms)",     td.audioHistory.data(),     {0.90f, 0.40f, 0.85f, 1.f} },
                    { "RENDERING (ms)", td.renderingHistory.data(), {1.00f, 0.55f, 0.20f, 1.f} },
                    { "Frame (ms)",     td.frameHistory.data(),     {0.85f, 0.85f, 0.20f, 1.f} },
                };
                for (auto& p : plots)
                {
                    float maxV = 0.1f;
                    for (int i = 0; i < ThreadDebugInfo::kHistorySize; ++i)
                        maxV = std::max(maxV, p.data[i]);
                    char overlay[32];
                    snprintf(overlay, sizeof(overlay), "%.2f ms", p.data[
                        (off + ThreadDebugInfo::kHistorySize - 1) % ThreadDebugInfo::kHistorySize]);
                    ImGui::PushStyleColor(ImGuiCol_PlotLines,
                                          ImGui::ColorConvertFloat4ToU32(p.col));
                    ImGui::PlotLines(p.lbl, p.data, ThreadDebugInfo::kHistorySize,
                                     off, overlay, 0.f, maxV * 1.2f,
                                     ImVec2(avail, 45.f));
                    ImGui::PopStyleColor();
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

        // Re-add editor grid after scene load if needed, and apply visibility
        if (engineInstance_->editorGrid_)
        {
            auto& renderables = engineInstance_->renderEngine_.GetRenderables();
            bool found = std::find(renderables.begin(), renderables.end(),
                                   engineInstance_->editorGrid_) != renderables.end();
            if (!found)
                engineInstance_->renderEngine_.AddRenderable(engineInstance_->editorGrid_);

            engineInstance_->editorGrid_->enabled = gameViewShowGrid_;
        }

        // ── Playback step handling (once per frame) ─────────────────────────
        if (stepRequested_)
        {
            engineInstance_->simulationPaused_ = false;
            stepRequested_ = false;
        }
        else if (playbackState_ == PlaybackState::Paused ||
                 playbackState_ == PlaybackState::Stopped)
        {
            engineInstance_->simulationPaused_ = true;
        }

        ShowMenuBar();
        configurationsWindow_.Draw();
        ShowDockSpace();
        ShowDebugger();
        ShowEditorViewPort();
        ShowGameView();
        ShowInspector();
        ShowSceneHierarchy();
        ShowAssetsView();
        buildPanel_.Draw();
        spriteEditor_.Draw(engineInstance_);
    }

    void DevEditor::Init()
    {
        // ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;


        engineInstance_->InitEditorCamera();
        //TODO: MOVE EVERYTHING BELOW TO THE ENGINE...
        assetLoader_  = std::make_shared<AssetLoader>();
        assetBuilder_ = std::make_shared<AssetBuilder>(assetLoader_);

        pickerBuffer_ = std::make_shared<PickerBuffer>(glm::ivec2(1200, 800));
        pickerBuffer_->Init();

        auto resources   = GetDependency(Globals);
        auto shadersPath = resources->GetWorkingFolder() + resources->Get(gk::prefix::PATHS, gk::key::PATH_SHADERS);
        pickerBuffer_->InitShader(shadersPath);

        // Game-view preview FBO (starts at first resolution preset)
        gameViewFBO_ = std::make_shared<FrameBuffer>(
            glm::ivec2(0, 0),
            glm::ivec2(kResolutionPresets[0].width, kResolutionPresets[0].height),
            false);
        gameViewFBO_->Init();
        gameViewFBOReady_ = true;
    }

    void DevEditor::UpdateUI() { DrawEditor(); }
    void DevEditor::Update()   {}


} // namespace ettycc
