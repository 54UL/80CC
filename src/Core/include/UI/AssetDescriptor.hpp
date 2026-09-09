#ifndef ASSET_DESCRIPTOR_HPP
#define ASSET_DESCRIPTOR_HPP

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <filesystem>
#include <algorithm>

namespace ettycc
{
    class Engine;
    class SceneNode;

    // -- Asset type identifier --------------------------------------------------
    enum class AssetType
    {
        Template,
        Scene,
        Config,
        Code,
        Shader,
        Image,
        Audio,
        Material,
        Unknown
    };

    // -- Selected asset state ---------------------------------------------------
    struct SelectedAsset
    {
        std::string path;
        std::string name;
        AssetType   type     = AssetType::Unknown;
        uintmax_t   fileSize = 0;
        bool        active   = false;
    };

    // -- Asset entry (scanned file) --------------------------------------------
    struct AssetEntry
    {
        std::string path;
        std::string name;
        AssetType   type = AssetType::Unknown;
    };

    // -- Context passed to asset callbacks --------------------------------------
    struct AssetContext
    {
        Engine*                       engine     = nullptr;
        std::shared_ptr<SceneNode>    targetNode;
        std::string                   absolutePath;
        std::string                   relativePath;
        const SelectedAsset*          selected   = nullptr;
    };

    // -- AssetDescriptor -------------------------------------------------------
    // Declaratively describes how a single asset type behaves in the editor.
    // All callback fields default to null -- only set what you need.
    struct AssetDescriptor
    {
        AssetType   type        = AssetType::Unknown;
        const char* label       = "???";
        const char* displayName = "Unknown";
        ImVec4      color       = {0.35f, 0.35f, 0.35f, 1.f};

        std::vector<std::string> extensions;

        std::function<bool(const std::filesystem::path&)> classifyFn;
        std::function<void(const AssetContext&)>           inspectFn;
        std::function<bool(const AssetContext&)>           viewportDropFn;
        std::function<bool(const AssetContext&)>           hierarchyDropFn;
        std::function<bool(const AssetContext&)>           inspectorDropFn;

        std::string                                        extraPayloadType;
        std::function<std::string(const AssetContext&)>    buildExtraPayloadFn;

        const char* dragTooltip = nullptr;

        std::function<void(const AssetContext&)>           contextMenuFn;

        // Builder-style setters for clean C++17 registration
        AssetDescriptor& Classify(std::function<bool(const std::filesystem::path&)> fn)
        { classifyFn = std::move(fn); return *this; }
        AssetDescriptor& Inspect(std::function<void(const AssetContext&)> fn)
        { inspectFn = std::move(fn); return *this; }
        AssetDescriptor& OnViewportDrop(std::function<bool(const AssetContext&)> fn)
        { viewportDropFn = std::move(fn); return *this; }
        AssetDescriptor& OnHierarchyDrop(std::function<bool(const AssetContext&)> fn)
        { hierarchyDropFn = std::move(fn); return *this; }
        AssetDescriptor& OnInspectorDrop(std::function<bool(const AssetContext&)> fn)
        { inspectorDropFn = std::move(fn); return *this; }
        AssetDescriptor& ExtraPayload(std::string payloadType,
                                       std::function<std::string(const AssetContext&)> buildFn)
        { extraPayloadType = std::move(payloadType); buildExtraPayloadFn = std::move(buildFn); return *this; }
        AssetDescriptor& DragTip(const char* tip)
        { dragTooltip = tip; return *this; }
        AssetDescriptor& ContextMenu(std::function<void(const AssetContext&)> fn)
        { contextMenuFn = std::move(fn); return *this; }

        // Factory
        static AssetDescriptor Create(AssetType t, const char* lbl, const char* name,
                                       ImVec4 col, std::vector<std::string> exts = {})
        {
            AssetDescriptor d;
            d.type        = t;
            d.label       = lbl;
            d.displayName = name;
            d.color       = col;
            d.extensions  = std::move(exts);
            return d;
        }
    };

    // -- AssetDescriptorRegistry -----------------------------------------------
    class AssetDescriptorRegistry
    {
    public:
        void Register(AssetDescriptor desc)
        {
            descriptors_.push_back(std::move(desc));
        }

        AssetType Classify(const std::filesystem::path& p) const
        {
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            for (const auto& d : descriptors_)
                if (d.classifyFn && d.classifyFn(p))
                    return d.type;

            for (const auto& d : descriptors_)
                for (const auto& e : d.extensions)
                    if (ext == e) return d.type;

            return AssetType::Unknown;
        }

        const AssetDescriptor* Find(AssetType type) const
        {
            for (const auto& d : descriptors_)
                if (d.type == type) return &d;
            return nullptr;
        }

        const std::vector<AssetDescriptor>& All() const { return descriptors_; }

    private:
        std::vector<AssetDescriptor> descriptors_;
    };

} // namespace ettycc

#endif
