#pragma once
#include <string>
#include <vector>
#include <functional>
#include <imgui.h>

namespace ettycc
{
    class Engine;

    // Callback types for module-provided editor extensions.
    // All callbacks receive the Engine pointer for scene/registry access.

    // Menu item: drawn inside an ImGui::BeginMenu block.
    // The callback should call ImGui::MenuItem() and handle clicks.
    using EditorMenuCallback = std::function<void(Engine& engine)>;

    // Popup/window: called every frame. The callback should manage its own
    // ImGui::Begin/End or BeginPopup/EndPopup lifecycle.
    using EditorDrawCallback = std::function<void(Engine& engine)>;

    // Viewport gizmo: called during the editor viewport overlay pass.
    // imgMin/imgSize define the viewport rect in screen coordinates.
    using EditorGizmoCallback = std::function<void(Engine& engine, ImVec2 imgMin, ImVec2 imgSize)>;

    struct EditorMenuItem
    {
        std::string       owner;     // module name (for unregistration)
        std::string       label;     // display label (informational)
        EditorMenuCallback callback;
    };

    struct EditorPopup
    {
        std::string       owner;
        std::string       label;
        EditorDrawCallback callback;
    };

    struct EditorGizmo
    {
        std::string         owner;
        std::string         label;
        EditorGizmoCallback callback;
        bool                enabled = true;  // toggle from gizmo combo
    };

    // Registry for module-provided editor extensions.
    // Lives on Engine so modules can register during OnStart and unregister
    // during OnDestroy. DevEditor iterates registered extensions at the
    // appropriate draw points.
    class EditorExtensionRegistry
    {
    public:
        // -- Menu items (drawn in ShowBuiltInScenes or a "Modules" submenu) ----
        void RegisterMenuItem(const std::string& owner, const std::string& label,
                              EditorMenuCallback callback)
        {
            menuItems_.push_back({ owner, label, std::move(callback) });
        }

        // -- Popups/windows (drawn every frame after menu bar) -----------------
        void RegisterPopup(const std::string& owner, const std::string& label,
                           EditorDrawCallback callback)
        {
            popups_.push_back({ owner, label, std::move(callback) });
        }

        // -- Viewport gizmos (drawn in the editor viewport overlay) -----------
        void RegisterGizmo(const std::string& owner, const std::string& label,
                           EditorGizmoCallback callback)
        {
            gizmos_.push_back({ owner, label, std::move(callback), true });
        }

        // -- Unregister all extensions from a given module --------------------
        void UnregisterAll(const std::string& owner)
        {
            auto removeByOwner = [&](auto& vec) {
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                    [&](const auto& item) { return item.owner == owner; }),
                    vec.end());
            };
            removeByOwner(menuItems_);
            removeByOwner(popups_);
            removeByOwner(gizmos_);
        }

        // -- Access (read by DevEditor) ----------------------------------------
        const std::vector<EditorMenuItem>& MenuItems() const { return menuItems_; }
        const std::vector<EditorPopup>&    Popups()    const { return popups_; }
        std::vector<EditorGizmo>&          Gizmos()          { return gizmos_; }
        const std::vector<EditorGizmo>&    Gizmos()    const { return gizmos_; }

    private:
        std::vector<EditorMenuItem> menuItems_;
        std::vector<EditorPopup>    popups_;
        std::vector<EditorGizmo>    gizmos_;
    };

} // namespace ettycc
