#include <Scene/Assets/AssetBuilder.hpp>
#include <fstream>
#include <Scene/SceneNode.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <spdlog/spdlog.h>

namespace ettycc {
    AssetBuilder::AssetBuilder(std::shared_ptr<AssetLoader> assetLoader)
        : assetLoader_(assetLoader) {}

    std::vector<std::shared_ptr<SceneNode>> AssetBuilder::BuildFromTemplate(const std::string& templatePath) {
        std::vector<std::shared_ptr<SceneNode>> nodes;
        std::ifstream ifs(templatePath);
        if (!ifs.is_open()) {
            spdlog::error("Failed to open template file: {}", templatePath);
            return nodes;
        }
        nlohmann::json j;
        try {
            ifs >> j;
        } catch (const std::exception& e) {
            spdlog::error("JSON parse error: {}", e.what());
            return nodes;
        }
        // Example template: { "objects": [ { "type": "sprite", "assetId": 12345, "name": "mySprite", ... } ] }
        if (!j.contains("template-object")) return nodes;
        for (const auto& obj : j["template-object"]) {
            std::string type = obj.value("type", "");
            std::size_t assetId = obj.value("assetId", 0);
            std::string name = obj.value("name", "");
            // TODO: JUST A DRAF ON HOW CREATING OWN PRIMITIVES SHOULD BE IMPLEMENTED....
            if (type == "sprite" && assetId != 0) {
                const Asset* asset = assetLoader_->GetAssetById(assetId);
                if (!asset) {
                    spdlog::warn("Asset id {} not found for sprite {}", assetId, name);
                    continue;
                }
                auto sprite = std::make_shared<Sprite>(asset->name);
                auto node = std::make_shared<SceneNode>(name);
                // Add RenderableNode component, set up sprite, etc.
                // node->AddComponent(std::make_shared<RenderableNode>(sprite));
                nodes.push_back(node);
            }
            // Add more types (camera, sprite (etc))
        }
        return nodes;
    }
}