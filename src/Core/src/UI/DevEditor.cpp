#include <UI/DevEditor.hpp>
#include <imgui_internal.h>  // For docking
#include <stb_image.h>
#include <stack>
#include <Dependency.hpp>

namespace ettycc
{
    DevEditor::DevEditor()
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
    
    static int viewportNumber = 1;

    void DevEditor::ShowViewport()
    {
        ImGui::Begin("Game view");

        // Rendering of the main engine viewport (it's suppossed to have multiple frame buffers for split screen or image effects (todo: composited game view))
        // Get the framebuffer texture ID
        auto frambuffer = GetDependency(Engine)->renderEngine_.GetViewPortFrameBuffer();
        GLuint framebufferTextureID = frambuffer->GetTextureId();
        frambuffer->SetSize(glm::ivec2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y));
        
        // Display the framebuffer texture in an ImGui image
        ImVec2 imageSize(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y );
        ImGui::Image((void*)(intptr_t)framebufferTextureID, imageSize);

        ImGui::End();
    }

    void DevEditor::ShowInspector()
    {
        ImGui::Begin("Inspector");

        std::string name;
        float position[3] = {0.0f,0.0f,0.0f};
        float scale[3] = {1.0f,1.0f,1.0f};
        float rotation[3] = {0.0f,0.0f,0.0f};

        // Add other properties as needed
        // ImGui::SameLine();
        ImGui::Text("Position");
        ImGui::SliderFloat3("##Pos", position, -10.0f, 10.0f);
        ImGui::InputFloat3("##PosEdit", position, "%.2f");
        ImGui::Text("Rotation");
        ImGui::SliderFloat3("##Rot", position, -10.0f, 10.0f);
        ImGui::InputFloat3("##PosEdit", position, "%.2f");
        // Add more properties as needed

        ImGui::End();
    }

    void DevEditor::ShowSceneHierarchy()
    {
        ImGui::Begin("Scene Hierarchy");

        RenderTree(); // demo example
        
        // Add content for the scene hierarchy here
        ImGui::End();
    }

    void DevEditor::ShowAssetsView()
    {
        ImGui::Begin("Assets");

        // RenderTree(); // demo example
        
        // Add content for the scene hierarchy here

        ImGui::End();
    }

    GLuint DevEditor::LoadTextureFromFile(const char *filePath)
    {
        // load the image with stbi_load
        int width, height, numChannels;
        unsigned char* image = stbi_load(filePath, &width, &height, &numChannels, 0);

        // Generate OpenGL texture
        GLuint textureID;

        if (image) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            glGenerateMipmap(GL_TEXTURE_2D);

            stbi_image_free(image);
        } else {
            std::cerr << "Error loading image: " << stbi_failure_reason() << std::endl;
        }

        return textureID; // IMPORTANT (DONT LOSE THIS ID SO IT WOULD BE GREAT TO ENCAPSULATE THIS IN A CLASS TO USE RAII)
    }

    void ShowContextMenu()
    {
        if (ImGui::BeginPopupContextItem()) // This creates a context menu for the current item
        {
            if (ImGui::MenuItem("Option 1"))
            {
                // Code to execute when Option 1 is selected
            }
            if (ImGui::MenuItem("Option 2"))
            {
                // Code to execute when Option 2 is selected
            }

            // Add more items as needed

            ImGui::EndPopup();
        }
    }

    /*
    void RenderSceneNode(Node &rootNode)
    {
        std::stack<std::pair<Node *, int>> nodeStack;
        nodeStack.push({&rootNode, 0});

        while (!nodeStack.empty())
        {
            std::pair<Node *, int> nodePair = nodeStack.top();
            Node *currentNode = nodePair.first;
            int depth = nodePair.second;
            nodeStack.pop();

            bool isNodeOpen = ImGui::TreeNodeEx(currentNode->name.c_str());

            if (ImGui::IsItemClicked())
            {
                currentNode->selected = !currentNode->selected;
            }

            if (ImGui::BeginPopupContextItem("NodeContextMenu"))
            {
                if (ImGui::Selectable("Edit Name"))
                {
                    ImGui::OpenPopup("EditNamePopup");
                }
                if (ImGui::Selectable("Add Child"))
                {
                    currentNode->children.push_back({"New Child", false, {}});
                }
                if (ImGui::Selectable("Delete Node"))
                {
                    // Add your code here for deleting the node
                    // This can be done by removing the node from its parent's children vector
                }
                if (ImGui::Selectable("Convert to Prefab"))
                {
                    // Add your code here for converting the node to a prefab
                }
                ImGui::EndPopup();
            }

            // Edit name popup
            if (ImGui::BeginPopup("EditNamePopup"))
            {
                static char newName[64] = {0};
                ImGui::InputText("New Name", newName, sizeof(newName));
                if (ImGui::Button("Apply") && newName[0] != '\0')
                {
                    currentNode->name = newName;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (isNodeOpen)
            {
                ImGui::Indent(depth * 20.0f);

                for (auto it = currentNode->children.rbegin(); it != currentNode->children.rend(); ++it)
                {
                    nodeStack.push({&(*it), depth + 1});
                }
                ImGui::TreePop();
                ImGui::Separator();

                // ImGui::Unindent(depth * 20.0f);
            }
        }
    }
    */
    // Function to recursively render the tree
    void DevEditor::RenderTree()
    {
        // engineInstance_->
        // for (Node &node : nodes)
        // {
        //     RenderSceneNode(node);
        // }

        // const char *names[5] = {"Label1", "Label2", "Label3", "Label4", "Label5"};
        // static bool selection[5] = { false, false, false, false, false };
        // char buf[32];
        // for (int n = 0; n < 5; n++)
        // {
        //     sprintf(buf, "Object %s", names[n]);
        //     if (ImGui::Selectable(buf, selection[n]))
        //     {
        //         if (!ImGui::GetIO().KeyCtrl) // Clear selection when CTRL is not held
        //             memset(selection, 0, sizeof(selection));
        //         selection[n] ^= 1;
        //     }

        //     if (ImGui::BeginPopupContextItem())
        //     {
        //         ImGui::Text("This a popup for \"%s\"!", names[n]);
        //         if (ImGui::Button("Close"))
        //             ImGui::CloseCurrentPopup();
        //         ImGui::EndPopup();
        //     }
        //     ImGui::SetItemTooltip("Right-click to open popup");
        // }
        // Render the tree structure
    }
    
    static const int MAX_SAMPLES = 32;
    static float values[MAX_SAMPLES] = { 0.0f };  // Your data values
    static int sampleIndex = 0;  // Your data values

    void PlotGraph()
    {

        // Alternatively, you can use ImGui::PlotHistogram for a histogram plot
        // ImGui::PlotHistogram("Graph Title", values, IM_ARRAYSIZE(values));
    }

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
                
                auto mousepos = engineInstance->inputSystem_.GetMousePos();
                ImGui::Text("Delta time: %.4f", engineInstance->appInstance_->GetDeltaTime());
                ImGui::Text("App time: %.4f", engineInstance->appInstance_->GetCurrentTime());
                ImGui::Text("Mouse x: [%i] Mouse y:[%i]",mousepos.x, mousepos.y);
                ImGui::Text("FPS: %.4f", (1.0f / engineInstance->appInstance_->GetDeltaTime()));
                // ImVec2 graphSize(200, 200);  // Adjust the size as needed
                
                plotStep += engineInstance->appInstance_->GetDeltaTime();
                ImGui::PlotHistogram("HISTOGRAM", values, IM_ARRAYSIZE(values));

                if (sampleIndex < MAX_SAMPLES && plotStep >= 1) {
                    values[sampleIndex] = (1.0f / engineInstance->appInstance_->GetDeltaTime());
                    sampleIndex++;
                    plotStep = 0;
                }
                else if (sampleIndex >= MAX_SAMPLES){
                    for (int i =0; i < MAX_SAMPLES; i++){
                        values[i] = 0;
                    }
                    sampleIndex = 0;
                }

                ImGui::EndTabItem();
            }

            // End the tab bar
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void DevEditor::DrawEditor()
    {
        bool open = true;
        ShowMenuBar();
        
        ShowDockSpace(); 

        ImGui::ShowDemoWindow(&open);

        ShowDebugger();
        
        ShowViewport();

        // ShowInspector();

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
