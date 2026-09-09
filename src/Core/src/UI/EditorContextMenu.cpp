#include <UI/EditorContextMenu.hpp>
#include <UI/ComponentRegistry.hpp>
#include <UI/AssetDescriptor.hpp>
#include <Engine.hpp>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace ettycc
{
    void EditorContextMenu::Draw(const ContextMenuState& state)
    {
        // -- Node operations (available when a node is in context) -------
        if (state.targetNode)
            DrawNodeSection(state);

        // -- Asset-specific actions (available when an asset is selected) -
        if (state.source == ContextMenuSource::Assets && state.selectedAsset)
            DrawAssetSection(state);

        // -- Editor extras (viewport-only: Reload Scene, etc.) ----------
        if (state.showEditorExtras)
            DrawEditorSection(state);
    }

    void EditorContextMenu::DrawNodeSection(const ContextMenuState& state)
    {
        if (ImGui::BeginMenu("Add"))
        {
            if (ImGui::MenuItem("Empty Node") && state.onNewNode)
                state.onNewNode();

            ImGui::Separator();
            DrawAddComponentMenu(state);
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Focus") && state.onFocus)
            state.onFocus(state.targetNode);

        bool alreadyFollowing = state.isFollowing ? state.isFollowing() : false;
        if (ImGui::MenuItem("Follow", nullptr, alreadyFollowing) && state.onFollow)
            state.onFollow(state.targetNode);

        ImGui::Separator();

        if (ImGui::MenuItem("Duplicate") && state.onDuplicate)
            state.onDuplicate(state.targetNode);

        if (ImGui::MenuItem("Remove", nullptr, false, !state.isRootNode) && state.onRemove)
            state.onRemove(state.targetNode);
    }

    void EditorContextMenu::DrawAddComponentMenu(const ContextMenuState& state)
    {
        if (!state.targetNode || !state.engine) return;

        const auto& entries = state.engine->componentRegistry_.Entries();
        std::string lastCategory;

        for (const auto& entry : entries)
        {
            if (!lastCategory.empty() && entry.category != lastCategory)
                ImGui::Separator();
            lastCategory = entry.category;

            bool has = entry.hasFn(state.targetNode);
            if (ImGui::MenuItem(entry.displayLabel.c_str(), nullptr, false, !has))
                entry.addFn(state.targetNode, *state.engine);
        }
    }

    void EditorContextMenu::DrawAssetSection(const ContextMenuState& state)
    {
        if (!state.selectedAsset) return;

        ImGui::SeparatorText("Asset");

        // Copy path to clipboard
        if (ImGui::MenuItem("Copy Path"))
            ImGui::SetClipboardText(state.selectedAsset->path.c_str());

        // Reveal in file explorer
        if (ImGui::MenuItem("Reveal in Explorer"))
        {
#ifdef _WIN32
            std::string winPath = state.selectedAsset->path;
            std::replace(winPath.begin(), winPath.end(), '/', '\\');
            std::string param = "/select,\"" + winPath + "\"";
            ShellExecuteA(NULL, "open", "explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
#endif
        }

        // Open with default application
        if (ImGui::MenuItem("Open with Default App"))
        {
#ifdef _WIN32
            std::string winPath = state.selectedAsset->path;
            std::replace(winPath.begin(), winPath.end(), '/', '\\');
            ShellExecuteA(NULL, "open", winPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
        }

        // Type-specific actions via descriptor
        if (state.engine)
        {
            const auto* desc = state.engine->assetDescriptors_.Find(state.selectedAsset->type);
            if (desc && desc->contextMenuFn)
            {
                ImGui::Separator();
                AssetContext ctx;
                ctx.engine       = state.engine;
                ctx.absolutePath = state.selectedAsset->path;
                ctx.selected     = state.selectedAsset;
                desc->contextMenuFn(ctx);
            }
        }
    }

    void EditorContextMenu::DrawEditorSection(const ContextMenuState& state)
    {
        ImGui::Separator();

        if (ImGui::MenuItem("Reload Scene") && state.onReloadScene)
            state.onReloadScene();
    }

} // namespace ettycc
