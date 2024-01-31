#include <UI/DevEditor.hpp>
#include <imgui_internal.h>  // For docking

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

        // Add content for the viewport here

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
                ImGui::Text("Delta time: %.4f", engineInstance_->appInstance_->GetDeltaTime());
                auto mousepos = engineInstance_->inputSystem_.GetMousePos();
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
