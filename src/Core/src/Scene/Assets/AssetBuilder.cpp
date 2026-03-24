#include <Scene/Assets/AssetBuilder.hpp>
#include <Scene/SceneNode.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace ettycc {

    AssetBuilder::AssetBuilder(std::shared_ptr<AssetLoader> assetLoader)
        : assetLoader_(assetLoader)
    {
        // --- Sprite prefab creator ---
        // Expected JSON fields:
        //   "name"           (string)  node name
        //   "spriteFilePath" (string)  path to the image
        //   "transform"      (object)  position/rotation/scale, all optional (default: identity)
        RegisterCreator("Sprite", [](const nlohmann::json& obj) -> std::shared_ptr<SceneNode> {
            std::string name           = obj.value("name", "sprite");
            std::string spriteFilePath = obj.value("spriteFilePath", "");

            auto sprite = std::make_shared<Sprite>(spriteFilePath);

            if (obj.contains("transform")) {
                const auto& t = obj["transform"];
                sprite->underylingTransform.setGlobalPosition({
                    t.value("position_x", 0.f),
                    t.value("position_y", 0.f),
                    t.value("position_z", 0.f)
                });
                sprite->underylingTransform.setGlobalRotation({
                    t.value("rotation_x", 0.f),
                    t.value("rotation_y", 0.f),
                    t.value("rotation_z", 0.f)
                });
                sprite->underylingTransform.setGlobalScale({
                    t.value("scale_x", 1.f),
                    t.value("scale_y", 1.f),
                    t.value("scale_z", 1.f)
                });
            }

            auto node = std::make_shared<SceneNode>(name);
            node->AddComponent(std::make_shared<RenderableNode>(sprite));
            return node;
        });
    }

    void AssetBuilder::RegisterCreator(
        const std::string& type,
        std::function<std::shared_ptr<SceneNode>(const nlohmann::json&)> creator)
    {
        creators_[type] = creator;
    }

    std::vector<std::shared_ptr<SceneNode>> AssetBuilder::BuildFromTemplate(const std::string& templatePath) const
    {
        std::vector<std::shared_ptr<SceneNode>> nodes;

        // Serve from cache if already parsed
        auto cached = loadedTemplates_.find(templatePath);
        nlohmann::json j;

        if (cached != loadedTemplates_.end()) {
            j = cached->second;
        } else {
            std::ifstream ifs(templatePath);
            if (!ifs.is_open()) {
                spdlog::error("[AssetBuilder] Cannot open template: {}", templatePath);
                return nodes;
            }
            try {
                ifs >> j;
                loadedTemplates_[templatePath] = j;
            } catch (const std::exception& e) {
                spdlog::error("[AssetBuilder] JSON parse error in '{}': {}", templatePath, e.what());
                return nodes;
            }
        }

        if (!j.contains("template-object")) {
            spdlog::warn("[AssetBuilder] Missing 'template-object' key in: {}", templatePath);
            return nodes;
        }

        std::string prefabName = j.value("prefab_name", templatePath);

        for (const auto& obj : j["template-object"]) {
            std::string type = obj.value("type", "");
            auto it = creators_.find(type);
            if (it == creators_.end()) {
                spdlog::warn("[AssetBuilder] No creator for type '{}' in prefab '{}'", type, prefabName);
                continue;
            }
            auto node = it->second(obj);
            if (node) {
                spdlog::info("[AssetBuilder] Spawned '{}' (type: {}) from prefab '{}'",
                             node->GetName(), type, prefabName);
                nodes.push_back(node);
            }
        }

        return nodes;
    }

    void AssetBuilder::SpawnIntoScene(
        const std::string& templatePath,
        std::shared_ptr<SceneNode> parentNode) const
    {
        if (!parentNode) {
            spdlog::error("[AssetBuilder] parentNode is null, cannot spawn '{}'", templatePath);
            return;
        }

        auto nodes = BuildFromTemplate(templatePath);
        for (auto& node : nodes) {
            parentNode->AddChild(node);
        }

        spdlog::info("[AssetBuilder] Spawned {} node(s) from '{}' under '{}'",
                     nodes.size(), templatePath, parentNode->GetName());
    }

    std::vector<nlohmann::json> AssetBuilder::GetAllLoadedTemplates() const
    {
        std::vector<nlohmann::json> result;
        result.reserve(loadedTemplates_.size());
        for (const auto& kv : loadedTemplates_) {
            result.push_back(kv.second);
        }
        return result;
    }

} // namespace ettycc
