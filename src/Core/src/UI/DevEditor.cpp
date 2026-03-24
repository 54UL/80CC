#include <UI/DevEditor.hpp>
#include <Dependency.hpp>
#include <Dependencies/Resources.hpp>
#include <Input/Controls/EditorCamera.hpp>
#include <unordered_map>
#include <algorithm>
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
    }

    DevEditor::~DevEditor() {}

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
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpaceOverViewport(NULL, dockspace_flags);
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

    void DevEditor::ShowEditorViewPort() const
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
            GLuint framebufferTextureID = framebuffer->GetTextureId();
            glm::ivec2 fbSize = framebuffer->GetSize();
            float fbAspect = static_cast<float>(fbSize.x) / static_cast<float>(fbSize.y);

            ImVec2 avail = ImGui::GetContentRegionAvail();
            float availAspect = avail.x / avail.y;

            ImVec2 displaySize;
            if (availAspect > fbAspect) {
                displaySize.y = avail.y;
                displaySize.x = displaySize.y * fbAspect;
            } else {
                displaySize.x = avail.x;
                displaySize.y = displaySize.x / fbAspect;
            }

            framebuffer->SetSize(glm::ivec2(static_cast<int>(avail.x), static_cast<int>(avail.y)));

            ImVec2 cursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cursorPos.x, cursorPos.y));

            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(framebufferTextureID)),
                         avail, ImVec2(0, 1), ImVec2(1, 0));

            // Accept prefab template drops → spawn under scene root
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ENTRY"))
                {
                    std::string path(static_cast<const char*>(payload->Data), payload->DataSize - 1);
                    AssetType dropType = GetAssetType(std::filesystem::path(path));

                    if (dropType == AssetType::Template)
                    {
                        auto nodes = assetBuilder_->BuildFromTemplate(path);
                        for (auto& node : nodes)
                        {
                            // AddChild → AddNode → GetDependency(Engine) → OnStart (Init + AddRenderable)
                            engineInstance_->mainScene_->root_node_->AddChild(node);
                            spdlog::info("[DevEditor] Spawned '{}' from prefab '{}'",
                                         node->GetName(), path);
                        }
                    }
                    else
                    {
                        spdlog::warn("[DevEditor] Drop type '{}' not handled yet",
                                     GetAssetTypeName(dropType));
                    }
                }
                ImGui::EndDragDropTarget();
            }

            static bool isViewportFocused = false;
            static ImVec2 lockedCursorPos;

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_None)) {
                engineInstance_->editorCamera_->editorCameraControl_->enabled = true;
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    if (!isViewportFocused) {
                        isViewportFocused = true;
                        lockedCursorPos = ImGui::GetMousePos();
                    }
                    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
                    ImGui::GetIO().MousePos = lockedCursorPos;
                } else {
                    isViewportFocused = false;
                }
            } else {
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

        bool isActive = true;
        ImGui::Checkbox("##isActive", &isActive);
        ImGui::SameLine();
        char nameBuf[128] = "";
        {
            std::string name = selectedNode->GetName();
            strncpy(nameBuf, name.empty() ? "UNNAMED" : name.c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf)))
            selectedNode->SetName(nameBuf);

        ImGui::Separator();

        static std::unordered_map<const void*, TransformUI> transformCache;
        const void* key = selectedNode.get();
        TransformUI& uiTransform = transformCache[key];

        if (!uiTransform.initialized)
        {
            SetTransformUIFromNode(selectedNode, uiTransform);
            uiTransform.initialized = true;
        }

        static bool updated = false;
        if (ImGui::BeginTable("##transform_table", 1, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Position");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::DragFloat3("##position", uiTransform.pos, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
                updated = true;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Rotation");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::DragFloat3("##rotation", uiTransform.rot, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
                updated = true;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Scale");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::DragFloat3("##scale", uiTransform.scale, 0.1f, 0.0f, FLT_MAX, "%.3f"))
                updated = true;

            ImGui::EndTable();
        }

        if (updated)
        {
            auto spriteComponent = selectedNode->GetComponentByName(RenderableNode::componentType);
            if (spriteComponent)
            {
                auto renderableNode = std::dynamic_pointer_cast<RenderableNode>(spriteComponent);
                if (renderableNode && renderableNode->renderable_)
                {
                    Transform newTransform;
                    newTransform.setGlobalPosition({uiTransform.pos[0], uiTransform.pos[1], uiTransform.pos[2]});
                    newTransform.setGlobalRotation({uiTransform.rot[0], uiTransform.rot[1], uiTransform.rot[2]});
                    newTransform.setGlobalScale({uiTransform.scale[0], uiTransform.scale[1], uiTransform.scale[2]});

                    renderableNode->renderable_->SetTransform(newTransform);
                    updated = false;
                }
            }
        }

        ImGui::TextDisabled("Reset Position / Rotation / Scale");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("add_component_popup");

            if (ImGui::BeginPopup("add_component_popup"))
            {
                if (ImGui::MenuItem("Camera"))
                    AddComponentFromTemplate(selectedNode, "Camera");
                if (ImGui::MenuItem("Sprite"))
                    AddComponentFromTemplate(selectedNode, "Sprite");
                ImGui::EndPopup();
            }

            ImGui::Spacing();
            ImGui::TextWrapped("Component introspection is not enabled in this build.");
        }

        ImGui::Separator();
        ImGui::TextColored({0.3f, 0.9f, 0.3f, 1.0f}, "[ Source: Scene Node ]");
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

        bool open = true;
        ShowMenuBar();
        ShowDockSpace();
        ImGui::ShowDemoWindow(&open);
        ShowDebugger();
        ShowEditorViewPort();
        ShowGameView();
        ShowInspector();
        ShowSceneHierarchy();
        ShowAssetsView();
    }

    void DevEditor::Init()
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        engineInstance_->InitEditorCamera();

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
