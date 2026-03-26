#ifndef EDITOR_UI
#define EDITOR_UI

#include <Engine.hpp>
#include <App/ExecutionPipeline.hpp>
#include <UI/Console.hpp>
#include <UI/ImGuiConsoleSink.hpp>
#include <UI/BuildPanel.hpp>
#include <Scene/Assets/AssetBuilder.hpp>
#include <Graphics/Rendering/PickerBuffer.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <stb_image.h>
#include <stack>
#include <memory>
#include <filesystem>
#include <Scene/Components/RenderableNode.hpp>

namespace ettycc
{
    class DevEditor : public ExecutionPipeline
    {
    public:
        DevEditor(const std::shared_ptr<Engine>& engine);
        ~DevEditor();
        void Init() override;
        void UpdateUI() override;
        void Update() override;

    private:
        void DrawEditor();

        // WINDOWS
        void ShowDebugger();
        void ShowDockSpace();
        void ShowMenuBar();
        void ShowEditorViewPort();
        void ShowGameView() const;
        void ShowInspector();
        void ShowAssetsView();
        void ShowSceneHierarchy();
        // SCENE HIERARCHY
        std::shared_ptr<Engine> engineInstance_;
        std::vector<std::shared_ptr<SceneNode>> selectedNodes_;
        std::string searchFilter_;
        DebugConsole uiConsole;
        bool uiConsoleOpen_;

        void RenderSceneTree();
        void RenderSceneNode(const std::shared_ptr<SceneNode>& rootNode, std::vector<std::shared_ptr<SceneNode>>& selectedNodes, int depth);
        void ShowSceneContextMenu(const std::shared_ptr<SceneNode>& node);
        void AddNode(const std::shared_ptr<SceneNode>& selectedNode);
        void AddComponentFromTemplate(const std::shared_ptr<SceneNode>& selectedNode, const char* templateName);

        // ASSET BROWSER ############################################################
        enum class AssetType { Template, Scene, Config, Code, Shader, Image, Unknown };

        struct AssetEntry {
            std::string path;
            std::string name;
            AssetType   type;
        };

        struct SelectedAsset {
            std::string path;
            std::string name;
            AssetType   type;
            uintmax_t   fileSize = 0;
            bool        active   = false;
        };

        enum class InspectorSource { None, SceneNode, Asset };

        std::shared_ptr<AssetLoader>   assetLoader_;
        std::shared_ptr<AssetBuilder>  assetBuilder_;
        std::shared_ptr<PickerBuffer>  pickerBuffer_;
        uint32_t                       lastPickedId_ = 0;
        std::vector<AssetEntry>       assetEntries_;
        bool                          assetsScanned_ = false;
        std::string                   currentFolder_;
        SelectedAsset                 selectedAsset_;
        InspectorSource               inspectorSource_ = InspectorSource::None;

        void        ScanAssets();
        void        RenderFolderTree(const std::filesystem::path& path);
        void        RenderAssetGrid(const std::string& searchQuery);
        AssetType   GetAssetType(const std::filesystem::path& p) const;
        ImVec4      GetAssetColor(AssetType t) const;
        const char* GetAssetLabel(AssetType t) const;
        const char* GetAssetTypeName(AssetType t) const;

        // VIEWPORT HELPERS
        std::shared_ptr<SceneNode> FindNodeByRenderable(
            const std::shared_ptr<SceneNode>& node,
            const std::shared_ptr<Renderable>& renderable) const;

        // MISC
        GLuint LoadTextureFromFile(const char* filePath);

        // BUILD WINDOW
        BuildPanel buildPanel_;
    };
} // namespace ettycc

#endif
