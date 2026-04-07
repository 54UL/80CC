#ifndef EDITOR_UI
#define EDITOR_UI

#include <Engine.hpp>
#include <App/ExecutionPipeline.hpp>
#include <UI/Console.hpp>
#include <UI/ImGuiConsoleSink.hpp>
#include <UI/Build/BuildPanelUI.hpp>
#include <UI/ConfigurationsWindow.hpp>
#include <UI/SpriteEditor.hpp>
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

        void DrawGravityAttractorGizmos(ImVec2 imgMin, ImVec2 imgSize);
        void DrawAudioGizmos(ImVec2 imgMin, ImVec2 imgSize);

        void ShowEditorViewPort();
        void ShowGameView();
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
        void AddNode(const std::shared_ptr<SceneNode>& selectedNode);

        void DrawAddComponentMenu(const std::shared_ptr<SceneNode>& node);
        // editorExtras = true adds viewport-only items (Reload Scene, etc.).
        void DrawNodeContextMenu(const std::shared_ptr<SceneNode>& node, bool editorExtras = false);
        void RemoveComponentByName(const std::shared_ptr<SceneNode>& node, const std::string& typeName);
        void DuplicateNode(const std::shared_ptr<SceneNode>& node);
        void ReloadScene();
        void NewScene();
        void CleanupNodeRenderables(const std::shared_ptr<SceneNode>& node);

        // ASSET BROWSER ############################################################
        //TODO: URGENT REFACTORS (forgive me)
        enum class AssetType { Template, Scene, Config, Code, Shader, Image, Audio, Material, Unknown };

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

        // Shared playback toolbar drawn in both viewport and game view.
        // Returns true if any button was pressed.
        bool DrawPlaybackToolbar();

        // Draws yellow wireframe collider outlines as ImGui overlay on the
        // editor viewport (gizmo-style, never touches the render pipeline).
        void DrawColliderGizmos(ImVec2 imgMin, ImVec2 imgSize);

        // BUILD WINDOW
        ConfigurationsWindow configurationsWindow_;
        BuildPanelUI         buildPanel_;

        // SPRITE EDITOR
        SpriteEditor         spriteEditor_;

        // EDITOR OVERLAY TOGGLES
        bool showColliderDebug_  = true;
        bool showGravityDebug_   = true;
        bool showAudioDebug_     = true;

        // GAME VIEW ################################################################
        enum class PlaybackState { Stopped, Playing, Paused };

        struct ResolutionPreset {
            const char* label;
            int         width;
            int         height;
        };

        PlaybackState  playbackState_      = PlaybackState::Stopped;
        bool           stepRequested_      = false;
        int            resolutionIndex_    = 0;
        bool           gameViewShowGrid_   = true;

        std::shared_ptr<FrameBuffer> gameViewFBO_; //TODO: gameViewFBO_SHOULD EXIST THIS IS THE SAME A USING A CAMERA IN THE SCENE...
        static const ResolutionPreset kResolutionPresets[];
        static const int              kNumPresets;
    };
} // namespace ettycc

#endif
