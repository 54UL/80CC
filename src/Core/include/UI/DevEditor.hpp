#ifndef EDITOR_UI
#define EDITOR_UI
#include <imgui.h>
#include <Engine.hpp>
#include <App/ExecutionPipeline.hpp>
#include <memory>

namespace ettycc
{    
    class DevEditor : public ExecutionPipeline
    {
    public:
        // Define a basic object structure
        struct Object
        {
            std::string name;
            std::vector<Object> children;
        };

    private:
        std::shared_ptr<Engine> engineInstance_;


    public:
        DevEditor(std::shared_ptr<Engine> engineInstance);
        ~DevEditor();
    
        // Editor execution pipeline
        void Init() override;
        void UpdateUI() override;
        void Update() override;
        
    private:
        void DrawEditor();
        // Editor internals
        void RenderTree(const Object &obj);
        void RenderTree();

        // MAIN EDITOR "WINDOWS"
        void ShowDebugger();
        void ShowDockSpace();
        void ShowMenuBar();
        void ShowSidePanel();
        void ShowViewport();
        void ShowInspector();
        void ShowAssetsView();
        void ShowSceneHierarchy();
    
    private:// FUNCTIONS THAT MUST BE EVERYWHERE OR BE IN A GENERIC CLASS !!!
        GLuint LoadTextureFromFile(const char * filePath);
    };
} // namespace ettycc

#endif