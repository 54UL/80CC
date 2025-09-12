#ifndef EDITOR_UI
#define EDITOR_UI

#include <Engine.hpp>
#include <App/ExecutionPipeline.hpp>
#include <UI/Console.hpp>

#include <imgui.h>
#include <imgui_internal.h>  // For docking
#include <stb_image.h>
#include <stack>
#include <memory>

#include <Scene/Components/RenderableNode.hpp>

namespace ettycc
{
    class DevEditor : public ExecutionPipeline
    {
    public:
        DevEditor(const std::shared_ptr<Engine>& engine);
        ~DevEditor();
        // Editor execution pipeline
        void Init() override;
        void UpdateUI() override;
        void Update() override;

    private:
        void DrawEditor();

        // WINDOWS ##################################################################
        void ShowDebugger();
        void ShowDockSpace();
        void ShowMenuBar();
        void ShowViewport();
        void ShowInspector();
        void ShowAssetsView();
        void ShowSceneHierarchy();

        // COMPONENTS ###############################################################

        // SCENE NODE VIEW COMPONENT ################################################
        // PROPERTIES
        std::shared_ptr<Engine> engineInstance_;
        std::vector<std::shared_ptr<SceneNode>> selectedNodes_;
        std::string searchFilter_;
        DebugConsole uiConsole;
        bool uiConsoleOpen_;

        // METHODS
        void RenderSceneTree();
        void RenderSceneNode(const std::shared_ptr<SceneNode>& rootNode, std::vector<std::shared_ptr<SceneNode>> &selectedNodes, int depth);
        void ShowSceneContextMenu(const std::shared_ptr<SceneNode>& node);

        // logic methods
        void AddNode(const std::shared_ptr<SceneNode>& selectedNode);
        void AddComponentFromTemplate(const std::shared_ptr<SceneNode>& selectedNode, const char *templateName);

    private: // FUNCTIONS THAT MUST BE EVERYWHERE OR BE IN A GENERIC CLASS !!!
        GLuint LoadTextureFromFile(const char *filePath);
    };
} // namespace ettycc

#endif