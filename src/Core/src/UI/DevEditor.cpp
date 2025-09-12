#include <UI/DevEditor.hpp>
#include <Dependency.hpp>

#include "Input/Controls/EditorCamera.hpp"
#include <unordered_map>
#include <cstring>

namespace ettycc
{
    // move tf out of here...
    static int viewportNumber = 1;
    static bool frameBufferErrorShown = false;

    DevEditor::DevEditor(const std::shared_ptr<Engine>& engine) : engineInstance_(engine), uiConsoleOpen_(false)
    {
    }

    DevEditor::~DevEditor()
    {
    }

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
                ImGui::MenuItem("Open", NULL);
                ImGui::MenuItem("Save", NULL);
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

    void DevEditor::ShowSidePanel()
    {
        // ImGui::Begin("Side Panel", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // // Add content for the side panel here

        // ImGui::End();
    }

    void DevEditor::ShowViewport()
    {
        ImGui::Begin("Game view");

        auto framebuffer = GetDependency(Engine)->renderEngine_.GetViewPortFrameBuffer();

        if (framebuffer)
        {
            GLuint framebufferTextureID = framebuffer->GetTextureId();

            // Query the framebuffer’s *native ratio* (you may store or compute it once)
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

            framebuffer->SetSize(glm::ivec2(static_cast<int>(displaySize.x), static_cast<int>(displaySize.y)));

            // Center the image inside the window
            ImVec2 cursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(
                cursorPos.x + (avail.x - displaySize.x) * 0.5f,
                cursorPos.y + (avail.y - displaySize.y) * 0.5f
            ));

            // Draw the framebuffer
            ImGui::Image(reinterpret_cast<void *>(static_cast<intptr_t>(framebufferTextureID)), displaySize, ImVec2(0,1), ImVec2(1,0));

            static bool isViewportFocused = false;
            static ImVec2 lockedCursorPos;

            // ALL THIS LOGIC SHOULD BE LOCATED ON THE CAMERA CONTROL CLASS (EDITOR CAMERA)
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_None)) {
                engineInstance_->editorCamera_->editorCameraControl_->enabled = true;

                if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
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
            if (!frameBufferErrorShown)
            {
                spdlog::error("DevEditor:ShowViewport():GetViewPortFrameBuffer() was nullptr");
                frameBufferErrorShown = true;
            }
        }

        ImGui::End();
    }    

    void DevEditor::ShowInspector()
    {
        ImGui::Begin("Inspector");

        if (selectedNodes_.empty())
        {
            ImGui::TextDisabled("No selection");
            ImGui::Separator();
            ImGui::TextWrapped("Select a node in the Scene Hierarchy to inspect and edit its properties.");
            ImGui::End();
            return;
        }

        if (selectedNodes_.size() > 1)
        {
            ImGui::Text("Multiple Selection (%zu)", selectedNodes_.size());
            ImGui::Separator();
            ImGui::TextWrapped("Editing multiple objects at once is supported in the future. Use single selection for now.");
            ImGui::End();
            return;
        }

        auto selectedNode = selectedNodes_.back();

        struct TransformUI
        {
            float pos[3];
            float rot[3];
            float scale[3];
            bool initialized = false;
        };

        static std::unordered_map<const void*, TransformUI> transformCache;
        const void* key = static_cast<const void*>(selectedNode.get());
        TransformUI &uiTransform = transformCache[key];

        if (!uiTransform.initialized)
        {
            uiTransform.pos[0] = uiTransform.pos[1] = uiTransform.pos[2] = 1.0f;
            uiTransform.rot[0] = uiTransform.rot[1] = uiTransform.rot[2] = 1.0f;
            uiTransform.scale[0] = uiTransform.scale[1] = uiTransform.scale[2] = 1.0f;
            uiTransform.initialized = true;
        }

        bool isActive = true;
        ImGui::Checkbox("##isActive", &isActive);
        ImGui::SameLine();
        char nameBuf[128] = "";
        {
            std::string name = selectedNode->GetName();
            if (name.size() == 0)
                strncpy(nameBuf, "UNNAMED", sizeof(nameBuf) - 1);
            else
                strncpy(nameBuf, name.c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf)))
        {
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("##transform_table", 1, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Position");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragFloat3("##position", uiTransform.pos, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
                {
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Rotation");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragFloat3("##rotation", uiTransform.rot, 0.1f, -FLT_MAX, FLT_MAX, "%.3f"))
                {
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Scale");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::DragFloat3("##scale", uiTransform.scale, 0.1f, 0.0f, FLT_MAX, "%.3f"))
                {
                }

                ImGui::EndTable();
            }

            ImGui::Separator();
            if (ImGui::Button("Reset"))
            {
                uiTransform.pos[0] = uiTransform.pos[1] = uiTransform.pos[2] = 0.0f;
                uiTransform.rot[0] = uiTransform.rot[1] = uiTransform.rot[2] = 0.0f;
                uiTransform.scale[0] = uiTransform.scale[1] = uiTransform.scale[2] = 1.0f;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Reset Position / Rotation / Scale");
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("add_component_popup");

            if (ImGui::BeginPopup("add_component_popup"))
            {
                if (ImGui::MenuItem("Camera"))
                {
                    AddComponentFromTemplate(selectedNode, "Camera");
                }
                if (ImGui::MenuItem("Sprite"))
                {
                    AddComponentFromTemplate(selectedNode, "Sprite");
                }
                ImGui::EndPopup();
            }

            ImGui::Spacing();

            ImGui::TextWrapped("Component introspection is not enabled in this UI build. Use Add Component to attach mock components (Sprite/Camera).\n\nTo show real component fields, implement a simple reflection or expose accessors on SceneNode and components.");
        }

        ImGui::End();
    }

    void DevEditor::ShowSceneHierarchy()
    {
        ImGui::Begin("Scene Hierarchy");

        RenderSceneTree(); // demo example

        // Add content for the scene hierarchy here
        ImGui::End();
    }

    void DevEditor::ShowAssetsView()
    {
        ImGui::Begin("Assets");

        // Add content for the scene hierarchy here

        ImGui::End();
    }

    GLuint DevEditor::LoadTextureFromFile(const char *filePath)
    {
        // load the image with stbi_load
        int width, height, numChannels;
        unsigned char *image = stbi_load(filePath, &width, &height, &numChannels, 0);

        // Generate OpenGL texture
        GLuint textureID;

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
            std::cerr << "Error loading image: " << stbi_failure_reason() << std::endl;
        }

        return textureID; // IMPORTANT (DONT LOSE THIS ID SO IT WOULD BE GREAT TO ENCAPSULATE THIS IN A CLASS TO USE RAII)
    }

    static bool showPopup = false;
    void DevEditor::ShowSceneContextMenu(const std::shared_ptr<SceneNode> &node)
    {
        if (ImGui::BeginPopupContextItem()) // This creates a context menu for the current item
        {
            if (ImGui::BeginMenu("Add"))
            {
                if (ImGui::MenuItem("Node"))
                {
                    showPopup = true;
                }
                if (ImGui::BeginMenu("Components"))
                {
                    // EXAMPLE CODE... (IMPROVE!!!)
                    // TODO: FETCH COMPONENTS FROM THE PERSISTANCE UNIT (SHOULD BE ABLE TO SAVE COMPONENT PRESETS)
                    if (ImGui::MenuItem("Camera", NULL))
                    {
                        AddComponentFromTemplate(node, "Camera");
                    }
                    if (ImGui::MenuItem("Sprite", NULL))
                    {
                        AddComponentFromTemplate(node, "Sprite");
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Remove"))
            {
                // TODO: IMPLEMENT...
            }
            if (ImGui::MenuItem("Duplicate"))
            {
                // TODO: IMPLEMENT...
            }
            if (ImGui::MenuItem("Persist"))
            {
                // TODO: IMPLEMENT...
            }

            ImGui::EndPopup();
        }

        AddNode(node);
    }

    void DevEditor::AddNode(const std::shared_ptr<SceneNode> &selectedNode)
    {
        static char node_name[128] = "";

        if (showPopup)
        {
            ImGui::OpenPopup("new node");
            // showPopup = false;
            showPopup = false;
        }

        if (ImGui::BeginPopupModal("new node", NULL))
        {
            ImGui::Text("Enter node name:");
            // TODO: FIX RECURSION ISSUE WITH THIS INPUT BOX

            auto pressed = ImGui::InputText("##node_name", node_name, IM_ARRAYSIZE(node_name), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();

            if (ImGui::Button("Apply") || pressed)
            {
                // Add the new node with the propper name
                std::shared_ptr<SceneNode> newNode = std::make_shared<SceneNode>(node_name);
                selectedNode->AddChild(newNode);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    static int positionIndex = 2;
    void DevEditor::AddComponentFromTemplate(const std::shared_ptr<SceneNode> &selectedNode, const char *templateName)
    {
        const char *notFoundTexturePath = "D:/repos2/ALPHA_V1/assets/images/not_found_texture.png"; // TODO: FETCH FROM config???

        // TODO: DUMMY CODE, IMPLEMENT A REAL TEMPLATE PARSER TO BUILD UP BUILT INS COMPONENTS
        if (strcmp("Camera", templateName) == 0)
        {
            spdlog::info("Implement camera spawning!!");
        }
        if (strcmp("Sprite", templateName) == 0)
        {
            std::shared_ptr<Sprite> notFoundSprite = std::make_shared<Sprite>(notFoundTexturePath);
            notFoundSprite->underylingTransform.setGlobalPosition(glm::vec3(positionIndex, positionIndex, -5));

            selectedNode->AddComponent(std::make_shared<RenderableNode>(notFoundSprite));
        }

        positionIndex += positionIndex;
    }

    void DevEditor::RenderSceneNode(const std::shared_ptr<SceneNode> &rootNode, std::vector<std::shared_ptr<SceneNode>> &selectedNodes, int depth = 0)
    {
        auto treeNodeName = rootNode->GetName();
        const char *nodeName = treeNodeName.empty() ? "UNNAMED" : treeNodeName.c_str();

        bool isNodeOpen = ImGui::TreeNodeEx(nodeName);

        // Check if the node is selected
        bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), rootNode) != selectedNodes.end();

        if (ImGui::IsItemClicked(0)) // Left click for selection
        {
            if (!ImGui::GetIO().KeyShift) // If shift key is not pressed
                selectedNodes.clear();    // Clear previous selection if not using multi-select

            if (!isSelected) // Toggle selection if not already selected
                selectedNodes.push_back(rootNode);
            else // Deselect if already selected
                selectedNodes.erase(std::remove(selectedNodes.begin(), selectedNodes.end(), rootNode), selectedNodes.end());
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("SCENE_NODE", &rootNode, sizeof(std::shared_ptr<SceneNode>));
            ImGui::TextUnformatted(nodeName);
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SCENE_NODE"))
            {
                IM_ASSERT(payload->DataSize == sizeof(std::shared_ptr<SceneNode>));
                std::shared_ptr<SceneNode> payload_node = *(const std::shared_ptr<SceneNode> *)payload->Data;

                // Handle reordering logic here
            }
            ImGui::EndDragDropTarget();
        }

        ShowSceneContextMenu(rootNode);

        if (isNodeOpen)
        {
            if (rootNode->children_.size() > 0)
            {
                ImGui::Separator();
            }

            for (auto &child : rootNode->children_)
            {
                RenderSceneNode(child, selectedNodes, depth + 1);
            }
            ImGui::TreePop();
        }
    }

    void DevEditor::RenderSceneTree()
    {
        if (ImGui::ArrowButton("##Collapse", ImGuiDir_Down))
        {
        }

        ImGui::SameLine();
        static char search[32] = "Object name...";
        ImGui::InputText("##Search", search, IM_ARRAYSIZE(search));
        ImGui::SameLine();

        // if (ImGui::ArrowButton("##Search_scene", ImGuiDir_Right))
        // {

        // }

        ImGui::Separator();
        RenderSceneNode(GetDependency(Engine)->mainScene_->root_node_, selectedNodes_, 0);
    }

    // BASE CURVE EDITOR
    struct Point
    {
        ImVec2 pos;
        bool selected;
    };

    std::vector<Point> points;

    void CurveEditor()
    {
        // Draw the graph
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        ImVec2 start = window->DC.CursorPos;

        ImVec2 size(400, 200);
        ImVec2 end = ImVec2(start.x + size.x, start.y + size.y);

        ImGui::GetWindowDrawList()->AddRectFilled(start, end, IM_COL32(50, 50, 50, 255));
        ImGui::GetWindowDrawList()->AddRect(start, end, IM_COL32(255, 255, 255, 255));

        // Handle mouse input
        ImGuiIO &io = ImGui::GetIO();
        ImVec2 mouse_pos_in_canvas = ImVec2(io.MousePos.x - start.x, io.MousePos.y - start.y);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(start, end))
        {
            points.push_back({mouse_pos_in_canvas, false});
        }

        // Draw points
        for (Point &point : points)
        {
            ImVec2 p1 = ImVec2(start.x + point.pos.x, start.y + point.pos.y);
            ImVec2 p2 = ImVec2(p1.x + 5, p1.y + 5);
            ImGui::GetWindowDrawList()->AddRectFilled(p1, p2, point.selected ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 255, 255));
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(p1, p2))
            {
                point.selected = !point.selected;
            }
        }
    }

    static const int MAX_SAMPLES = 32;
    static float values[MAX_SAMPLES] = {0.0f}; // Your data values
    static int sampleIndex = 0;                // Your data values
    static float plotStep = 0.0f;

    void DevEditor::ShowDebugger()
    {
        auto engineInstance = GetDependency(Engine);
        ImGui::Begin("Debug");
        if (ImGui::BeginTabBar("tools", ImGuiTabBarFlags_Reorderable))
        {
            // Tab 1
            if (ImGui::BeginTabItem("Stats"))
            {
                // Yeah just put everything you would print
                auto mousepos = engineInstance->inputSystem_.GetMousePos();
                ImGui::Text("Delta time: %.4f", engineInstance->appInstance_->GetDeltaTime());
                ImGui::Text("App time: %.4f", engineInstance->appInstance_->GetCurrentTime());
                ImGui::Text("Mouse x: [%i] Mouse y:[%i]", mousepos.x, mousepos.y);

                ImGui::Text("FPS: %.4f", (1.0f / engineInstance->appInstance_->GetDeltaTime()));
                if (engineInstance->editorCamera_ != nullptr) {
                    auto& cam = engineInstance->editorCamera_->editorCameraControl_;

                    // Manual controls for camera position and zoom
                    ImGui::Separator();
                    ImGui::Text("Camera Controls");
                    ImGui::SliderFloat2("Position", &cam->position.x, -10.0f, 10.0f, "%.2f");
                    ImGui::InputFloat2("Edit Position", &cam->position.x, "%.2f");
                    ImGui::SliderFloat("Zoom", &cam->zoom, 0.1f, 10.00, "%.2f");
                    ImGui::InputFloat("Edit Zoom", &cam->zoom, 0.1);
                }

                // ImVec2 graphSize(200, 200);  // Adjust the size as needed

                plotStep += engineInstance->appInstance_->GetDeltaTime();
                ImGui::PlotHistogram("##HISTOGRAM", values, IM_ARRAYSIZE(values));

                if (sampleIndex < MAX_SAMPLES && plotStep >= 1)
                {
                    values[sampleIndex] = (1.0f / engineInstance->appInstance_->GetDeltaTime());
                    sampleIndex++;
                    plotStep = 0;
                }
                else if (sampleIndex >= MAX_SAMPLES)
                {
                    for (int i = 0; i < MAX_SAMPLES; i++)
                    {
                        values[i] = 0;
                    }
                    sampleIndex = 0;
                }

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Console"))
            {
                uiConsole.Draw("Debuging consle", &uiConsoleOpen_);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("CURVE EDITOR[TEST]"))
            {
                CurveEditor();
                ImGui::EndTabItem();
            }

            // End the tab bar
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void DevEditor::DrawEditor()
    {
        // MAIN STYLES...
        // ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        bool open = true; // MOVE THIS FROM HERE...

        ShowMenuBar();

        ShowDockSpace();

        ImGui::ShowDemoWindow(&open);

        ShowDebugger();

        ShowViewport();

        ShowInspector();

        ShowSceneHierarchy();

        // ShowAssetsView();
    }

    void DevEditor::Init()
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    void DevEditor::UpdateUI()
    {
        // IMGUI EDITOR STARTS HERE...
        DrawEditor();
    }

    void DevEditor::Update()
    {
        // Does nothing...
    }

} // namespace ettycc
