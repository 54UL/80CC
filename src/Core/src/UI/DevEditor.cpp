#include <UI/DevEditor.hpp>
#include <imgui_internal.h>  // For docking
#include <stb_image.h>

namespace ettycc
{
    DevEditor::DevEditor(std::shared_ptr<Engine> engineInstance) : engineInstance_(engineInstance)
    {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
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
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                // Add file menu items here
                ImGui::EndMenu();
            }

            // Add more menus if needed

            ImGui::EndMenuBar();
        }
    }

    void DevEditor::ShowSidePanel()
    {
        ImGui::Begin("Side Panel", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // Add content for the side panel here

        ImGui::End();
    }

    void DevEditor::ShowViewport()
    {
        ImGui::Begin("Viewport");

        // ImGui::Begin("Floating Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

        // ImVec2 buttonSize(32, 32);

        // if (ImGui::ImageButton(0, buttonSize) && ImGui::IsItemClicked())
        // {
        //     // Handle button 1 click
        // }

        // if (ImGui::ImageButton(0, buttonSize) && ImGui::IsItemClicked())
        // {
        //     // Handle button 2 click
        // }

        // ImGui::End();

        // Rendering of the main engine viewport (it's suppossed to have multiple frame buffers for split screen or image effects (todo: composited game view))
        // Get the framebuffer texture ID
        GLuint framebufferTextureID = engineInstance_->renderEngine_.GetViewPortFrameBuffer()->GetTextureId();

        // Display the framebuffer texture in an ImGui image
        ImVec2 imageSize(ImGui::GetWindowSize().x - 10, ImGui::GetWindowSize().y - 10);
        ImGui::Image((void*)(intptr_t)framebufferTextureID, imageSize);

        ImGui::End();
    }

    void DevEditor::ShowInspector()
    {
        ImGui::Begin("Inspector");

        // Add content for the inspector here

        ImGui::End();
    }

    void DevEditor::ShowSceneHierarchy()
    {
        ImGui::Begin("Scene Hierarchy");

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

    // Function to recursively render the tree
    void DevEditor::RenderTree(const Object &obj)
    {
        // Display the current object in the tree
        if (ImGui::TreeNode(obj.name.c_str()))
        {
            // Render children recursively
            for (const auto &child : obj.children)
            {
                RenderTree(child);
            }

            // Close the tree node
            ImGui::TreePop();
        }
    }

    void DevEditor::RenderTree()
    {
        // Sample data: Create a tree structure
        Object rootObject{"Root", {{"Object1", {{"Child1", {}}, {"Child2", {}}}}, {"Object2", {{"Child3", {}}, {"Child4", {{"Grandchild1", {}}, {"Grandchild2", {}}}}}}}};

        // Render the tree structure
        RenderTree(rootObject);
    }

    void DevEditor::ShowDebugger()
    {
        // Begin a new tab bar
        if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_Reorderable))
        {

            // Tab 1
            if (ImGui::BeginTabItem("Misc"))
            {
                auto mousepos = engineInstance_->inputSystem_.GetMousePos();
                ImGui::Text("Delta time: %.4f", engineInstance_->appInstance_->GetDeltaTime());
                ImGui::Text("Mouse x: [%i] Mouse y:[%i]",mousepos.x, mousepos.y);
                ImGui::EndTabItem();
            }

            // Tab 2
            if (ImGui::BeginTabItem("Rendering"))
            {
                RenderTree();
                ImGui::EndTabItem();
            }

            // End the tab bar
            ImGui::EndTabBar();
        }
    }

    void DevEditor::DrawEditor()
    {
        ShowDebugger();
          
        ShowDockSpace();

        ShowMenuBar();

        ShowSidePanel();

        ShowViewport();

        ShowInspector();

        ShowSceneHierarchy();
    }

    void DevEditor::Init()
    {

    }

    void DevEditor::UpdateUI()
    {
        // IMGUI EDITOR STARTS HERE...
        
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing);
        // ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
        ImGui::Begin("80CC [DEV EDITOR]");//ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize

        // editor update goes here..
        DrawEditor();

        ImGui::End();
    }

    void DevEditor::Update()
    {
        // Does nothing...
    }
} // namespace ettycc
