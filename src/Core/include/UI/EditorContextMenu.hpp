#ifndef EDITOR_CONTEXT_MENU_HPP
#define EDITOR_CONTEXT_MENU_HPP

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace ettycc
{
    class Engine;
    class SceneNode;
    class ComponentRegistry;
    struct AssetDescriptor;
    struct SelectedAsset;
    struct EditorPropertyVisitor;

    // Where the context menu was triggered from
    enum class ContextMenuSource
    {
        Viewport,
        Hierarchy,
        Assets
    };

    // State passed into the context menu for drawing
    struct ContextMenuState
    {
        ContextMenuSource              source;
        Engine*                        engine        = nullptr;
        std::shared_ptr<SceneNode>     targetNode;          // may be null
        const SelectedAsset*           selectedAsset = nullptr;
        bool                           isRootNode    = false;
        bool                           showEditorExtras = false; // reload scene, etc.

        // Callbacks wired by DevEditor
        std::function<void()>                                          onReloadScene;
        std::function<void()>                                          onNewNode;
        std::function<void(const std::shared_ptr<SceneNode>&)>         onDuplicate;
        std::function<void(const std::shared_ptr<SceneNode>&)>         onRemove;
        std::function<void(const std::shared_ptr<SceneNode>&)>         onFollow;
        std::function<void(const std::shared_ptr<SceneNode>&)>         onFocus;
        std::function<bool()>                                          isFollowing; // returns true if already following target
    };

    // -- EditorContextMenu -----------------------------------------------------
    // Centralized context menu that adapts content based on source and selection.
    class EditorContextMenu
    {
    public:
        // Draw the full context menu contents (call between BeginPopup/EndPopup).
        static void Draw(const ContextMenuState& state);

        // Draw just the add-component list (used by inspector popup).
        static void DrawAddComponentMenu(const ContextMenuState& state);

    private:
        // Section: node operations (Add, Duplicate, Remove, Follow, Focus)
        static void DrawNodeSection(const ContextMenuState& state);

        // Section: asset-specific actions
        static void DrawAssetSection(const ContextMenuState& state);

        // Section: editor-wide actions (Reload Scene, etc.)
        static void DrawEditorSection(const ContextMenuState& state);
    };

} // namespace ettycc

#endif
