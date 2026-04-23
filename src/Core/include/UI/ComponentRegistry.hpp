#ifndef COMPONENT_REGISTRY_HPP
#define COMPONENT_REGISTRY_HPP

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace ettycc
{
    class SceneNode;
    class Engine;
    struct EditorPropertyVisitor;

    // ── Badge colors for component categories ────────────────────────────────
    namespace badge
    {
        inline constexpr ImVec4 kRendering = {0.35f, 0.75f, 1.00f, 1.f};  // blue
        inline constexpr ImVec4 kPhysics   = {1.00f, 0.75f, 0.20f, 1.f};  // yellow
        inline constexpr ImVec4 kAudio     = {0.90f, 0.40f, 0.80f, 1.f};  // magenta
        inline constexpr ImVec4 kNetwork   = {1.00f, 0.75f, 0.20f, 1.f};  // yellow
        inline constexpr ImVec4 kModule    = {0.40f, 0.90f, 0.60f, 1.f};  // green
    }

    // ── ComponentEntry ────────────────────────────────────────────────────────
    // Describes a component type for the editor UI: how to add/remove/inspect it,
    // and how to display it in menus and the inspector.
    struct ComponentEntry
    {
        std::string typeName;     // matches T::componentType (e.g. "RigidBody")
        std::string displayLabel; // shown in menus (e.g. "Rigid Body")
        std::string category;     // grouping key (e.g. "Physics", "Audio")
        ImVec4      badgeColor;   // inspector badge tint

        // Returns true if the node already has this component.
        std::function<bool(const std::shared_ptr<SceneNode>&)> hasFn;

        // Adds a default instance of the component to the node.
        std::function<void(const std::shared_ptr<SceneNode>&, Engine&)> addFn;

        // Removes the component from the node.
        std::function<void(const std::shared_ptr<SceneNode>&, Engine&)> removeFn;

        // Draws inspector properties via the visitor pattern.
        std::function<void(const std::shared_ptr<SceneNode>&, EditorPropertyVisitor&)> inspectFn;
    };

    // ── ComponentRegistry ─────────────────────────────────────────────────────
    // A simple singleton list of registered component types.  Both built-in and
    // module components register here so every "Add Component" menu in the editor
    // is automatically populated from the same source.
    //
    // Implementation lives in ComponentRegistry.cpp so that the static instance
    // is unique across the engine and all loaded module DLLs.
    class ComponentRegistry
    {
    public:
        static ComponentRegistry& Instance();

        void Register(ComponentEntry entry);
        void Unregister(const std::string& typeName);

        const std::vector<ComponentEntry>& Entries() const;
        const ComponentEntry* FindByType(const std::string& typeName) const;

        ComponentRegistry() = default;

    private:
        std::vector<ComponentEntry> entries_;
    };

} // namespace ettycc

#endif
