#include <UI/DevEditor.hpp>
#include <glm/glm.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <UI/EditorContextMenu.hpp>
#include <UI/AssetDescriptor.hpp>
#include <imgui_internal.h>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Scene/Components/AudioSourceComponent.hpp>
#include <Scene/Components/AudioListenerComponent.hpp>
#include <Scene/Components/GravityAttractorComponent.hpp>
#include <Scene/Components/CameraControllerComponent.hpp>
#include <Networking/NetworkComponent.hpp>
#include <UI/ComponentRegistry.hpp>
#include <Dependency.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Input/Controls/EditorCamera.hpp>
#include <Scene/Systems/PhysicsSystem.hpp>
#include <Graphics/Rendering/Frustum.hpp>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <portable-file-dialogs.h>
#include <TextEditor.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

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
        // DebugConsole::AddLog only uses malloc/ImVector -- no ImGui context needed.
        auto consoleSink = std::make_shared<ImGuiConsoleSink_mt>(&uiConsole);
        spdlog::default_logger()->sinks().push_back(consoleSink);
    }

    DevEditor::~DevEditor() = default;

    // -------------------------------------------------------------------------
    // ASSET BROWSER HELPERS (delegate to AssetDescriptorRegistry)
    // -------------------------------------------------------------------------

    // Helper: get descriptor or a static fallback for unknown types
    static const AssetDescriptor& GetDescOrFallback(const AssetDescriptorRegistry& reg, AssetType t)
    {
        static const AssetDescriptor kUnknown; // all defaults: "???", "Unknown", gray
        const auto* d = reg.Find(t);
        return d ? *d : kUnknown;
    }

    std::string DevEditor::ComputeRelativePath(const std::string& absolutePath) const
    {
        auto globals = GetDependency(Globals);
        if (!globals) return absolutePath;
        const std::string& workDir = globals->GetWorkingFolder();
        std::string normWork = workDir;
        std::string normPath = absolutePath;
        for (char& c : normWork) if (c == '\\') c = '/';
        for (char& c : normPath) if (c == '\\') c = '/';
        if (normPath.rfind(normWork, 0) == 0)
            return normPath.substr(normWork.size());
        return normPath;
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
                // Skip .meta sidecar files
                if (e.path().extension() == ".meta") continue;
                AssetEntry ae;
                ae.path = e.path().string();
                ae.name = e.path().filename().string();
                ae.type = engineInstance_->assetDescriptors_.Classify(e.path());
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

            const auto& desc = GetDescOrFallback(engineInstance_->assetDescriptors_, entry.type);
            ImVec4 col   = desc.color;
            ImU32  col32 = ImGui::ColorConvertFloat4ToU32(col);
            const char* lbl = desc.label;

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

            // LEFT-CLICK -> select asset for inspector
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

            // DOUBLE-CLICK -> open with default system application
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
#ifdef _WIN32
                std::string winPath = entry.path;
                std::replace(winPath.begin(), winPath.end(), '/', '\\');
                ShellExecuteA(NULL, "open", winPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
            }

            // RIGHT-CLICK -> context menu
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                selectedAsset_.path   = entry.path;
                selectedAsset_.name   = entry.name;
                selectedAsset_.type   = entry.type;
                selectedAsset_.active = true;
                try { selectedAsset_.fileSize = std::filesystem::file_size(entry.path); }
                catch (...) { selectedAsset_.fileSize = 0; }
                ImGui::OpenPopup("##asset_ctx");
            }

            if (ImGui::BeginPopup("##asset_ctx"))
            {
                auto state = BuildContextMenuState(ContextMenuSource::Assets,
                    selectedNodes_.empty() ? nullptr : selectedNodes_.back(), false);
                EditorContextMenu::Draw(state);
                ImGui::EndPopup();
            }

            // Drag source (generic + type-specific payload via descriptor)
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("ASSET_ENTRY",
                                          entry.path.c_str(),
                                          entry.path.size() + 1);

                // Emit extra typed payload if the descriptor defines one
                if (!desc.extraPayloadType.empty() && desc.buildExtraPayloadFn)
                {
                    AssetContext ctx;
                    ctx.engine       = engineInstance_.get();
                    ctx.absolutePath = entry.path;
                    ctx.relativePath = ComputeRelativePath(entry.path);
                    std::string payload = desc.buildExtraPayloadFn(ctx);
                    ImGui::SetDragDropPayload(desc.extraPayloadType.c_str(),
                                              payload.c_str(),
                                              payload.size() + 1);
                }

                ImGui::TextColored(col, "%s", lbl);
                ImGui::SameLine();
                ImGui::Text("%s", entry.name.c_str());
                if (desc.dragTooltip)
                    ImGui::TextDisabled("%s", desc.dragTooltip);
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

            // -- Split left strip  ---------------------------------
            ImGuiID left, rest;
            ImGui::DockBuilderSplitNode(dsId, ImGuiDir_Left, 0.18f, &left, &rest);

            // -- Split right strip --------------------------------
            ImGuiID centre, right;
            ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.24f, &right, &centre);

            // -- Split bottom strip --------------------------
            ImGuiID viewport_row, bottom;
            ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.26f, &bottom, &viewport_row);

            ImGuiID bottomLeft, bottomRight;
            ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.38f, &bottomLeft, &bottomRight);

            // -- Assign windows -----------------------------------------------
            ImGui::DockBuilderDockWindow("Scene Hierarchy", bottomLeft);
            ImGui::DockBuilderDockWindow("Editor view",    viewport_row);
            ImGui::DockBuilderDockWindow("Game view",      viewport_row);
            ImGui::DockBuilderDockWindow("Assets",         bottomRight);
            ImGui::DockBuilderDockWindow("Debug",          bottomRight);
            ImGui::DockBuilderDockWindow("Inspector",      right);
            // ImGui::DockBuilderDockWindow("Build",          right);

            ImGui::DockBuilderFinish(dsId);
        }
    }

    void DevEditor::ShowBuiltInScenes()
    {
        if (ImGui::MenuItem("Empty Scene"))
            engineInstance_->LoadBuiltInScene(SampleScene::Default);

        if (ImGui::MenuItem("Soft Bodies..."))
        {
            builtInConfig_ = {};
            builtInConfig_.scene = SampleScene::SoftBodies;
            builtInPopupOpen_ = true;
        }
        if (ImGui::MenuItem("Networked Multiplayer..."))
        {
            builtInConfig_ = {};
            builtInConfig_.scene = SampleScene::Network;
            builtInPopupOpen_ = true;
        }

        // Module-registered menu items
        auto& moduleItems = engineInstance_->editorExtensions_.MenuItems();
        if (!moduleItems.empty())
        {
            ImGui::Separator();
            for (auto& item : moduleItems)
                item.callback(*engineInstance_);
        }

        ImGui::EndMenu();
    }

    void DevEditor::ShowBuiltInScenePopup()
    {
        if (builtInPopupOpen_)
        {
            ImGui::OpenPopup("##BuiltInSceneCfg");
            builtInPopupOpen_ = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal("##BuiltInSceneCfg", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        const char* title = "Scene Configuration";
        switch (builtInConfig_.scene)
        {
            case SampleScene::SoftBodies: title = "Soft Bodies";           break;
            case SampleScene::Network:    title = "Networked Multiplayer"; break;
            default: break;
        }
        ImGui::TextUnformatted(title);
        ImGui::Separator();
        ImGui::Spacing();

        switch (builtInConfig_.scene)
        {
        case SampleScene::SoftBodies:
        {
            auto& c = builtInConfig_.softBodies;
            ImGui::DragInt("Body Count",         &c.bodyCount, 1, 1, 100);
            ImGui::DragFloat("Gravity",          &c.gravity, 0.1f, -100.0f, 0.0f);
            ImGui::DragFloat("Soft Body Radius", &c.softBodyRadius, 0.01f, 0.1f, 5.0f);
            ImGui::DragFloat("Soft Body Mass",   &c.softBodyMass, 0.1f, 0.1f, 100.0f);
            break;
        }
        case SampleScene::Network:
        {
            auto& c = builtInConfig_.network;
            ImGui::DragInt("Box Pairs",          &c.boxPairs, 1, 1, 50);
            ImGui::DragFloat("Gravity",          &c.gravity, 0.1f, -100.0f, 0.0f);
            break;
        }
        default:
            break;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (ImGui::Button("Create", ImVec2(buttonWidth, 0)))
        {
            engineInstance_->LoadBuiltInScene(builtInConfig_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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
                if (ImGui::MenuItem("Polygon Editor", NULL))
                    polygonEditor_.isOpen = true;
                if (ImGui::BeginMenu("Built-in scenes"))
                    ShowBuiltInScenes();

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

                if (ImGui::MenuItem("Appearance"))
                {
                    showStyleEditor_ = true;
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

            // Inner ring -- full-strength zone (cyan)
            dl->AddCircle(center, innerPx,
                          IM_COL32(0, 200, 255, 200), 64, 1.5f);
            // Outer ring -- max gravitational influence (magenta, faded)
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

            // Inner ring -- full-volume zone (bright green)
            dl->AddCircle(center, minPx,
                          IM_COL32(80, 230, 80, 200), 64, 1.5f);
            // Outer ring -- silence boundary (orange, faded)
            dl->AddCircle(center, maxPx,
                          IM_COL32(255, 140, 40, 130), 64, 1.0f);

            dl->AddText({ center.x + minPx + 4.f, center.y - 8.f },
                        IM_COL32(80, 230, 80, 220), "min");
            dl->AddText({ center.x + maxPx + 4.f, center.y - 8.f },
                        IM_COL32(255, 140, 40, 200), "max");
        }
    }

    void DevEditor::DrawCameraFrustumGizmo(ImVec2 imgMin, ImVec2 imgSize)
    {
        auto sceneCam = FindSceneCamera();
        if (!sceneCam || !sceneCam->editorCameraControl_) return;

        // Editor camera: world -> screen
        auto& edCam = engineInstance_->editorCamera_->editorCameraControl_;
        glm::mat4 edView = edCam->ComputeViewMatrix(0.f);
        glm::mat4 edProj = edCam->ComputeProjectionMatrix(0.f);

        auto toScreen = [&](glm::vec3 wp) -> ImVec2 {
            glm::vec4 c = edProj * edView * glm::vec4(wp, 1.f);
            glm::vec3 n = glm::vec3(c) / c.w;
            return { imgMin.x + (n.x * 0.5f + 0.5f) * imgSize.x,
                     imgMin.y + (1.f - (n.y * 0.5f + 0.5f)) * imgSize.y };
        };

        // Scene camera: compute the 4 corners of its visible rect in world space
        // For ortho: inverse(P*V) * NDC corners gives world positions
        glm::mat4 scProj = sceneCam->editorCameraControl_->ComputeProjectionMatrix(0.f);
        glm::mat4 scView = sceneCam->editorCameraControl_->ComputeViewMatrix(0.f);
        glm::mat4 invPV  = glm::inverse(scProj * scView);

        // NDC corners (z=0 for 2D)
        glm::vec4 ndcCorners[4] = {
            { -1.f, -1.f, 0.f, 1.f },  // bottom-left
            {  1.f, -1.f, 0.f, 1.f },  // bottom-right
            {  1.f,  1.f, 0.f, 1.f },  // top-right
            { -1.f,  1.f, 0.f, 1.f },  // top-left
        };

        ImVec2 screenCorners[4];
        for (int i = 0; i < 4; ++i)
        {
            glm::vec4 wp = invPV * ndcCorners[i];
            wp /= wp.w;
            screenCorners[i] = toScreen(glm::vec3(wp));
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32 borderCol = IM_COL32(255, 200, 50, 200);
        const ImU32 fillCol   = IM_COL32(255, 200, 50, 15);

        // Filled quad + border
        dl->AddQuadFilled(screenCorners[0], screenCorners[1],
                          screenCorners[2], screenCorners[3], fillCol);
        dl->AddQuad(screenCorners[0], screenCorners[1],
                    screenCorners[2], screenCorners[3], borderCol, 1.5f);

        // Label
        dl->AddText({ screenCorners[3].x + 4.f, screenCorners[3].y + 4.f },
                    borderCol, "Game Camera");
    }

    void DevEditor::ShowEditorViewPort()
    {
        ImGui::Begin("Editor view");

        // --- Playback + overlay toggles ---------------------------------
        DrawPlaybackToolbar();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        // -- Gizmo multi-select combo ----------------------------------------
        {
            // Build preview string from active toggles
            struct GizmoEntry {
                const char* label;
                bool*       flag;
                ImVec4      color;
            };
            GizmoEntry gizmos[] = {
                { "Colliders",    &showColliderDebug_,   { 1.00f, 0.92f, 0.02f, 1.f } },
                { "Gravity",      &showGravityDebug_,    { 0.00f, 0.90f, 0.90f, 1.f } },
                { "Audio",        &showAudioDebug_,      { 0.31f, 0.90f, 0.31f, 1.f } },
                { "Trails",       &showTrajectoryDebug_, { 1.00f, 0.50f, 0.20f, 1.f } },
                { "Grid",         &showGridDebug_,       { 0.55f, 0.60f, 0.85f, 1.f } },
                { "Frustum Cull", &showFrustumDebug_,    { 0.85f, 0.45f, 0.45f, 1.f } },
            };
            constexpr int gizmoCount = 6;

            // Count active for preview
            int activeCount = 0;
            std::string preview;
            for (int i = 0; i < gizmoCount; ++i)
            {
                if (*gizmos[i].flag)
                {
                    if (!preview.empty()) preview += ", ";
                    preview += gizmos[i].label;
                    ++activeCount;
                }
            }
            if (activeCount == 0)        preview = "None";
            else if (activeCount == gizmoCount) preview = "All";

            ImGui::SetNextItemWidth(180.f);
            if (ImGui::BeginCombo("##GizmoToggles", preview.c_str()))
            {
                for (int i = 0; i < gizmoCount; ++i)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, gizmos[i].color);
                    ImGui::Selectable(gizmos[i].label, gizmos[i].flag,
                                      ImGuiSelectableFlags_DontClosePopups);
                    ImGui::PopStyleColor();
                }
                ImGui::EndCombo();
            }

            // Sync grid visibility
            gameViewShowGrid_ = showGridDebug_;

            // Frustum cull logic
            auto& edCam = engineInstance_->editorCamera_;
            if (showFrustumDebug_)
            {
                auto sceneCam = FindSceneCamera();
                if (sceneCam && sceneCam->editorCameraControl_)
                {
                    glm::mat4 proj = sceneCam->editorCameraControl_->ComputeProjectionMatrix(0.f);
                    glm::mat4 view = sceneCam->editorCameraControl_->ComputeViewMatrix(0.f);
                    edCam->useFrustumOverride_ = true;
                    edCam->frustumOverride_ = Frustum::FromPV(proj * view);
                }
                else
                {
                    edCam->useFrustumOverride_ = false;
                    edCam->frustumCullingEnabled_ = true;
                }
            }
            else
            {
                edCam->useFrustumOverride_ = false;
                edCam->frustumCullingEnabled_ = false;
            }
        }
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

            // --- Drag-drop onto viewport (uses descriptor registry) --------
            HandleViewportDragDrop();

            // --- Gizmo persistent state ---------------------------------------
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

            // --- Resolve selection --------------------------------------------
            bool hasSelection = !selectedNodes_.empty() && inspectorSource_ == InspectorSource::SceneNode;
            std::shared_ptr<SceneNode> selNode;
            if (hasSelection) selNode = selectedNodes_.back();

            // Compute centroid of all selected nodes (gizmo anchor point)
            glm::vec3 selCenter(0.f);
            if (hasSelection)
            {
                for (auto& n : selectedNodes_)
                    selCenter += n->transform_.getGlobalPosition();
                selCenter /= static_cast<float>(selectedNodes_.size());
            }

            // --- Gizmo hover detection (runs before click tests) --------------
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
            bool   mouseInVP  = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(imgMin, imgMax);

            constexpr float HANDLE_LEN = 60.f;
            constexpr float HIT_R      = 10.f;
            constexpr float RING_R     = 52.f;
            constexpr float SQ_HALF    = 6.f;

            if (hasSelection)
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

                gizmoOrigin = toScreen(selCenter);

                // Compute axis directions in screen space
                // In global mode: world X = right, world Y = up
                // In local mode: axes rotate with the object
                glm::vec2 axisXDir(1.f, 0.f);  // screen-space X direction
                glm::vec2 axisYDir(0.f, -1.f);  // screen-space Y direction (screen Y is flipped)

                if (gizmoLocalSpace && selectedNodes_.size() == 1)
                {
                    float rotZ = glm::radians(selectedNodes_[0]->transform_.getStoredRotation().z);
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

            // --- Gizmo mode toolbar (T/R/S + L/G) -- row 2 ---------------------
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

                // Slice tool button
                {
                    float sliceX = lgX + btnSz + gap * 3;
                    const char* sliceLabel = "/";
                    ImVec2 bMin = { sliceX, row2Y };
                    ImVec2 bMax = { bMin.x + btnSz, bMin.y + btnSz };
                    bool btnHov = ImGui::IsMouseHoveringRect(bMin, bMax);
                    ImU32 bgCol = sliceToolActive_ ? IM_COL32(220, 60, 60, 220) :
                                  btnHov           ? IM_COL32( 80,  80,  80, 200) :
                                                     IM_COL32( 40,  40,  40, 180);
                    dl->AddRectFilled(bMin, bMax, bgCol, 3.f);
                    dl->AddRect(bMin, bMax, IM_COL32(120, 120, 120, 180), 3.f);
                    ImVec2 tsz = ImGui::CalcTextSize(sliceLabel);
                    dl->AddText({ bMin.x + (btnSz - tsz.x) * 0.5f,
                                  bMin.y + (btnSz - tsz.y) * 0.5f },
                                sliceToolActive_ ? IM_COL32(255, 255, 255, 255)
                                                 : IM_COL32(230, 230, 230, 255),
                                sliceLabel);
                    if (btnHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        sliceToolActive_     = !sliceToolActive_;
                        sliceDragging_       = false;
                        toolbarConsumedClick = true;
                    }
                    if (btnHov)
                        toolbarConsumedClick = true;
                }
            }

            // --- Gizmo keyboard shortcuts (Q=Translate, W=Rotate, E=Scale) ------
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_None) && !ImGui::GetIO().WantTextInput)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Q)) gizmoMode = 0;
                if (ImGui::IsKeyPressed(ImGuiKey_W)) gizmoMode = 1;
                if (ImGui::IsKeyPressed(ImGuiKey_E)) gizmoMode = 2;
            }

            // --- Box selection (marquee) update ----------------------------------
            if (viewportBoxSelector_.active)
            {
                bool cancelled = false;
                viewportBoxSelector_.Update(mp, &cancelled);

                if (cancelled)
                {
                    if (!ImGui::GetIO().KeyShift)
                    {
                        selectedNodes_.clear();
                        inspectorSource_ = InspectorSource::None;
                    }
                }
                else
                {
                    // World-to-screen converter
                    auto& cam = engineInstance_->editorCamera_->editorCameraControl_;
                    glm::mat4 view = cam->ComputeViewMatrix(0.f);
                    glm::mat4 proj = cam->ComputeProjectionMatrix(0.f);

                    auto toScreen = [&](glm::vec3 wp) -> ImVec2 {
                        glm::vec4 c = proj * view * glm::vec4(wp, 1.f);
                        glm::vec3 n = glm::vec3(c) / c.w;
                        return { imgMin.x + (n.x * 0.5f + 0.5f) * avail.x,
                                 imgMin.y + (1.f - (n.y * 0.5f + 0.5f)) * avail.y };
                    };

                    bool shift = ImGui::GetIO().KeyShift;
                    if (!shift && !viewportBoxSelector_.deselectMode)
                        selectedNodes_.clear();

                    // Collect all scene nodes and test against the box
                    std::vector<std::shared_ptr<SceneNode>> allNodes;
                    for (auto& child : engineInstance_->mainScene_->root_node_->children_)
                        CollectAllNodes(child, allNodes);

                    for (auto& node : allNodes)
                    {
                        ImVec2 sp = toScreen(node->transform_.getGlobalPosition());
                        bool inside = viewportBoxSelector_.HitTest(sp);
                        bool alreadySelected = std::find(selectedNodes_.begin(),
                            selectedNodes_.end(), node) != selectedNodes_.end();

                        if (inside)
                        {
                            if (viewportBoxSelector_.deselectMode)
                            {
                                if (alreadySelected)
                                    selectedNodes_.erase(std::remove(selectedNodes_.begin(),
                                        selectedNodes_.end(), node), selectedNodes_.end());
                            }
                            else if (!alreadySelected)
                            {
                                selectedNodes_.push_back(node);
                            }
                        }
                    }

                    if (!selectedNodes_.empty())
                    {
                        inspectorSource_      = InspectorSource::SceneNode;
                        selectedAsset_.active = false;
                    }
                }
            }

            // --- Click-to-select via picker ------------------------------------
            // Uses click-pending to distinguish click (select/deselect) from
            // drag (box selection) when clicking empty space.
            static bool  vpClickPending = false;
            static ImVec2 vpClickPendingPos = {};

            if (!viewportBoxSelector_.active && !vpClickPending &&
                !toolbarConsumedClick && !sliceToolActive_ &&
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
                            bool shift = ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl;
                            if (!shift) selectedNodes_.clear();

                            bool alreadySelected = std::find(selectedNodes_.begin(),
                                selectedNodes_.end(), node) != selectedNodes_.end();
                            if (shift && alreadySelected)
                                selectedNodes_.erase(std::remove(selectedNodes_.begin(),
                                    selectedNodes_.end(), node), selectedNodes_.end());
                            else if (!alreadySelected)
                                selectedNodes_.push_back(node);

                            inspectorSource_      = InspectorSource::SceneNode;
                            selectedAsset_.active = false;
                        }
                    }
                }
                else
                {
                    // Clicked empty -- defer: might be a click (deselect) or drag (box select)
                    vpClickPending    = true;
                    vpClickPendingPos = mp;
                }
            }

            // --- Resolve click-pending: drag = box select, release = deselect --
            if (vpClickPending)
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.f))
                {
                    vpClickPending = false;
                    viewportBoxSelector_.Begin(vpClickPendingPos);
                }
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    vpClickPending = false;
                    if (!ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl)
                    {
                        selectedNodes_.clear();
                        selectedAsset_.active = false;
                        inspectorSource_      = InspectorSource::None;
                        StopFollowing();
                    }
                }
            }

            // --- Draw viewport box selection rectangle -------------------------
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                viewportBoxSelector_.Draw(dl);
            }

            // --- Viewport right-click context menu (unified) ----------------
            if (ImGui::BeginPopupContextWindow("##viewport_ctx", ImGuiPopupFlags_MouseButtonRight))
            {
                auto target = selNode
                    ? selNode
                    : engineInstance_->mainScene_->root_node_;
                auto state = BuildContextMenuState(ContextMenuSource::Viewport, target, true);
                EditorContextMenu::Draw(state);
                ImGui::EndPopup();
            }

            // --- Multi-node drag state ---------------------------------------
            struct NodeDragData {
                glm::vec3 pos;
                glm::vec3 rot;   // euler degrees
                glm::vec3 scale;
            };
            static std::vector<NodeDragData> dragStartNodes;
            static glm::vec3 dragStartCenter;

            // --- Gizmo drag start ---------------------------------------------
            if (hovered != AXIS_NONE && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hasSelection)
            {
                dragging       = hovered;
                dragStartMouse = mp;
                dragStartCenter = selCenter;
                dragStartAngle = glm::atan(mp.y - gizmoOrigin.y, mp.x - gizmoOrigin.x);

                // Store initial state for every selected node
                dragStartNodes.clear();
                dragStartNodes.reserve(selectedNodes_.size());
                engineInstance_->DrainPhysicsFuture();
                for (auto& n : selectedNodes_)
                {
                    dragStartNodes.push_back({
                        n->transform_.getGlobalPosition(),
                        n->transform_.getStoredRotation(),
                        n->transform_.getGlobalScale()
                    });
                    if (auto* rb = n->GetComponent<RigidBodyComponent>())
                        rb->BeginManipulation();
                }

                // Keep legacy single-node vars for backward compat
                dragStartPos   = dragStartNodes[0].pos;
                dragStartRot   = dragStartNodes[0].rot;
                dragStartScale = dragStartNodes[0].scale;
            }

            // --- Gizmo drag apply ---------------------------------------------
            if (dragging != AXIS_NONE && ImGui::IsMouseDown(ImGuiMouseButton_Left) && hasSelection)
            {
                engineInstance_->DrainPhysicsFuture();
                float dxPx = mp.x - dragStartMouse.x;
                float dyPx = mp.y - dragStartMouse.y;

                for (size_t i = 0; i < selectedNodes_.size() && i < dragStartNodes.size(); ++i)
                {
                    auto& node = selectedNodes_[i];
                    auto& ds   = dragStartNodes[i];

                    glm::vec3 pos   = ds.pos;
                    glm::quat rot   = glm::quat(glm::radians(ds.rot));
                    glm::vec3 scale = ds.scale;

                    if (dragging == AXIS_X || dragging == AXIS_Y || dragging == AXIS_XY)
                    {
                        if (gizmoLocalSpace && selectedNodes_.size() == 1)
                        {
                            float rotZ = glm::radians(ds.rot.z);
                            float cosR = cosf(rotZ), sinR = sinf(rotZ);
                            glm::vec2 localXWorld( cosR, sinR);
                            glm::vec2 localYWorld(-sinR, cosR);
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
                            if (dragging != AXIS_X) pos.y -= dyPx * worldPerPixel;
                        }
                    }
                    else if (dragging == AXIS_ROTATE)
                    {
                        float curAngle = glm::atan(mp.y - gizmoOrigin.y, mp.x - gizmoOrigin.x);
                        float deltaDeg = glm::degrees(curAngle - dragStartAngle);

                        // Rotate individual orientation
                        glm::vec3 newEuler = ds.rot;
                        newEuler.z -= deltaDeg;
                        rot = glm::quat(glm::radians(newEuler));

                        // Orbit position around the selection center
                        if (selectedNodes_.size() > 1)
                        {
                            float deltaRad = glm::radians(-deltaDeg);
                            glm::vec2 offset(ds.pos.x - dragStartCenter.x,
                                             ds.pos.y - dragStartCenter.y);
                            float cosD = cosf(deltaRad), sinD = sinf(deltaRad);
                            pos.x = dragStartCenter.x + offset.x * cosD - offset.y * sinD;
                            pos.y = dragStartCenter.y + offset.x * sinD + offset.y * cosD;
                        }
                    }
                    else // Scale
                    {
                        constexpr float SENS = 0.012f;
                        if (dragging == AXIS_SX  || dragging == AXIS_SXY)
                            scale.x = glm::max(0.001f, scale.x + dxPx * SENS);
                        if (dragging == AXIS_SY  || dragging == AXIS_SXY)
                            scale.y = glm::max(0.001f, scale.y - dyPx * SENS);

                        // Scale positions relative to center for multi-selection
                        if (selectedNodes_.size() > 1)
                        {
                            float fx = (ds.scale.x > 0.001f) ? scale.x / ds.scale.x : 1.f;
                            float fy = (ds.scale.y > 0.001f) ? scale.y / ds.scale.y : 1.f;
                            pos.x = dragStartCenter.x + (ds.pos.x - dragStartCenter.x) * fx;
                            pos.y = dragStartCenter.y + (ds.pos.y - dragStartCenter.y) * fy;
                        }
                    }

                    Transform newT;
                    newT.SetFromTRS(pos, rot, scale);
                    node->transform_ = newT;

                    if (auto* rb = node->GetComponent<RigidBodyComponent>())
                        rb->SyncFromRenderable();
                }
            }

            // --- Gizmo drag release -------------------------------------------
            if (dragging != AXIS_NONE && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                engineInstance_->DrainPhysicsFuture();
                for (auto& n : selectedNodes_)
                {
                    if (auto* rb = n->GetComponent<RigidBodyComponent>())
                        rb->EndManipulation();
                }
                dragging = AXIS_NONE;
                dragStartNodes.clear();
            }

            // --- Draw gizmo handles -------------------------------------------
            if (hasSelection)
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


            // --- Camera pan / focus -------------------------------------------
            static bool    isViewportFocused = false;
            static ImVec2  lockedCursorPos;

            bool gizmoOccupied = (dragging != AXIS_NONE) || (hovered != AXIS_NONE);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_None))
            {
                // Disable camera pan while a gizmo handle or box selection is active
                engineInstance_->editorCamera_->editorCameraControl_->enabled =
                    !gizmoOccupied && !viewportBoxSelector_.active;

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

            // --- Slice tool interaction ----------------------------------------
            if (sliceToolActive_ && mouseInVP)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                if (!sliceDragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    sliceDragging_    = true;
                    sliceStartScreen_ = mp;
                    sliceEndScreen_   = mp;
                }

                if (sliceDragging_)
                {
                    sliceEndScreen_ = mp;

                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        sliceDragging_ = false;

                        // Convert screen coords to world coords
                        auto& cam = engineInstance_->editorCamera_->editorCameraControl_;
                        glm::mat4 view = cam->ComputeViewMatrix(0.f);
                        glm::mat4 proj = cam->ComputeProjectionMatrix(0.f);
                        glm::mat4 invVP = glm::inverse(proj * view);

                        auto screenToWorld = [&](ImVec2 sp) -> glm::vec2 {
                            float nx = (sp.x - imgMin.x) / avail.x * 2.f - 1.f;
                            float ny = 1.f - (sp.y - imgMin.y) / avail.y * 2.f;
                            glm::vec4 wp = invVP * glm::vec4(nx, ny, 0.f, 1.f);
                            return { wp.x / wp.w, wp.y / wp.w };
                        };

                        glm::vec2 worldStart = screenToWorld(sliceStartScreen_);
                        glm::vec2 worldEnd   = screenToWorld(sliceEndScreen_);
                        glm::vec2 sliceDir   = worldEnd - worldStart;
                        float len = glm::length(sliceDir);

                        if (len > 0.01f)
                        {
                            sliceDir /= len;
                            glm::vec2 midpoint = (worldStart + worldEnd) * 0.5f;

                            // Find the nearest rigid body whose position is close to the slice line
                            auto& rbPool   = engineInstance_->mainScene_->registry_.Pool<RigidBodyComponent>();
                            auto& comps    = rbPool.Components();
                            auto& entities = rbPool.Entities();

                            ecs::Entity bestEntity = ecs::NullEntity;
                            float bestDist = std::numeric_limits<float>::max();
                            glm::vec2 lineNormal = { -sliceDir.y, sliceDir.x };

                            for (size_t i = 0; i < comps.size(); ++i)
                            {
                                auto& rb = comps[i];
                                if (!rb.IsInitialized() || !rb.IsDynamic()) continue;
                                glm::vec3 pos3 = rb.GetPosition();
                                glm::vec2 pos2 = { pos3.x, pos3.y };

                                // Distance from body center to slice line
                                float d = glm::abs(glm::dot(pos2 - worldStart, lineNormal));
                                // Also check body is within the line segment extent
                                float proj = glm::dot(pos2 - worldStart, sliceDir);
                                float halfExt = (rb.GetHalfExtents().x + rb.GetHalfExtents().y) * 0.5f;
                                if (proj > -halfExt && proj < len + halfExt && d < halfExt * 2.f)
                                {
                                    if (d < bestDist) { bestDist = d; bestEntity = entities[i]; }
                                }
                            }

                            if (bestEntity != ecs::NullEntity)
                            {
                                // Find PhysicsSystem and call SliceBody
                                for (auto& sys : engineInstance_->mainScene_->systems_)
                                {
                                    if (auto* ps = dynamic_cast<PhysicsSystem*>(sys.get()))
                                    {
                                        ps->SliceBody(*engineInstance_->mainScene_, bestEntity,
                                                      midpoint, sliceDir, sliceBreakForce_);
                                        // Clear selection if sliced node was selected
                                        selectedNodes_.erase(
                                            std::remove_if(selectedNodes_.begin(), selectedNodes_.end(),
                                                [bestEntity](const std::shared_ptr<SceneNode>& n) {
                                                    return n->GetId() == bestEntity;
                                                }),
                                            selectedNodes_.end());
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                sliceDragging_ = false;
            }

            // --- Slice line gizmo (drawn while dragging) ---------------------
            if (sliceDragging_)
            {
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                float time = static_cast<float>(ImGui::GetTime());

                // Extend the line beyond the drag points for visual effect
                ImVec2 dir = { sliceEndScreen_.x - sliceStartScreen_.x,
                               sliceEndScreen_.y - sliceStartScreen_.y };
                float dlen = glm::sqrt(dir.x * dir.x + dir.y * dir.y);
                if (dlen > 1.f)
                {
                    dir.x /= dlen; dir.y /= dlen;
                    float ext = 2000.f;
                    ImVec2 p0 = { sliceStartScreen_.x - dir.x * ext,
                                  sliceStartScreen_.y - dir.y * ext };
                    ImVec2 p1 = { sliceEndScreen_.x + dir.x * ext,
                                  sliceEndScreen_.y + dir.y * ext };

                    // Animated pulsing glow
                    float pulse = 0.5f + 0.5f * glm::sin(time * 8.f);
                    int alpha = static_cast<int>(120 + 135 * pulse);

                    // Outer glow
                    sdl->AddLine(p0, p1, IM_COL32(255, 60, 60, alpha / 3), 6.f);
                    // Core line
                    sdl->AddLine(p0, p1, IM_COL32(255, 80, 60, alpha), 2.f);
                    // Bright center
                    sdl->AddLine(sliceStartScreen_, sliceEndScreen_,
                                 IM_COL32(255, 200, 150, alpha), 1.5f);

                    // Drag handle dots
                    sdl->AddCircleFilled(sliceStartScreen_, 5.f,
                                         IM_COL32(255, 100, 50, 255));
                    sdl->AddCircleFilled(sliceEndScreen_, 5.f,
                                         IM_COL32(255, 100, 50, 255));
                }
            }

            // --- Debug overlay gizmos -----------------------------------------
            if (showColliderDebug_)    DrawColliderGizmos(imgMin, avail);
            if (showGravityDebug_)     DrawGravityAttractorGizmos(imgMin, avail);
            if (showAudioDebug_)       DrawAudioGizmos(imgMin, avail);
            if (showTrajectoryDebug_)  DrawTrajectoryGizmos(imgMin, avail);
            DrawCameraFrustumGizmo(imgMin, avail);

            // Module-registered gizmos
            for (auto& gizmo : engineInstance_->editorExtensions_.Gizmos())
                if (gizmo.enabled)
                    gizmo.callback(*engineInstance_, imgMin, avail);

            // --- Selection outlines ------------------------------------------
            DrawSelectionOutlines(imgMin, avail);
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
        // RenderableNode (sprites, cameras, ...)
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
    // COLLECT ALL NODES (recursive helper for box selection)
    // -------------------------------------------------------------------------

    void DevEditor::CollectAllNodes(const std::shared_ptr<SceneNode>& node,
                                    std::vector<std::shared_ptr<SceneNode>>& out) const
    {
        out.push_back(node);
        for (const auto& child : node->children_)
            CollectAllNodes(child, out);
    }

    // -------------------------------------------------------------------------
    // SELECTION OUTLINES
    // -------------------------------------------------------------------------

    void DevEditor::DrawSelectionOutlines(ImVec2 imgMin, ImVec2 imgSize)
    {
        if (selectedNodes_.empty() || inspectorSource_ != InspectorSource::SceneNode)
            return;

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
        constexpr ImU32 COL_OUTLINE = IM_COL32(50, 180, 255, 220);
        constexpr ImU32 COL_FILL    = IM_COL32(50, 180, 255, 20);
        constexpr float MIN_HALF    = 0.25f; // fallback half-size for shapeless objects

        for (auto& node : selectedNodes_)
        {
            glm::vec3 pos   = node->transform_.getGlobalPosition();
            glm::vec3 scale = node->transform_.getGlobalScale();
            float rotZ      = node->transform_.getStoredRotation().z;
            glm::quat rot   = glm::angleAxis(glm::radians(rotZ), glm::vec3(0.f, 0.f, 1.f));

            // Try to get actual sprite vertices for per-vertex outline
            auto* rn = engineInstance_->mainScene_
                ? engineInstance_->mainScene_->registry_.Get<RenderableNode>(node->GetId())
                : nullptr;
            auto* sprite = (rn && rn->renderable_)
                ? dynamic_cast<Sprite*>(rn->renderable_.get())
                : nullptr;

            if (sprite && sprite->GetShape().preset == SpriteShape::Preset::Custom
                && sprite->GetShape().vertices.size() >= 3)
            {
                // Per-vertex outline: use actual shape boundary vertices
                const auto& shape = sprite->GetShape();

                // Collect boundary vertices (skip center vertex at index 0 for fan shapes)
                std::vector<ImVec2> screenPoly;
                size_t startIdx = (shape.vertices.size() > 3) ? 1 : 0;
                screenPoly.reserve(shape.vertices.size() - startIdx);

                for (size_t i = startIdx; i < shape.vertices.size(); ++i)
                {
                    glm::vec3 local(shape.vertices[i].position * glm::vec2(scale), 0.f);
                    glm::vec3 world = pos + rot * local;
                    screenPoly.push_back(toScreen(world));
                }

                // Filled tint (convex polygon)
                if (screenPoly.size() >= 3)
                    dl->AddConvexPolyFilled(screenPoly.data(),
                                            static_cast<int>(screenPoly.size()), COL_FILL);

                // Outline edges
                for (size_t i = 0; i < screenPoly.size(); ++i)
                    dl->AddLine(screenPoly[i],
                                screenPoly[(i + 1) % screenPoly.size()], COL_OUTLINE, 2.0f);
            }
            else
            {
                // Fallback: axis-aligned box from half-extents
                float hx = glm::max(glm::abs(scale.x) * 0.5f, MIN_HALF);
                float hy = glm::max(glm::abs(scale.y) * 0.5f, MIN_HALF);

                glm::vec3 localCorners[4] = {
                    { -hx, -hy, 0.f },
                    {  hx, -hy, 0.f },
                    {  hx,  hy, 0.f },
                    { -hx,  hy, 0.f },
                };

                ImVec2 screenPts[4];
                for (int i = 0; i < 4; ++i)
                {
                    glm::vec3 world = pos + rot * localCorners[i];
                    screenPts[i] = toScreen(world);
                }

                // Filled tint
                dl->AddQuadFilled(screenPts[0], screenPts[1], screenPts[2], screenPts[3], COL_FILL);

                // Outline
                for (int i = 0; i < 4; ++i)
                    dl->AddLine(screenPts[i], screenPts[(i + 1) % 4], COL_OUTLINE, 2.0f);
            }
        }
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

        // -- Icon draw helpers ---------------------------------------------

        auto drawPlay = [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
            const float p = 5.f;
            d->AddTriangleFilled(
                {mn.x + p,      mn.y + p},
                {mx.x - p + 1,  (mn.y + mx.y) * 0.5f},
                {mn.x + p,      mx.y - p}, c);
        };

        auto drawPause = [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
            const float p = 5.f, gap = 3.f;
            float midX = (mn.x + mx.x) * 0.5f;
            d->AddRectFilled({midX - gap - 2.f, mn.y + p}, {midX - gap, mx.y - p}, c);
            d->AddRectFilled({midX + gap,       mn.y + p}, {midX + gap + 2.f, mx.y - p}, c);
        };

        auto drawStop = [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
            const float p = 5.f;
            d->AddRectFilled({mn.x + p, mn.y + p}, {mx.x - p, mx.y - p}, c);
        };

        auto drawStep = [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
            const float p = 5.f;
            float midX = (mn.x + mx.x) * 0.5f - 1.f;
            // Small play triangle
            d->AddTriangleFilled(
                {mn.x + p,  mn.y + p},
                {midX + 1,  (mn.y + mx.y) * 0.5f},
                {mn.x + p,  mx.y - p}, c);
            // Vertical bar
            d->AddRectFilled({midX + 3.f, mn.y + p}, {midX + 5.f, mx.y - p}, c);
        };

        auto drawRefresh = [](ImDrawList* d, ImVec2 mn, ImVec2 mx, ImU32 c) {
            float cx = (mn.x + mx.x) * 0.5f;
            float cy = (mn.y + mx.y) * 0.5f;
            float r  = (mx.x - mn.x) * 0.30f;
            // 270-degree arc (from top, clockwise, stopping at left)
            const int segs = 10;
            for (int i = 0; i < segs; ++i)
            {
                float a0 = -1.57f + (i       * 4.71f / segs);
                float a1 = -1.57f + ((i + 1) * 4.71f / segs);
                d->AddLine(
                    {cx + r * cosf(a0), cy + r * sinf(a0)},
                    {cx + r * cosf(a1), cy + r * sinf(a1)}, c, 2.0f);
            }
            // Arrowhead pointing down at the arc endpoint (top-center)
            float tipX = cx, tipY = cy - r;
            d->AddTriangleFilled(
                {tipX - 3.f, tipY - 1.f},
                {tipX + 3.f, tipY - 1.f},
                {tipX,       tipY + 3.5f}, c);
        };

        // -- Play / Pause -------------------------------------------------
        if (isStopped || isPaused)
        {
            if (iconBtn("##play", IM_COL32(80, 220, 90, 255), drawPlay))
            {
                if (isStopped)
                    engineInstance_->BeginPlay();
                else
                    engineInstance_->simulationPaused_ = false;

                playbackState_ = PlaybackState::Playing;
                pressed = true;
            }
        }
        else
        {
            if (iconBtn("##pause", IM_COL32(255, 210, 40, 255), drawPause))
            {
                playbackState_ = PlaybackState::Paused;
                engineInstance_->simulationPaused_ = true;
                pressed = true;
            }
        }

        ImGui::SameLine();

        // -- Stop ---------------------------------------------------------
        {
            const bool canStop = !isStopped;
            if (!canStop) ImGui::BeginDisabled();
            if (iconBtn("##stop", IM_COL32(240, 70, 70, 255), drawStop))
            {
                engineInstance_->EndPlay();
                ReloadScene();
                pressed = true;
            }
            if (!canStop) ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // -- Step ---------------------------------------------------------
        {
            const bool canStep = isPaused || isStopped;
            if (!canStep) ImGui::BeginDisabled();
            if (iconBtn("##step", IM_COL32(120, 180, 255, 255), drawStep))
            {
                if (isStopped)
                    engineInstance_->BeginPlay();

                stepRequested_ = true;
                playbackState_ = PlaybackState::Paused;
                pressed = true;
            }
            if (!canStep) ImGui::EndDisabled();
        }

        // -- Gap between playback controls and build tools ----------------
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 12.0f);

        // -- Refresh (rebuild + hot-reload modules) -----------------------
        {
            const bool building = moduleBuildHelper_.IsRunning();
            if (building) ImGui::BeginDisabled();
            if (iconBtn("##refresh", IM_COL32(180, 140, 255, 255), drawRefresh))
            {
                auto paths = engineInstance_->moduleLoader_.GetModuleSourcePaths();
                moduleBuildHelper_.Start(paths,
                    configurationsWindow_.GetBuildConfig());
                pressed = true;
            }
            if (building) ImGui::EndDisabled();

            // When module build finishes, auto-reload DLLs
            if (moduleBuildHelper_.IsReloadPending())
            {
                moduleBuildHelper_.ConsumeReload();
                engineInstance_->moduleLoader_.ForceReloadAll(engineInstance_.get());
            }

            // Auto-rebuild: if no modules are loaded and we haven't tried yet
            if (!modulesAutoBuilt_ && !building
                && engineInstance_->moduleLoader_.GetModules().empty())
            {
                modulesAutoBuilt_ = true;
                auto paths = engineInstance_->moduleLoader_.GetModuleSourcePaths();
                if (!paths.empty())
                {
                    spdlog::info("[DevEditor] No modules loaded -- triggering auto-rebuild");
                    moduleBuildHelper_.Start(paths,
                        configurationsWindow_.GetBuildConfig());
                }
            }
        }

        return pressed;
    }

    // -------------------------------------------------------------------------
    // COLLIDER WIREFRAME GIZMOS (ImGui overlay -- editor only)
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
        constexpr ImU32 COL_FUSED    = IM_COL32(50, 220, 255, 220);  // cyan for fused celestial bodies

        auto& registry = engineInstance_->mainScene_->registry_;

        // -- Rigid body colliders (box outlines matching real half-extents) ----
        auto rbEntities = registry.View<RigidBodyComponent>();
        for (ecs::Entity e : rbEntities)
        {
            auto* rb = registry.Get<RigidBodyComponent>(e);
            if (!rb || !rb->IsInitialized()) continue;

            glm::vec3 pos = rb->GetPosition();
            glm::quat rot = rb->GetRotation();
            glm::vec3 he  = rb->GetHalfExtents();

            const bool isFused = rb->IsDynamic() && rb->GetMass() > 1.1f;

            // Try to draw actual polygon collider shape from sprite boundary
            auto* rn = registry.Get<RenderableNode>(e);
            Sprite* sprite = nullptr;
            if (rn && rn->renderable_)
                sprite = dynamic_cast<Sprite*>(rn->renderable_.get());

            if (sprite && sprite->GetShape().vertices.size() >= 3)
            {
                const auto& shape = sprite->GetShape();
                // Fan layout: vertex[0] is centroid, boundary starts at [1]
                size_t start = (shape.vertices.size() > 3) ? 1 : 0;
                glm::vec3 scale = sprite->transform().getGlobalScale();
                ImU32 col = isFused ? COL_FUSED : COL_COLLIDER;

                std::vector<ImVec2> screenPoly;
                screenPoly.reserve(shape.vertices.size() - start);
                for (size_t i = start; i < shape.vertices.size(); ++i)
                {
                    glm::vec3 local = { shape.vertices[i].position.x * scale.x,
                                        shape.vertices[i].position.y * scale.y, 0.f };
                    glm::vec3 world = pos + rot * local;
                    screenPoly.push_back(toScreen(world));
                }

                for (size_t i = 0; i < screenPoly.size(); ++i)
                    dl->AddLine(screenPoly[i], screenPoly[(i + 1) % screenPoly.size()], col, 1.5f);

                if (isFused)
                {
                    char massLabel[32];
                    snprintf(massLabel, sizeof(massLabel), "%.0f", rb->GetMass());
                    ImVec2 center = toScreen(pos);
                    dl->AddText({ center.x + 4.f, center.y - 6.f }, COL_FUSED, massLabel);
                }
            }
            else
            {
                // Fallback: box outline from half-extents
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

                for (int i = 0; i < 4; ++i)
                    dl->AddLine(screenPts[i], screenPts[(i + 1) % 4], COL_COLLIDER, 1.5f);
            }
        }

        // -- Soft body outlines (trace actual mesh edges) ---------------------
        auto sbEntities = registry.View<SoftBodyComponent>();
        for (ecs::Entity e : sbEntities)
        {
            auto* sb = registry.Get<SoftBodyComponent>(e);
            if (!sb || !sb->IsInitialized()) continue;

            auto* softBody = sb->GetSoftBody();
            if (!softBody) continue;

            // Draw all face edges via abstract interface
            const int faceCount = softBody->GetFaceCount();
            for (int f = 0; f < faceCount; ++f)
            {
                glm::vec3 fa, fb, fc;
                softBody->GetFaceNodePositions(f, fa, fb, fc);
                const glm::vec3 faceVerts[3] = { fa, fb, fc };
                for (int j = 0; j < 3; ++j)
                {
                    ImVec2 sa = toScreen(faceVerts[j]);
                    ImVec2 sb2 = toScreen(faceVerts[(j + 1) % 3]);
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
        // Editor camera lives in the overlay, not in renderables.
        // All cameras in renderables are scene/game cameras.
        for (const auto& r : engineInstance_->renderEngine_.GetRenderables())
        {
            if (auto cam = std::dynamic_pointer_cast<Camera>(r))
                return cam;
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<Camera>> DevEditor::FindSceneCameras() const
    {
        std::vector<std::shared_ptr<Camera>> cameras;
        for (const auto& r : engineInstance_->renderEngine_.GetRenderables())
        {
            if (!r->enabled) continue;
            if (auto cam = std::dynamic_pointer_cast<Camera>(r))
                cameras.push_back(cam);
        }
        std::sort(cameras.begin(), cameras.end(),
                  [](const auto& a, const auto& b) { return a->depth < b->depth; });
        return cameras;
    }

    void DevEditor::ShowGameView()
    {
        ImGui::Begin("Game view");

        // --- Toolbar: shared playback + resolution + rendering layers ----
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

        // --- Collect scene cameras and render into the game-view FBO -----
        auto sceneCameras = FindSceneCameras(); // this stupid fuck should be slow...

        if (!sceneCameras.empty() && gameViewFBO_)
        {
            // Resize FBO to match selected resolution preset
            const auto& preset = kResolutionPresets[resolutionIndex_];
            glm::ivec2 targetSize(preset.width, preset.height);
            if (gameViewFBO_->GetSize() != targetSize)
                gameViewFBO_->SetSize(targetSize);

            // --- Lazily attach EditorCamera controls to scene cameras ----
            bool isPlaying = playbackState_ == PlaybackState::Playing;
            bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_None);

            for (auto& sceneCam : sceneCameras)
            {
                if (!sceneCam->editorCameraControl_)
                {
                    sceneCam->editorCameraControl_ =
                        std::make_shared<EditorCamera>(
                            &engineInstance_->inputSystem_,
                            gameViewFBO_.get());

                    if (engineInstance_->mainScene_)
                    {
                        auto* rnPool = engineInstance_->mainScene_->registry_
                            .TryGetPool<RenderableNode>();
                        if (rnPool)
                        {
                            for (auto e : rnPool->Entities())
                            {
                                auto* rn = rnPool->Get(e);
                                if (rn && rn->renderable_ == sceneCam)
                                {
                                    auto* node = engineInstance_->mainScene_->GetNode(e);
                                    if (node)
                                        sceneCam->editorCameraControl_->BindTransform(&node->transform_);

                                    auto* ccPool = engineInstance_->mainScene_->registry_
                                        .TryGetPool<CameraControllerComponent>();
                                    if (ccPool)
                                    {
                                        auto* cc = ccPool->Get(e);
                                        if (cc)
                                            sceneCam->editorCameraControl_->BindComponent(cc);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }

                // Only the primary camera (lowest depth) receives input
                bool isPrimary = (sceneCam == sceneCameras.front());
                sceneCam->editorCameraControl_->enabled =
                    isPlaying && isFocused && isPrimary;

                sceneCam->editorCameraControl_->Update(0.f);
            }

            // Render all scene cameras into the game-view FBO
            engineInstance_->renderEngine_.RenderGameView(gameViewFBO_, 0.f);

            // --- Display the game-view FBO -------------------------------
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

            // Resolution label overlay + camera count
            char resLabel[64];
            snprintf(resLabel, sizeof(resLabel), "%dx%d (%d cam%s)",
                     preset.width, preset.height,
                     static_cast<int>(sceneCameras.size()),
                     sceneCameras.size() > 1 ? "s" : "");
            ImVec2 imgMin = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(imgMin.x + 6.f, imgMin.y + 4.f),
                IM_COL32(200, 200, 200, 140), resLabel);
        }
        else
        {
            // No active scene camera
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* msg = "No active game camera.\n"
                              "Enable a Camera component or add one\n"
                              "to a scene node to see the game preview.";
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

    // -------------------------------------------------------------------------
    // ASSET INSPECTOR HELPERS
    // -------------------------------------------------------------------------

    void DevEditor::DrawAssetInspectorHeader()
    {
        const auto& desc = GetDescOrFallback(engineInstance_->assetDescriptors_, selectedAsset_.type);

        // -- Title row with colored type badge --
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertFloat4ToU32(desc.color));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertFloat4ToU32(desc.color));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertFloat4ToU32(desc.color));
        ImGui::SmallButton(desc.label);
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::TextUnformatted(selectedAsset_.name.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // -- Info table --
        const float labelW = 80.f;

        ImGui::TextDisabled("Type");
        ImGui::SameLine(labelW);
        ImGui::TextColored(desc.color, "%s", desc.displayName);

        ImGui::TextDisabled("Size");
        ImGui::SameLine(labelW);
        if (selectedAsset_.fileSize < 1024)
            ImGui::Text("%llu B", static_cast<unsigned long long>(selectedAsset_.fileSize));
        else if (selectedAsset_.fileSize < 1024 * 1024)
            ImGui::Text("%.1f KB", static_cast<double>(selectedAsset_.fileSize) / 1024.0);
        else
            ImGui::Text("%.2f MB", static_cast<double>(selectedAsset_.fileSize) / (1024.0 * 1024.0));

        ImGui::TextDisabled("Path");
        ImGui::SameLine(labelW);
        ImGui::TextWrapped("%s", ComputeRelativePath(selectedAsset_.path).c_str());
    }

    void DevEditor::DrawAssetInspectorActions()
    {
        ImGui::Spacing();
        float btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.f;

        if (ImGui::Button("Copy Path", ImVec2(btnW, 0)))
            ImGui::SetClipboardText(selectedAsset_.path.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Reveal", ImVec2(btnW, 0)))
        {
#ifdef _WIN32
            std::string winPath = selectedAsset_.path;
            std::replace(winPath.begin(), winPath.end(), '/', '\\');
            std::string param = "/select,\"" + winPath + "\"";
            ShellExecuteA(NULL, "open", "explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
#endif
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(btnW, 0)))
            assetsScanned_ = false;
    }

    void DevEditor::DrawAssetInspectorPreview()
    {
        // Delegate to the descriptor's inspect function
        const auto* desc = engineInstance_->assetDescriptors_.Find(selectedAsset_.type);
        if (desc && desc->inspectFn)
        {
            AssetContext ctx;
            ctx.engine       = engineInstance_.get();
            ctx.absolutePath = selectedAsset_.path;
            ctx.relativePath = ComputeRelativePath(selectedAsset_.path);
            ctx.selected     = &selectedAsset_;
            ctx.targetNode   = selectedNodes_.empty() ? nullptr : selectedNodes_.back();
            desc->inspectFn(ctx);
        }
    }

    // -------------------------------------------------------------------------
    // INSPECTOR
    // -------------------------------------------------------------------------

    void DevEditor::ShowInspector()
    {
        ImGui::Begin("Inspector");

        // --- ASSET SELECTED ---
        if (inspectorSource_ == InspectorSource::Asset && selectedAsset_.active)
        {
            DrawAssetInspectorHeader();
            DrawAssetInspectorActions();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            DrawAssetInspectorPreview();

            ImGui::Spacing();
            ImGui::Separator();
            const auto& desc = GetDescOrFallback(engineInstance_->assetDescriptors_, selectedAsset_.type);
            ImGui::TextColored(desc.color, "[ Source: Asset - %s ]", desc.displayName);
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

        // -- Node name ----------------------------------------------------------
        char nameBuf[128] = "";
        {
            std::string name = selectedNode->GetName();
            strncpy(nameBuf, name.empty() ? "UNNAMED" : name.c_str(), sizeof(nameBuf) - 1);
        }
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("##name", nameBuf, IM_ARRAYSIZE(nameBuf)))
            selectedNode->SetName(nameBuf);

        ImGui::Spacing();

        // -- Transform section -------------------------------------------------
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

        // -- Apply transform changes + sync physics -----------------------------
        if (transformActivated && rigidBody)
        {
            engineInstance_->DrainPhysicsFuture();
            rigidBody->BeginManipulation();
        }

        if (transformChanged)
        {
            Transform t;
            t.setGlobalPosition({uiTransform.pos[0],   uiTransform.pos[1],   uiTransform.pos[2]});
            t.setGlobalRotation({uiTransform.rot[0],   uiTransform.rot[1],   uiTransform.rot[2]});
            t.setGlobalScale   ({uiTransform.scale[0], uiTransform.scale[1], uiTransform.scale[2]});
            selectedNode->transform_ = t;

            if (rigidBody && rigidBody->IsManipulated())
            {
                engineInstance_->DrainPhysicsFuture();
                rigidBody->SyncFromRenderable();
            }
        }

        if (transformDeactivated && rigidBody && rigidBody->IsManipulated())
        {
            engineInstance_->DrainPhysicsFuture();
            rigidBody->EndManipulation();
        }

        ImGui::Spacing();

        // -- Components --------------------------------------------------------
        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float avail = ImGui::GetContentRegionAvail().x;
            if (ImGui::Button("Add Component", ImVec2(avail, 0)))
                ImGui::OpenPopup("add_component_popup");

            if (ImGui::BeginPopup("add_component_popup"))
            {
                ContextMenuState acState;
                acState.engine     = engineInstance_.get();
                acState.targetNode = selectedNode;
                EditorContextMenu::DrawAddComponentMenu(acState);
                ImGui::EndPopup();
            }

            ImGui::Spacing();

            {
                auto& scene = *engineInstance_->mainScene_;
                const ecs::Entity eid = selectedNode->GetId();
                // Copy the type names -- removal during iteration would invalidate refs.
                const auto typeNames = scene.registry_.GetComponentTypes(eid);

                std::string pendingRemove; // deferred removal (safe outside loop)

                for (int i = 0; i < (int)typeNames.size(); ++i)
                {
                    const std::string& typeName = typeNames[i];

                    // Badge color from registry; unregistered types are module components.
                    const auto* regEntry = engineInstance_->componentRegistry_.FindByType(typeName);
                    bool isModuleComponent = (regEntry == nullptr);
                    ImVec4 badgeCol = regEntry ? regEntry->badgeColor
                                               : badge::kModule;

                    char headerLabel[128];
                    if (isModuleComponent)
                        snprintf(headerLabel, sizeof(headerLabel), "[MOD] %s##comp_%d", typeName.c_str(), i);
                    else
                        snprintf(headerLabel, sizeof(headerLabel), "%s##comp_%d", typeName.c_str(), i);

                    ImGui::PushStyleColor(ImGuiCol_Text, badgeCol);
                    ImGui::Bullet();
                    ImGui::PopStyleColor();
                    ImGui::SameLine();

                    // Tint the header background for module components
                    if (isModuleComponent)
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.35f, 0.20f, 0.60f));

                    // Header + remove button on the same line
                    bool open = ImGui::CollapsingHeader(headerLabel, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

                    if (isModuleComponent)
                        ImGui::PopStyleColor();

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

                    if (regEntry && regEntry->inspectFn)
                        regEntry->inspectFn(selectedNode, visitor);

                    if (visitor.propertyCount == 0)
                        ImGui::TextDisabled("No exposed properties");

                    ImGui::Spacing();
                    ImGui::Unindent(12.f);
                    ImGui::PopID();
                }

                // Deferred removal -- safe to mutate after the loop
                if (!pendingRemove.empty())
                    RemoveComponentByName(selectedNode, pendingRemove);
            }
        }

        // -- Asset drag-drop target on the inspector (dispatched via descriptors) --
        if (ImGui::BeginDragDropTarget())
        {
            for (const auto& desc : engineInstance_->assetDescriptors_.All())
            {
                if (desc.extraPayloadType.empty() || !desc.inspectorDropFn) continue;
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(desc.extraPayloadType.c_str()))
                {
                    std::string data(static_cast<const char*>(payload->Data),
                                     payload->DataSize - 1);
                    AssetContext ctx;
                    ctx.engine       = engineInstance_.get();
                    ctx.relativePath = data;
                    ctx.targetNode   = selectedNode;
                    desc.inspectorDropFn(ctx);
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

        // Show "Stop Following" banner when actively following a node
        if (isFollowing_)
        {
            auto target = followTarget_.lock();
            std::string label = target
                ? "Following: " + target->GetName()
                : "Following: (lost)";
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", label.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Stop Following"))
                StopFollowing();
            ImGui::Separator();
        }

        RenderSceneTree();

        // Click on empty space in hierarchy -> deselect & stop following
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            selectedNodes_.clear();
            inspectorSource_      = InspectorSource::None;
            selectedAsset_.active = false;
            StopFollowing();
        }

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

    // -- Unified context menu builder -----------------------------------------
    static bool showPopup = false;

    ContextMenuState DevEditor::BuildContextMenuState(
        ContextMenuSource source,
        const std::shared_ptr<SceneNode>& node,
        bool editorExtras)
    {
        ContextMenuState state;
        state.source       = source;
        state.engine       = engineInstance_.get();
        state.targetNode   = node;
        state.isRootNode   = node && (node == engineInstance_->mainScene_->root_node_);
        state.showEditorExtras = editorExtras;
        state.selectedAsset = (selectedAsset_.active) ? &selectedAsset_ : nullptr;

        state.onReloadScene = [this]() { ReloadScene(); };
        state.onNewNode     = []() { showPopup = true; };
        state.onDuplicate   = [this](const std::shared_ptr<SceneNode>& n) { DuplicateNode(n); };
        state.onRemove      = [this](const std::shared_ptr<SceneNode>& n) {
            if (n && n->parent_)
            {
                CleanupNodeRenderables(n);
                n->parent_->RemoveNode(n->GetId());
                selectedNodes_.clear();
                inspectorSource_ = InspectorSource::None;
            }
        };
        state.onFollow = [this](const std::shared_ptr<SceneNode>& n) {
            bool alreadyFollowing = isFollowing_ && followTarget_.lock() == n;
            if (alreadyFollowing)
                StopFollowing();
            else
                StartFollowing(n);
        };
        state.onFocus = [this](const std::shared_ptr<SceneNode>& n) {
            FocusCameraOnNode(n);
        };
        state.isFollowing = [this]() {
            return isFollowing_ && followTarget_.lock() != nullptr;
        };

        return state;
    }

    void DevEditor::DrawContextMenu(const char* popupId, ContextMenuSource source,
                                     const std::shared_ptr<SceneNode>& node,
                                     bool editorExtras)
    {
        if (ImGui::BeginPopup(popupId))
        {
            auto state = BuildContextMenuState(source, node, editorExtras);
            EditorContextMenu::Draw(state);
            ImGui::EndPopup();
        }
    }

    // -- Viewport drag-drop handler (uses descriptor registry) ----------------
    void DevEditor::HandleViewportDragDrop()
    {
        if (!ImGui::BeginDragDropTarget()) return;

        // Generic ASSET_ENTRY payload -> dispatch via descriptor
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ENTRY"))
        {
            std::string path(static_cast<const char*>(payload->Data), payload->DataSize - 1);
            AssetType dropType = engineInstance_->assetDescriptors_.Classify(std::filesystem::path(path));
            const auto* desc = engineInstance_->assetDescriptors_.Find(dropType);

            if (desc && desc->viewportDropFn)
            {
                AssetContext ctx;
                ctx.engine       = engineInstance_.get();
                ctx.absolutePath = path;
                ctx.relativePath = ComputeRelativePath(path);
                ctx.targetNode   = selectedNodes_.empty() ? nullptr : selectedNodes_.back();
                desc->viewportDropFn(ctx);
            }
            else
            {
                const auto& d = GetDescOrFallback(engineInstance_->assetDescriptors_, dropType);
                spdlog::warn("[DevEditor] Drop type '{}' not handled yet", d.displayName);
            }
        }

        // Typed payloads from descriptors (e.g. MATERIAL_ASSET)
        for (const auto& desc : engineInstance_->assetDescriptors_.All())
        {
            if (desc.extraPayloadType.empty()) continue;
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(desc.extraPayloadType.c_str()))
            {
                std::string data(static_cast<const char*>(payload->Data), payload->DataSize - 1);
                if (desc.viewportDropFn)
                {
                    AssetContext ctx;
                    ctx.engine       = engineInstance_.get();
                    ctx.relativePath = data;
                    ctx.targetNode   = selectedNodes_.empty() ? nullptr : selectedNodes_.back();
                    desc.viewportDropFn(ctx);
                }
            }
        }

        // Sprite shape drag-drop from Sprite Editor (non-asset, kept here)
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

        ImGui::EndDragDropTarget();
    }

    // -- Remove component by type name ----------------------------------------
    void DevEditor::RemoveComponentByName(const std::shared_ptr<SceneNode>& node, const std::string& typeName)
    {
        if (!node) return;

        const auto* entry = engineInstance_->componentRegistry_.FindByType(typeName);
        if (entry && entry->removeFn)
            entry->removeFn(node, *engineInstance_);
        else
            spdlog::warn("[DevEditor] Unknown component type to remove: {}", typeName);
    }

    // -- Duplicate node -------------------------------------------------------
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

    // -- Recursively remove renderables from the render engine --------------
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

    // -- New scene -------------------------------------------------------------
    void DevEditor::NewScene()
    {
        spdlog::info("[DevEditor] Creating new scene...");

        playbackState_ = PlaybackState::Stopped;
        engineInstance_->simulationPaused_ = true;
        selectedNodes_.clear();
        inspectorSource_ = InspectorSource::None;
        selectedAsset_.active = false;

        engineInstance_->LoadBuiltInScene();

        // engineInstance_->InitEditorCamera(); // TODO: engine initialization should be keept on the engine class

        spdlog::info("[DevEditor] New scene created");
    }

    // -- Reload scene ---------------------------------------------------------
    void DevEditor::ReloadScene()
    {
        spdlog::info("[DevEditor] Reloading scene...");

        playbackState_ = PlaybackState::Stopped;
        engineInstance_->simulationPaused_ = true;
        selectedNodes_.clear();
        inspectorSource_ = InspectorSource::None;

        // Preserve editor camera state so it doesn't reset on reload
        glm::vec2 savedPos  = {0.f, 0.f};
        float     savedZoom = 1.f;
        if (auto& cam = engineInstance_->editorCamera_) {
            if (cam->editorCameraControl_) {
                savedPos  = cam->editorCameraControl_->GetPosition();
                savedZoom = cam->editorCameraControl_->zoom;
            }
        }

        // Reload from last loaded path, or fall back to default
        auto globals = engineInstance_->globals_;
        auto lastScene = globals->Get(gk::prefix::STATE, gk::key::STATE_LAST_SCENE);
        if (!lastScene.empty())
            engineInstance_->LoadScene(lastScene, false);
        else
            engineInstance_->LoadBuiltInScene();

        // Re-init editor camera and restore previous view
        engineInstance_->InitEditorCamera();

        if (auto& cam = engineInstance_->editorCamera_) {
            if (cam->editorCameraControl_) {
                if (cam->editorCameraControl_->linkedTransform_)
                    cam->editorCameraControl_->linkedTransform_->setGlobalPosition(
                        glm::vec3(savedPos, 0.f));
                cam->editorCameraControl_->zoom = savedZoom;
            }
        }

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

        // Use node pointer as unique ImGui ID so nodes with duplicate names
        // get distinct popups, drag-drop targets, etc.
        ImGui::PushID(rootNode.get());

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
                                     | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                     | ImGuiTreeNodeFlags_SpanAvailWidth;
        // Prevent TreeNode from toggling on double-click so our
        // double/triple-click detection works reliably.
        // We keep OpenOnArrow so the arrow still expands/collapses.
        nodeFlags &= ~ImGuiTreeNodeFlags_OpenOnDoubleClick;
        bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), rootNode) != selectedNodes.end();
        if (isSelected)
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        bool isNodeOpen = ImGui::TreeNodeEx(nodeName, nodeFlags);

        if (ImGui::IsItemClicked(0))
        {
            int clickCount = ImGui::GetMouseClickedCount(ImGuiMouseButton_Left);

            if (clickCount == 1)
            {
                // Single click: select (Shift or Ctrl for multi-select)
                bool multiSelect = ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl;
                if (!multiSelect)
                    selectedNodes.clear();
                if (!isSelected)
                    selectedNodes.push_back(rootNode);
                else if (multiSelect)
                    selectedNodes.erase(std::remove(selectedNodes.begin(), selectedNodes.end(), rootNode),
                                        selectedNodes.end());
            }
            else if (clickCount == 2)
            {
                // Double click: focus camera on object
                selectedNodes.clear();
                selectedNodes.push_back(rootNode);
                FocusCameraOnNode(rootNode);
            }
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
            auto state = BuildContextMenuState(ContextMenuSource::Hierarchy, rootNode, false);
            EditorContextMenu::Draw(state);
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

        ImGui::PopID();
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

    // -- FPS / frame-time ring buffer (per-frame sampling) --------------------
    static constexpr int kFpsHistorySize = 240; // ~4 seconds at 60fps
    static float fpsHistory[kFpsHistorySize]       = {};
    static float frameTimeHistory[kFpsHistorySize] = {};
    static int   fpsHistoryOffset = 0;

    void DevEditor::ShowDebugger()
    {
        auto engineInstance = GetDependency(Engine);
        ImGui::Begin("Debug");

        // Sample every frame
        const float dt  = engineInstance->appInstance_->GetDeltaTime();
        const float fps = (dt > 0.f) ? 1.0f / dt : 0.f;
        fpsHistory[fpsHistoryOffset]       = fps;
        frameTimeHistory[fpsHistoryOffset] = dt * 1000.f; // ms
        fpsHistoryOffset = (fpsHistoryOffset + 1) % kFpsHistorySize;

        if (ImGui::BeginTabBar("tools", ImGuiTabBarFlags_Reorderable))
        {
            if (ImGui::BeginTabItem("Stats"))
            {
                const float avail = ImGui::GetContentRegionAvail().x;

                // -- Compute stats over history --------------------------------
                float minFps = 1e9f, maxFps = 0.f, avgFps = 0.f;
                float minMs  = 1e9f, maxMs  = 0.f, avgMs  = 0.f;
                for (int i = 0; i < kFpsHistorySize; ++i)
                {
                    float f = fpsHistory[i], m = frameTimeHistory[i];
                    if (f > 0.f) { minFps = glm::min(minFps, f); maxFps = glm::max(maxFps, f); avgFps += f; }
                    if (m > 0.f) { minMs  = glm::min(minMs, m);  maxMs  = glm::max(maxMs, m);  avgMs  += m; }
                }
                avgFps /= kFpsHistorySize;
                avgMs  /= kFpsHistorySize;

                // -- Header stats ---------------------------------------------
                ImGui::TextColored({0.4f, 1.f, 0.6f, 1.f}, "%.0f FPS", fps);
                ImGui::SameLine(100);
                ImGui::Text("%.2f ms", dt * 1000.f);
                ImGui::SameLine(200);
                ImGui::TextDisabled("avg %.0f  min %.0f  max %.0f", avgFps, minFps, maxFps);

                // -- FPS graph (prettified: colored PlotLines + overlay) -------
                ImGui::Spacing();
                char fpsOverlay[48];
                snprintf(fpsOverlay, sizeof(fpsOverlay), "%.0f fps", fps);

                ImVec4 fpsColor = (fps >= 55.f)  ? ImVec4(0.3f, 1.f, 0.5f, 1.f) :
                                  (fps >= 30.f)  ? ImVec4(1.f, 0.85f, 0.2f, 1.f) :
                                                   ImVec4(1.f, 0.3f, 0.3f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImGui::ColorConvertFloat4ToU32(fpsColor));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 20, 30, 200));
                ImGui::PlotLines("##fps", fpsHistory, kFpsHistorySize,
                                 fpsHistoryOffset, fpsOverlay,
                                 0.f, maxFps * 1.15f,
                                 ImVec2(avail, 80.f));
                ImGui::PopStyleColor(2);

                // -- Frame time graph -----------------------------------------
                char msOverlay[48];
                snprintf(msOverlay, sizeof(msOverlay), "%.2f ms", dt * 1000.f);

                ImVec4 msColor = (dt * 1000.f <= 18.f) ? ImVec4(0.3f, 0.8f, 1.f, 1.f) :
                                 (dt * 1000.f <= 33.f) ? ImVec4(1.f, 0.85f, 0.2f, 1.f) :
                                                         ImVec4(1.f, 0.3f, 0.3f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImGui::ColorConvertFloat4ToU32(msColor));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 20, 30, 200));
                ImGui::PlotLines("##frametime", frameTimeHistory, kFpsHistorySize,
                                 fpsHistoryOffset, msOverlay,
                                 0.f, maxMs * 1.15f,
                                 ImVec2(avail, 60.f));
                ImGui::PopStyleColor(2);

                ImGui::TextDisabled("Frame time:  avg %.2f ms  min %.2f ms  max %.2f ms",
                                    avgMs, minMs, maxMs);

                // -- SpriteBatch rendering stats ------------------------------
                ImGui::SeparatorText("Rendering");
                const auto& sbStats = engineInstance->renderEngine_.GetSpriteBatchStats();
                ImGui::Text("Draw calls:  %d", sbStats.drawCalls);
                ImGui::SameLine(180);
                ImGui::Text("Sprites:  %d", sbStats.spriteCount);
                ImGui::Text("Vertices:    %d", sbStats.vertexCount);
                ImGui::SameLine(180);
                ImGui::Text("Triangles: %d", sbStats.triangleCount);

                ImGui::Spacing();
                ImGui::Text("CPU build:   %.2f ms", sbStats.cpuBuildMs);
                ImGui::SameLine(180);
                ImGui::Text("CPU upload: %.2f ms", sbStats.cpuUploadMs);
                ImGui::Text("GPU time:    %.2f ms", sbStats.gpuMs);

                // -- Mouse position -------------------------------------------
                ImGui::Spacing();
                auto mousepos = engineInstance->inputSystem_.GetMousePos();
                ImGui::TextDisabled("Mouse: %d, %d", mousepos.x, mousepos.y);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Editor Camera"))
            {
                // -- Camera properties via component inspector ---------------
                if (engineInstance->editorCamera_)
                {
                    EditorPropertyVisitor visitor;
                    engineInstance->editorCamera_->Inspect(visitor);
                }

                // -- Framebuffer ---------------------------------------------
                ImGui::SeparatorText("Framebuffer");
                auto fb = engineInstance->renderEngine_.GetViewPortFrameBuffer();
                if (fb)
                {
                    auto sz = fb->GetSize();
                    ImGui::Text("Size: %d x %d", sz.x, sz.y);
                    ImGui::Text("Texture ID: %u", fb->GetTextureId());
                }

                // -- Object Picker -------------------------------------------
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

                // -- Auto-load on connect ---------------------------------------
                // When state transitions to CONNECTED, load (or reload) the
                // network scene immediately so both peers start in sync.
                if (prevNetState != NetState::CONNECTED && state == NetState::CONNECTED)
                {
                    engineInstance_->NetworkScene();
                    engineInstance_->InitEditorCamera();
                }
                prevNetState = state;

                // -- Status ----------------------------------------------------
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

                // -- Config ---------------------------------------------------
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

                // -- Actions ---------------------------------------------------
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

                // -- Bandwidth & stats -----------------------------------------
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

                    // -- Graphs ------------------------------------------------
                    ImGui::Spacing();
                    float avail = ImGui::GetContentRegionAvail().x;

                    // Find max across both histories for a shared Y scale
                    float maxBw = 1.f; // never show a zero-scale graph
                    for (float v : nm.GetSendHistory()) maxBw = glm::max(maxBw, v);
                    for (float v : nm.GetRecvHistory()) maxBw = glm::max(maxBw, v);

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

                // -- Per-object table ------------------------------------------
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

            // -----------------------------------------------------------------
            if (ImGui::BeginTabItem("Threads"))
            {
                auto& td  = engineInstance_->threadDebugInfo_;
                int   off = td.historyOffset;
                const auto& sbStats = engineInstance->renderEngine_.GetSpriteBatchStats();

                const float avail = ImGui::GetContentRegionAvail().x;

                // -- Per-channel stat table ------------------------------------
                ImGui::SeparatorText("Channel Timings");

                struct Row { const char* label; float ms; bool async; ImVec4 col; };
                Row rows[] = {
                    { "Physics",     td.physics.durationMs,    td.physics.async,    {0.55f, 0.85f, 0.40f, 1.f} },
                    { "Network",     td.network.durationMs,    td.network.async,    {0.40f, 0.90f, 0.80f, 1.f} },
                    { "MAIN",        td.main.durationMs,       td.main.async,       {0.30f, 0.65f, 1.00f, 1.f} },
                    { "RENDERING",   td.rendering.durationMs,  td.rendering.async,  {1.00f, 0.55f, 0.20f, 1.f} },
                    { "  CPU build", sbStats.cpuBuildMs,        false,               {1.00f, 0.70f, 0.35f, 1.f} },
                    { "  CPU upload",sbStats.cpuUploadMs,       false,               {1.00f, 0.80f, 0.50f, 1.f} },
                    { "  GPU",       sbStats.gpuMs,             false,               {0.95f, 0.40f, 0.15f, 1.f} },
                    { "AUDIO",       td.audio.durationMs,      td.audio.async,      {0.90f, 0.40f, 0.85f, 1.f} },
                };

                if (ImGui::BeginTable("##threadtable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 110.f);
                    ImGui::TableSetupColumn("ms",      ImGuiTableColumnFlags_WidthFixed,  65.f);
                    ImGui::TableSetupColumn("Thread",  ImGuiTableColumnFlags_WidthFixed,  55.f);
                    ImGui::TableSetupColumn("Bar",     ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    float maxMs = 0.1f;
                    for (auto& r : rows) maxMs = glm::max(maxMs, r.ms);

                    for (auto& r : rows)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(r.col, "%s", r.label);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.2f", r.ms);

                        ImGui::TableSetColumnIndex(2);
                        if (r.label[0] == ' ')
                            ImGui::TextDisabled("--");
                        else
                            ImGui::TextDisabled(r.async ? "worker" : "main");

                        ImGui::TableSetColumnIndex(3);
                        float barW = glm::max(1.f, (r.ms / maxMs) *
                                     ImGui::GetContentRegionAvail().x);
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImU32 barCol = ImGui::ColorConvertFloat4ToU32(r.col);
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            p, { p.x + barW, p.y + 12.f }, barCol, 2.f);
                        ImGui::Dummy({ ImGui::GetContentRegionAvail().x, 12.f });
                    }
                    ImGui::EndTable();
                }

                // -- Summary line ---------------------------------------------
                ImGui::Spacing();
                float total = td.updatePhaseMs + td.presentPhaseMs;
                ImGui::Text("Update: %.2f ms", td.updatePhaseMs);
                ImGui::SameLine(140);
                ImGui::Text("Present: %.2f ms", td.presentPhaseMs);
                ImGui::SameLine(290);
                ImVec4 totalCol = (total <= 16.7f) ? ImVec4(0.3f, 1.f, 0.5f, 1.f) :
                                  (total <= 33.3f) ? ImVec4(1.f, 0.85f, 0.2f, 1.f) :
                                                     ImVec4(1.f, 0.3f, 0.3f, 1.f);
                ImGui::TextColored(totalCol, "Total: %.2f ms (%.0f fps)",
                                   total, total > 0.f ? 1000.f / total : 0.f);

                // -- Rendering stats ------------------------------------------
                ImGui::SeparatorText("Rendering Stats");
                ImGui::Text("Draw calls: %d", sbStats.drawCalls);
                ImGui::SameLine(140);
                ImGui::Text("Sprites: %d", sbStats.spriteCount);
                ImGui::SameLine(290);
                ImGui::Text("Tris: %d  Verts: %d", sbStats.triangleCount, sbStats.vertexCount);

                // -- Timeline (three horizontal rows: main / pool / net) ------
                ImGui::SeparatorText("Frame Timeline");

                if (total > 0.f)
                {
                    const float tlW  = avail - 8.f;
                    const float rowH = 20.f;
                    const float gap  = 3.f;
                    const float lblW = 55.f;

                    ImDrawList* dl   = ImGui::GetWindowDrawList();
                    ImVec2 origin    = ImGui::GetCursorScreenPos();
                    origin.x += lblW;
                    float barArea = tlW - lblW;

                    auto drawSegment = [&](float xStart, float dur, ImVec4 col,
                                          const char* lbl, float rowY)
                    {
                        float x0 = origin.x + (xStart / total) * barArea;
                        float x1 = origin.x + ((xStart + dur)  / total) * barArea;
                        if (x1 <= x0 + 1.f) x1 = x0 + 2.f;
                        ImU32 c  = ImGui::ColorConvertFloat4ToU32(col);
                        ImU32 bg = ImGui::ColorConvertFloat4ToU32({col.x*0.3f, col.y*0.3f, col.z*0.3f, 0.6f});
                        dl->AddRectFilled({x0, rowY}, {x1, rowY + rowH}, c, 3.f);
                        dl->AddRect      ({x0, rowY}, {x1, rowY + rowH}, IM_COL32(0,0,0,100), 3.f);
                        ImVec2 tsz = ImGui::CalcTextSize(lbl);
                        if (x1 - x0 > tsz.x + 4.f)
                            dl->AddText({x0 + (x1-x0-tsz.x)*0.5f,
                                         rowY + (rowH-tsz.y)*0.5f},
                                        IM_COL32(255,255,255,230), lbl);
                    };

                    float row0 = origin.y;
                    float row1 = row0 + rowH + gap;
                    float row2 = row1 + rowH + gap;
                    float row3 = row2 + rowH + gap;

                    // Row labels
                    dl->AddText({origin.x - lblW, row0 + 3.f}, IM_COL32(180,180,180,200), "main");
                    dl->AddText({origin.x - lblW, row1 + 3.f}, IM_COL32(180,180,180,200), "pool");
                    dl->AddText({origin.x - lblW, row2 + 3.f}, IM_COL32(180,180,180,200), "GPU");
                    dl->AddText({origin.x - lblW, row3 + 3.f}, IM_COL32(180,180,180,200), "net wk");

                    // Row backgrounds
                    ImU32 rowBg = IM_COL32(30, 30, 40, 100);
                    for (float ry : {row0, row1, row2, row3})
                        dl->AddRectFilled({origin.x, ry}, {origin.x + barArea, ry + rowH}, rowBg, 3.f);

                    // Main thread: Net → MAIN → RENDERING
                    float netStart  = 0.f;
                    float mainStart = netStart + td.network.durationMs;
                    float renderOff = td.updatePhaseMs;
                    drawSegment(netStart,   td.network.durationMs,   {0.40f,0.90f,0.80f,0.9f}, "Net",  row0);
                    drawSegment(mainStart,  td.main.durationMs,      {0.30f,0.65f,1.00f,0.9f}, "MAIN", row0);
                    drawSegment(renderOff,  td.rendering.durationMs, {1.00f,0.55f,0.20f,0.9f}, "REND", row0);

                    // Pool: PHYS (overlaps with rendering) + AUDIO
                    drawSegment(renderOff, td.physics.durationMs, {0.55f,0.85f,0.40f,0.9f}, "PHYS", row1);
                    drawSegment(renderOff + td.physics.durationMs, td.audio.durationMs,
                                {0.90f,0.40f,0.85f,0.9f}, "AUDIO", row1);

                    // GPU row: SpriteBatch GPU time
                    if (sbStats.gpuMs > 0.f)
                        drawSegment(renderOff, sbStats.gpuMs, {0.95f,0.40f,0.15f,0.9f}, "GPU", row2);

                    // Net worker: continuous polling
                    drawSegment(0.f, total, {0.40f,0.90f,0.80f,0.35f}, "NET POLL", row3);

                    ImGui::Dummy({tlW, rowH * 4.f + gap * 3.f + 4.f});
                }

                // -- Rolling sparkline histories -------------------------------
                ImGui::SeparatorText("History");

                struct Plot { const char* lbl; const float* data; ImVec4 col; };
                Plot plots[] = {
                    { "MAIN (ms)",      td.mainHistory.data(),      {0.30f, 0.65f, 1.00f, 1.f} },
                    { "RENDERING (ms)", td.renderingHistory.data(), {1.00f, 0.55f, 0.20f, 1.f} },
                    { "AUDIO (ms)",     td.audioHistory.data(),     {0.90f, 0.40f, 0.85f, 1.f} },
                    { "Frame (ms)",     td.frameHistory.data(),     {0.85f, 0.85f, 0.20f, 1.f} },
                };
                for (auto& p : plots)
                {
                    float maxV = 0.1f;
                    for (int i = 0; i < ThreadDebugInfo::kHistorySize; ++i)
                        maxV = glm::max(maxV, p.data[i]);
                    char overlay[32];
                    int lastIdx = (off + ThreadDebugInfo::kHistorySize - 1) % ThreadDebugInfo::kHistorySize;
                    snprintf(overlay, sizeof(overlay), "%.2f ms", p.data[lastIdx]);
                    ImGui::PushStyleColor(ImGuiCol_PlotLines,
                                          ImGui::ColorConvertFloat4ToU32(p.col));
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 20, 30, 200));
                    ImGui::PlotLines(p.lbl, p.data, ThreadDebugInfo::kHistorySize,
                                     off, overlay, 0.f, maxV * 1.2f,
                                     ImVec2(avail, 40.f));
                    ImGui::PopStyleColor(2);
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

        // Apply grid visibility (grid lives in editor overlay, not renderables)
        if (engineInstance_->editorGrid_)
            engineInstance_->editorGrid_->enabled = gameViewShowGrid_;

        // -- Playback step handling (once per frame) -------------------------
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
        ShowBuiltInScenePopup();

        // Module-registered popups/windows
        for (auto& popup : engineInstance_->editorExtensions_.Popups())
            popup.callback(*engineInstance_);

        configurationsWindow_.Draw();
        ShowDockSpace();
        ShowDebugger();
        ShowEditorViewPort();
        ShowGameView();
        ShowInspector();
        ShowSceneHierarchy();
        ShowAssetsView();
        buildPanel_.Draw();
        polygonEditor_.Draw(engineInstance_);


        if (showStyleEditor_)
        {
            ImGui::Begin("Style Editor", &showStyleEditor_);
            ImGui::ShowStyleEditor();
            ImGui::End();
        }
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

        // -- Register built-in components -------------------------------------
        auto& reg = engineInstance_->componentRegistry_;
        if (!reg.FindByType(RenderableNode::componentType))
        {
            // Shared lambdas for Renderable-based components (Sprite & Camera)
            auto renderableHas = [](const std::shared_ptr<SceneNode>& n) {
                return n->HasComponent<RenderableNode>();
            };
            auto renderableRemove = [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                auto* rn = n->GetComponent<RenderableNode>();
                if (rn && rn->renderable_)
                    eng.renderEngine_.RemoveRenderable(rn->renderable_);
                n->RemoveComponent<RenderableNode>();
            };
            auto renderableInspect = [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                if (auto* c = n->GetComponent<RenderableNode>()) c->InspectProperties(v);
            };

            // Sprite
            reg.Register({
                RenderableNode::componentType, "Sprite", "Rendering",
                badge::kRendering,
                renderableHas,
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    const std::string tex = eng.assetRegistry_->GetSpritePath("not_found_texture");
                    auto sprite = std::make_shared<Sprite>(tex);
                    sprite->underylingTransform = n->transform_;
                    n->AddComponent<RenderableNode>(RenderableNode(sprite));
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                renderableRemove,
                renderableInspect
            });

            // Camera
            reg.Register({
                "Camera", "Camera", "Rendering",
                badge::kRendering,
                renderableHas,
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    auto sz = eng.appInstance_->GetMainWindowSize();
                    auto cam = std::make_shared<Camera>(sz.x, sz.y);
                    cam->underylingTransform = n->transform_;
                    cam->underylingTransform.setGlobalPosition({0.0f, 0.0f, -1.0f});
                    n->AddComponent<RenderableNode>(RenderableNode(cam));
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                renderableRemove,
                renderableInspect
            });

            // Camera Controller
            reg.Register({
                CameraControllerComponent::componentType, "Camera Controller", "Input",
                badge::kInput,
                [](const std::shared_ptr<SceneNode>& n) { return n->HasComponent<CameraControllerComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    n->AddComponent<CameraControllerComponent>(CameraControllerComponent{});
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                [](const std::shared_ptr<SceneNode>& n, Engine&) { n->RemoveComponent<CameraControllerComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                    if (auto* c = n->GetComponent<CameraControllerComponent>()) c->InspectProperties(v);
                }
            });

            // Physics
            reg.Register({
                RigidBodyComponent::componentType, "Rigid Body", "Physics",
                badge::kPhysics,
                [](const std::shared_ptr<SceneNode>& n) { return n->HasComponent<RigidBodyComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    n->AddComponent<RigidBodyComponent>(RigidBodyComponent{});
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                [](const std::shared_ptr<SceneNode>& n, Engine&) { n->RemoveComponent<RigidBodyComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                    if (auto* c = n->GetComponent<RigidBodyComponent>()) c->InspectProperties(v);
                }
            });

            reg.Register({
                SoftBodyComponent::componentType, "Soft Body", "Physics",
                badge::kPhysics,
                [](const std::shared_ptr<SceneNode>& n) { return n->HasComponent<SoftBodyComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    n->AddComponent<SoftBodyComponent>(SoftBodyComponent{});
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                [](const std::shared_ptr<SceneNode>& n, Engine&) { n->RemoveComponent<SoftBodyComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                    if (auto* c = n->GetComponent<SoftBodyComponent>()) c->InspectProperties(v);
                }
            });

            reg.Register({
                GravityAttractorComponent::componentType, "Gravity Attractor", "Physics",
                badge::kPhysics,
                [](const std::shared_ptr<SceneNode>& n) { return n->HasComponent<GravityAttractorComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    n->AddComponent<GravityAttractorComponent>(GravityAttractorComponent{});
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                [](const std::shared_ptr<SceneNode>& n, Engine&) { n->RemoveComponent<GravityAttractorComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                    if (auto* c = n->GetComponent<GravityAttractorComponent>()) c->InspectProperties(v);
                }
            });

            // Audio
            reg.Register({
                AudioSourceComponent::componentType, "Audio Source", "Audio",
                badge::kAudio,
                [](const std::shared_ptr<SceneNode>& n) { return n->HasComponent<AudioSourceComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    n->AddComponent<AudioSourceComponent>(AudioSourceComponent{});
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                [](const std::shared_ptr<SceneNode>& n, Engine&) { n->RemoveComponent<AudioSourceComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                    if (auto* c = n->GetComponent<AudioSourceComponent>()) c->InspectProperties(v);
                }
            });

            reg.Register({
                AudioListenerComponent::componentType, "Audio Listener", "Audio",
                badge::kAudio,
                [](const std::shared_ptr<SceneNode>& n) { return n->HasComponent<AudioListenerComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    n->AddComponent<AudioListenerComponent>(AudioListenerComponent{});
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                [](const std::shared_ptr<SceneNode>& n, Engine&) { n->RemoveComponent<AudioListenerComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                    if (auto* c = n->GetComponent<AudioListenerComponent>()) c->InspectProperties(v);
                }
            });

            // Networking
            reg.Register({
                NetworkComponent::componentType, "Network", "Networking",
                badge::kNetwork,
                [](const std::shared_ptr<SceneNode>& n) { return n->HasComponent<NetworkComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, Engine& eng) {
                    n->AddComponent<NetworkComponent>(NetworkComponent{});
                    eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
                },
                [](const std::shared_ptr<SceneNode>& n, Engine&) { n->RemoveComponent<NetworkComponent>(); },
                [](const std::shared_ptr<SceneNode>& n, EditorPropertyVisitor& v) {
                    if (auto* c = n->GetComponent<NetworkComponent>()) c->InspectProperties(v);
                }
            });
        }

        // -- Register Asset Descriptors ----------------------------------------
        auto& ad = engineInstance_->assetDescriptors_;
        auto abRef = assetBuilder_;  // capture shared_ptr for lambdas

        // -- Source code preview helper (shared by Code & Shader & Config) ------
        // Uses ImGuiColorTextEdit for syntax-highlighted, read-only preview.
        auto sourcePreviewFn = [](const AssetContext& ctx) {
            static TextEditor editor;
            static std::string cachedPath;
            static bool initialized = false;

            if (!initialized)
            {
                editor.SetReadOnlyEnabled(true);
                editor.SetShowWhitespacesEnabled(false);
                editor.SetShowScrollbarMiniMapEnabled(false);
                editor.SetShowMiniMapEnabled(false);
                editor.SetLanguage(TextEditor::Language::Cpp());
                editor.SetText("");
                initialized = true;
            }

            if (ctx.absolutePath.empty()) return;

            if (cachedPath != ctx.absolutePath)
            {
                cachedPath = ctx.absolutePath;

                // Read via the asset system (avoids raw ifstream in UI code)
                std::string content = AssetRegistry::ReadFile(ctx.absolutePath);
                if (content.empty())
                    content = "// unable to read file";

                // Cap preview size to avoid heavy rendering
                constexpr size_t kMaxBytes = 32 * 1024;
                if (content.size() > kMaxBytes)
                {
                    content.resize(kMaxBytes);
                    content += "\n\n... (truncated)\n";
                }

                // Detect language from extension
                std::string ext = std::filesystem::path(ctx.absolutePath).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                const TextEditor::Language* lang = TextEditor::Language::Cpp(); // safe default
                if (ext == ".c")
                    lang = TextEditor::Language::C();
                else if (ext == ".cs")
                    lang = TextEditor::Language::Cs();
                else if (ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".geom" || ext == ".comp")
                    lang = TextEditor::Language::Glsl();
                else if (ext == ".hlsl" || ext == ".fx")
                    lang = TextEditor::Language::Hlsl();
                else if (ext == ".json" || ext == ".csv")
                    lang = TextEditor::Language::Json();
                else if (ext == ".lua")
                    lang = TextEditor::Language::Lua();
                else if (ext == ".py")
                    lang = TextEditor::Language::Python();
                else if (ext == ".sql")
                    lang = TextEditor::Language::Sql();
                else if (ext == ".md" || ext == ".markdown")
                    lang = TextEditor::Language::Markdown();

                editor.SetLanguage(lang);
                editor.SetText(content);
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Source Preview");

            float availH = ImGui::GetContentRegionAvail().y - 30.f;
            if (availH < 100.f) availH = 200.f;

            // Render the editor widget, then immediately release keyboard
            // focus so it doesn't steal input from the rest of the UI.
            editor.Render("##src_preview", ImVec2(0, availH));
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            {
                auto& io = ImGui::GetIO();
                io.WantCaptureKeyboard = false;
                io.WantTextInput       = false;
            }
        };

        // Template (.json in templates/)
        ad.Register(
            AssetDescriptor::Create(AssetType::Template, "TPL", "Prefab Template",
                                     {0.27f, 0.51f, 0.71f, 1.f})
            .Classify([](const std::filesystem::path& p) {
                std::string ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                return ext == ".json" && p.string().find("templates") != std::string::npos;
            })
            .Inspect([](const AssetContext& ctx) {
                ImGui::Spacing();
                ImGui::TextWrapped("Drag onto the viewport to spawn this prefab into the scene.");
            })
            .OnViewportDrop([abRef](const AssetContext& ctx) -> bool {
                if (!abRef || !ctx.engine) return false;
                for (auto& node : abRef->BuildFromTemplate(ctx.absolutePath))
                {
                    ctx.engine->mainScene_->root_node_->AddChild(node);
                    spdlog::info("[DevEditor] Spawned '{}' from prefab '{}'",
                                 node->GetName(), ctx.absolutePath);
                }
                return true;
            })
            .DragTip("Drop onto viewport to spawn")
        );

        // Scene (.json in scenes/)
        ad.Register(
            AssetDescriptor::Create(AssetType::Scene, "SCN", "Scene",
                                     {0.24f, 0.70f, 0.44f, 1.f})
            .Classify([](const std::filesystem::path& p) {
                std::string ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                return ext == ".json" && p.string().find("scenes") != std::string::npos;
            })
            .Inspect([](const AssetContext& ctx) {
                ImGui::Spacing();
                if (ImGui::Button("Load Scene", ImVec2(-1, 0)))
                {
                    ctx.engine->LoadScene(ctx.absolutePath, false);
                    spdlog::info("[DevEditor] Loaded scene '{}'", ctx.absolutePath);
                }
            })
            .ContextMenu([](const AssetContext& ctx) {
                if (ImGui::MenuItem("Load Scene"))
                    ctx.engine->LoadScene(ctx.absolutePath, false);
            })
        );

        // Config (.json fallback, .csv, .xml, .yaml, .toml, .ini, .txt)
        ad.Register(
            AssetDescriptor::Create(AssetType::Config, "CFG", "Config",
                                     {0.63f, 0.63f, 0.63f, 1.f},
                                     {".json", ".csv", ".xml", ".yaml", ".yml",
                                      ".toml", ".ini", ".cfg", ".txt"})
            .Inspect(sourcePreviewFn)
        );

        // Code (.cpp, .hpp, .h, .cc, .c, .cxx, .cs, .py, .lua)
        ad.Register(
            AssetDescriptor::Create(AssetType::Code, "SRC", "Source Code",
                                     {0.90f, 0.55f, 0.10f, 1.f},
                                     {".cpp", ".hpp", ".h", ".cc", ".c", ".cxx",
                                      ".inl", ".cs", ".py", ".lua"})
            .Inspect(sourcePreviewFn)
        );

        // Shader (.glsl, .vert, .frag, .hlsl, etc.)
        ad.Register(
            AssetDescriptor::Create(AssetType::Shader, "SHD", "Shader",
                                     {0.58f, 0.44f, 0.86f, 1.f},
                                     {".glsl", ".vert", ".frag", ".geom", ".comp",
                                      ".hlsl", ".fx", ".cg"})
            .Inspect(sourcePreviewFn)
        );

        // Image (.png, .jpg, etc.)
        ad.Register(
            AssetDescriptor::Create(AssetType::Image, "IMG", "Image",
                                     {0.13f, 0.70f, 0.67f, 1.f},
                                     {".png", ".jpg", ".jpeg", ".bmp", ".tga"})
            .Inspect([](const AssetContext& ctx) {
                auto* registry = ctx.engine->assetRegistry_.get();
                if (!registry) return;

                auto texHandle = registry->GetTexture(ctx.relativePath);
                GLuint glTex   = registry->GetGLTextureHandle(texHandle);
                if (glTex != 0)
                {
                    ImGui::Spacing();
                    float previewSize = glm::min(ImGui::GetContentRegionAvail().x, 256.f);
                    ImGui::Image(reinterpret_cast<ImTextureID>(
                        static_cast<intptr_t>(glTex)),
                        ImVec2(previewSize, previewSize));
                }

                TextureAsset* tex = registry->Textures().Get(texHandle);
                if (tex)
                {
                    ImGui::Spacing();
                    ImGui::SeparatorText("Image Settings");
                    const float labelW = 100.f;

                    ImGui::TextDisabled("Dimensions");
                    ImGui::SameLine(labelW);
                    ImGui::Text("%d x %d", tex->sourceWidth, tex->sourceHeight);

                    {
                        static const char* filterLabels[] = { "Point (Pixel Art)", "Bilinear", "Trilinear" };
                        int idx = static_cast<int>(tex->filterMode);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Filter Mode");
                        ImGui::SameLine(labelW);
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("##FilterMode", &idx, filterLabels, 3))
                        {
                            tex->filterMode = static_cast<TextureFilterMode>(idx);
                            registry->SaveTextureMeta(texHandle);
                            registry->ReapplyTextureParams(texHandle);
                        }
                    }
                    {
                        static const char* wrapLabels[] = { "Repeat", "Clamp", "Mirror Repeat" };
                        int idx = static_cast<int>(tex->wrapMode);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Wrap Mode");
                        ImGui::SameLine(labelW);
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("##WrapMode", &idx, wrapLabels, 3))
                        {
                            tex->wrapMode = static_cast<TextureWrapMode>(idx);
                            registry->SaveTextureMeta(texHandle);
                            registry->ReapplyTextureParams(texHandle);
                        }
                    }
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Pixels/Unit");
                        ImGui::SameLine(labelW);
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragFloat("##PPU", &tex->pixelsPerUnit, 1.f, 1.f, 2048.f, "%.0f"))
                            registry->SaveTextureMeta(texHandle);
                    }
                    {
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Mipmaps");
                        ImGui::SameLine(labelW);
                        if (ImGui::Checkbox("##Mipmaps", &tex->generateMipmaps))
                            registry->SaveTextureMeta(texHandle);
                    }
                }
            })
        );

        // Audio (.wav, .ogg, etc.)
        ad.Register(
            AssetDescriptor::Create(AssetType::Audio, "SFX", "Audio Clip",
                                     {0.90f, 0.40f, 0.80f, 1.f},
                                     {".wav", ".ogg", ".mp3", ".flac"})
            .Inspect([](const AssetContext& ctx) {
                ImGui::Spacing();
                ImGui::TextWrapped("Audio clip -- add an Audio Source component to a scene node "
                                   "and set 'Clip Path' to this file's path.");
                ImGui::Spacing();
                ImGui::SeparatorText("Clip Info");
                ImGui::TextDisabled("Clip Path");
                ImGui::SameLine(100);
                ImGui::TextWrapped("%s", ctx.relativePath.c_str());
                if (ImGui::SmallButton("Copy Clip Path"))
                    ImGui::SetClipboardText(ctx.relativePath.c_str());
            })
        );

        // Material (.material)
        ad.Register(
            AssetDescriptor::Create(AssetType::Material, "MAT", "Material",
                                     {0.85f, 0.65f, 0.13f, 1.f}, {".material"})
            .Inspect([](const AssetContext& ctx) {
                ImGui::Spacing();
                ImGui::TextWrapped("Drag onto a sprite in the viewport or onto the inspector "
                                   "to assign this material.");

                auto* registry = ctx.engine->assetRegistry_.get();
                if (!registry) return;

                auto matHandle = registry->GetMaterial(ctx.relativePath);
                auto* mat = registry->Materials().Get(matHandle);
                if (mat && mat->IsValid())
                {
                    ImGui::Spacing();
                    ImGui::SeparatorText("Material Properties");
                    ImGui::TextDisabled("Shader");
                    ImGui::SameLine(80);
                    ImGui::Text("%s", mat->shaderName.c_str());
                    ImGui::TextDisabled("Texture");
                    ImGui::SameLine(80);
                    ImGui::Text("%s", mat->texturePath.c_str());

                    if (!mat->texturePath.empty())
                    {
                        auto texH   = registry->GetTexture(mat->texturePath);
                        GLuint glTex = registry->GetGLTextureHandle(texH);
                        if (glTex != 0)
                        {
                            float previewSize = ImGui::GetContentRegionAvail().x;
                            ImGui::Image(reinterpret_cast<ImTextureID>(
                                static_cast<intptr_t>(glTex)),
                                ImVec2(previewSize, previewSize));
                        }
                    }
                }
            })
            .OnViewportDrop([](const AssetContext& ctx) -> bool {
                std::string matPath = ctx.relativePath;

                if (ctx.targetNode)
                {
                    if (auto* rn = ctx.targetNode->GetComponent<RenderableNode>())
                    {
                        if (auto* spr = dynamic_cast<Sprite*>(rn->renderable_.get()))
                        {
                            spr->SetMaterialPath(matPath);
                            spdlog::info("[DevEditor] Assigned material '{}' to '{}'",
                                         matPath, ctx.targetNode->GetName());
                            return true;
                        }
                    }
                }

                auto sprite = std::make_shared<Sprite>();
                sprite->SetMaterialPath(matPath);

                std::string name = "Sprite_" +
                    std::filesystem::path(matPath).stem().string();
                auto node = std::make_shared<SceneNode>(name);
                node->AddComponent(RenderableNode(sprite));

                ctx.engine->mainScene_->root_node_->AddChild(node);
                ctx.engine->renderEngine_.AddRenderable(sprite);
                spdlog::info("[DevEditor] Spawned sprite with material '{}'", matPath);
                return true;
            })
            .OnInspectorDrop([](const AssetContext& ctx) -> bool {
                if (!ctx.targetNode) return false;
                if (auto* rn = ctx.targetNode->GetComponent<RenderableNode>())
                {
                    if (auto* spr = dynamic_cast<Sprite*>(rn->renderable_.get()))
                    {
                        spr->SetMaterialPath(ctx.relativePath);
                        spdlog::info("[DevEditor] Material '{}' assigned via inspector", ctx.relativePath);
                        return true;
                    }
                }
                return false;
            })
            .ExtraPayload("MATERIAL_ASSET", [](const AssetContext& ctx) -> std::string {
                return ctx.relativePath;
            })
            .DragTip("Drop onto sprite to assign")
        );
    }

    void DevEditor::UpdateUI() { DrawEditor(); }

    void DevEditor::Update()
    {
        // Detect scene change -- clear stale selections/follow state from previous scene
        {
            uint32_t gen = engineInstance_->sceneGeneration_;
            if (gen != lastSceneGeneration_)
            {
                lastSceneGeneration_ = gen;
                selectedNodes_.clear();
                inspectorSource_ = InspectorSource::None;
                selectedAsset_.active = false;
                if (isFollowing_) StopFollowing();
                trajectoryTrails_.clear();
            }
        }

        // Trajectory sampling
        if (showTrajectoryDebug_)
            UpdateTrajectories(ImGui::GetIO().DeltaTime);

        // Follow-camera: keep editor camera centered on followed node
        if (isFollowing_)
        {
            if (auto target = followTarget_.lock())
            {
                auto pos = target->transform_.getGlobalPosition();
                auto* camCtrl = engineInstance_->editorCamera_->editorCameraControl_.get();
                if (camCtrl && camCtrl->linkedTransform_)
                    camCtrl->linkedTransform_->setGlobalPosition(pos);
            }
            else
            {
                StopFollowing();
            }
        }
    }

    void DevEditor::FocusCameraOnNode(const std::shared_ptr<SceneNode>& node)
    {
        if (!node) return;
        auto pos = node->transform_.getGlobalPosition();
        auto* camCtrl = engineInstance_->editorCamera_->editorCameraControl_.get();
        if (camCtrl && camCtrl->linkedTransform_)
            camCtrl->linkedTransform_->setGlobalPosition(pos);
    }

    void DevEditor::StartFollowing(const std::shared_ptr<SceneNode>& node)
    {
        if (!node) return;
        followTarget_ = node;
        isFollowing_  = true;
        FocusCameraOnNode(node);
        spdlog::info("[DevEditor] Following '{}'", node->GetName());
    }

    void DevEditor::StopFollowing()
    {
        isFollowing_ = false;
        followTarget_.reset();
    }

    // -------------------------------------------------------------------------
    // TRAJECTORY TRAIL GIZMO
    // -------------------------------------------------------------------------

    void DevEditor::UpdateTrajectories(float dt)
    {
        if (!engineInstance_->mainScene_) return;

        trailSampleTimer_ += dt;
        if (trailSampleTimer_ < trailSampleInterval_) return;
        trailSampleTimer_ = 0.f;

        auto& rbPool   = engineInstance_->mainScene_->registry_.Pool<RigidBodyComponent>();
        auto& comps    = rbPool.Components();
        auto& entities = rbPool.Entities();

        // Prune trails for entities that no longer exist
        for (auto it = trajectoryTrails_.begin(); it != trajectoryTrails_.end(); )
        {
            bool found = false;
            for (size_t i = 0; i < entities.size(); ++i)
            {
                if (static_cast<uint64_t>(entities[i]) == it->first) { found = true; break; }
            }
            if (!found) it = trajectoryTrails_.erase(it);
            else ++it;
        }

        for (size_t i = 0; i < comps.size(); ++i)
        {
            auto& rb = comps[i];
            if (!rb.IsInitialized() || !rb.IsDynamic()) continue;

            uint64_t key = static_cast<uint64_t>(entities[i]);

            // Cap tracked bodies
            if (trajectoryTrails_.find(key) == trajectoryTrails_.end()
                && trajectoryTrails_.size() >= kTrailMaxBodies)
                continue;

            auto& trail = trajectoryTrails_[key];
            trail.positions[trail.head] = rb.GetPosition();
            trail.mass = rb.GetMass();
            trail.head = (trail.head + 1) % kTrailLength;
            if (trail.count < kTrailLength) ++trail.count;
        }
    }

    void DevEditor::DrawTrajectoryGizmos(ImVec2 imgMin, ImVec2 imgSize)
    {
        if (!engineInstance_->mainScene_ || trajectoryTrails_.empty()) return;

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

        // HSV hue palette -- each body gets a unique hue based on entity ID
        auto entityColor = [](uint64_t id, float alpha) -> ImU32 {
            float hue = glm::mod(id * 0.618033988f, 1.0f); // golden ratio scatter
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(hue, 0.85f, 1.0f, r, g, b);
            return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(alpha * 255));
        };

        for (auto& [entityId, trail] : trajectoryTrails_)
        {
            if (trail.count < 2) continue;

            // Fused bodies get thicker lines
            const float thickness = (trail.mass > 1.1f) ? 2.5f : 1.2f;

            // Draw trail segments from oldest to newest with fading alpha
            for (int i = 0; i < trail.count - 1; ++i)
            {
                // Ring buffer: oldest sample is at head (wraps around)
                int idxA = (trail.head - trail.count + i + kTrailLength) % kTrailLength;
                int idxB = (idxA + 1) % kTrailLength;

                float t = static_cast<float>(i) / static_cast<float>(trail.count - 1);

                // Fade: older = transparent, newer = opaque
                float alpha = t * t * 0.9f; // quadratic fade-in

                ImU32 col = entityColor(entityId, alpha);
                ImVec2 a = toScreen(trail.positions[idxA]);
                ImVec2 b = toScreen(trail.positions[idxB]);

                dl->AddLine(a, b, col, thickness * (0.3f + 0.7f * t));
            }

            // Draw a bright dot at the current (newest) position
            if (trail.count > 0)
            {
                int newest = (trail.head - 1 + kTrailLength) % kTrailLength;
                ImVec2 headPt = toScreen(trail.positions[newest]);
                ImU32 headCol = entityColor(entityId, 1.0f);
                float dotRadius = (trail.mass > 1.1f) ? 4.0f : 2.5f;
                dl->AddCircleFilled(headPt, dotRadius, headCol);
            }
        }
    }

} // namespace ettycc
