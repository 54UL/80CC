#ifndef EDITOR_UI
#define EDITOR_UI

#include <Engine.hpp>
#include <App/ExecutionPipeline.hpp>
#include <UI/Console.hpp>
#include <UI/ImGuiConsoleSink.hpp>
#include <UI/Build/BuildPanelUI.hpp>
#include <UI/ConfigurationsWindow.hpp>
#include <UI/PolygonEditor.hpp>
#include <UI/BoxSelector.hpp>
#include <Scene/Assets/AssetBuilder.hpp>
#include <Graphics/Rendering/PickerBuffer.hpp>
#include <Build/ModuleBuildHelper.hpp>
#include <UI/AssetDescriptor.hpp>
#include <UI/EditorContextMenu.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <stb_image.h>
#include <stack>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <Scene/Components/RenderableNode.hpp>
#include <UI/ComponentRegistry.hpp>

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
        void ShowBuiltInScenes();

        void DrawGravityAttractorGizmos(ImVec2 imgMin, ImVec2 imgSize);
        void DrawAudioGizmos(ImVec2 imgMin, ImVec2 imgSize);
        void DrawCameraFrustumGizmo(ImVec2 imgMin, ImVec2 imgSize);

        void ShowEditorViewPort();
        void ShowGameView();
        void ShowInspector();
        void ShowAssetsView();
        void ShowSceneHierarchy();
        // SCENE HIERARCHY
        std::shared_ptr<Engine> engineInstance_;
        std::vector<std::shared_ptr<SceneNode>> selectedNodes_;
        uint32_t    lastSceneGeneration_ = 0;
        std::string searchFilter_;
        DebugConsole uiConsole;
        bool uiConsoleOpen_;

        void RenderSceneTree();
        void RenderSceneNode(const std::shared_ptr<SceneNode>& rootNode, std::vector<std::shared_ptr<SceneNode>>& selectedNodes, int depth);
        void AddNode(const std::shared_ptr<SceneNode>& selectedNode);

        void RemoveComponentByName(const std::shared_ptr<SceneNode>& node, const std::string& typeName);
        void DuplicateNode(const std::shared_ptr<SceneNode>& node);
        void ReloadScene();
        void NewScene();
        void CleanupNodeRenderables(const std::shared_ptr<SceneNode>& node);

        // Builds a ContextMenuState for the unified context menu system.
        ContextMenuState BuildContextMenuState(ContextMenuSource source,
                                                const std::shared_ptr<SceneNode>& node,
                                                bool editorExtras = false);
        // Opens a context menu popup with the unified system.
        void DrawContextMenu(const char* popupId, ContextMenuSource source,
                             const std::shared_ptr<SceneNode>& node,
                             bool editorExtras = false);

        // ASSET BROWSER ############################################################
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

        void ScanAssets();
        void RenderFolderTree(const std::filesystem::path& path);
        void RenderAssetGrid(const std::string& searchQuery);

        // Asset inspector sections
        void DrawAssetInspectorHeader();
        void DrawAssetInspectorActions();
        void DrawAssetInspectorPreview();

        // Handles all viewport drag-drop via the descriptor registry
        void HandleViewportDragDrop();

        // Computes relative path from absolute (working folder relative)
        std::string ComputeRelativePath(const std::string& absolutePath) const;

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

        // Draws a procedural selection outline around every selected node.
        void DrawSelectionOutlines(ImVec2 imgMin, ImVec2 imgSize);

        // BUILD WINDOW
        ConfigurationsWindow configurationsWindow_;
        BuildPanelUI         buildPanel_;

        // SPRITE EDITOR
        PolygonEditor        polygonEditor_;

        // MODULE REBUILD
        build::ModuleBuildHelper moduleBuildHelper_;

        // CAMERA FOCUS / FOLLOW
        std::weak_ptr<SceneNode> followTarget_;
        bool isFollowing_ = false;

        void FocusCameraOnNode(const std::shared_ptr<SceneNode>& node);
        void StartFollowing(const std::shared_ptr<SceneNode>& node);
        void StopFollowing();

        // SLICE TOOL
        bool  sliceToolActive_ = false;
        bool  sliceDragging_   = false;
        float sliceBreakForce_ = 50.f;
        ImVec2 sliceStartScreen_ = {};
        ImVec2 sliceEndScreen_   = {};

        // VIEWPORT BOX SELECTION
        BoxSelector viewportBoxSelector_;
        void CollectAllNodes(const std::shared_ptr<SceneNode>& node,
                             std::vector<std::shared_ptr<SceneNode>>& out) const;

        // EDITOR OVERLAY TOGGLES (driven by gizmo combo)
        bool showColliderDebug_    = true;
        bool showGravityDebug_     = true;
        bool showAudioDebug_       = true;
        bool showTrajectoryDebug_  = false;
        bool showGridDebug_        = true;
        bool showFrustumDebug_     = false;

        // View states (enabled/disabled)
        bool showStyleEditor_ = false;

        // TRAJECTORY TRAIL GIZMO
        static constexpr int kTrailLength = 64;     // samples per body
        static constexpr int kTrailMaxBodies = 2048; // cap tracked bodies

        struct TrailEntry {
            glm::vec3 positions[64]; // ring buffer (kTrailLength)
            int       head   = 0;    // next write index
            int       count  = 0;    // filled samples (up to kTrailLength)
            float     mass   = 1.f;  // cached for coloring
        };

        std::unordered_map<uint64_t, TrailEntry> trajectoryTrails_;
        float trailSampleTimer_    = 0.f;
        float trailSampleInterval_ = 0.016f; // ~60 samples/s

        void UpdateTrajectories(float dt);
        void DrawTrajectoryGizmos(ImVec2 imgMin, ImVec2 imgSize);

        // BUILT-IN SCENE CONFIG POPUP
        bool               builtInPopupOpen_ = false;
        BuiltInSceneConfig builtInConfig_;
        void ShowBuiltInScenePopup();

        // GAME VIEW ################################################################
        enum class PlaybackState { Stopped, Playing, Paused };

        struct ResolutionPreset {
            const char* label;
            int         width;
            int         height;
        };

        PlaybackState  playbackState_      = PlaybackState::Stopped;
        bool           stepRequested_      = false;
        bool           modulesAutoBuilt_   = false; // true once auto-rebuild was triggered
        int            resolutionIndex_    = 0;
        bool           gameViewShowGrid_   = true;

        // Game-view preview: own FBO + finds (or creates) a scene camera
        std::shared_ptr<FrameBuffer>  gameViewFBO_;
        bool                          gameViewFBOReady_ = false;

        // Locate the first Camera renderable in the scene (not the editor camera).
        std::shared_ptr<Camera> FindSceneCamera() const;

        // Collect all scene cameras (not editor camera), sorted by depth.
        std::vector<std::shared_ptr<Camera>> FindSceneCameras() const;

        static const ResolutionPreset kResolutionPresets[];
        static const int              kNumPresets;
    };
} // namespace ettycc

#endif
