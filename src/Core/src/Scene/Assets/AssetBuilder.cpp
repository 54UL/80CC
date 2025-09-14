#include <Scene/Assets/AssetBuilder.hpp>
#include <fstream>
#include <Scene/SceneNode.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <spdlog/spdlog.h>

namespace ettycc {
    AssetBuilder::AssetBuilder(std::shared_ptr<AssetLoader> assetLoader)
        : assetLoader_(assetLoader) {
        // Register Sprite creator
        RegisterCreator("Sprite", [this](const nlohmann::json& obj) -> std::shared_ptr<SceneNode> {
            // Extract all required fields from JSON
            std::string name = obj.value("name", "sprite");
            std::string spriteFilePath = obj.value("spriteFilePath", "");
            bool initialize = obj.value("initialize", true);
            std::size_t assetId = obj.value("assetId", 0);
            // Optionally, extract more fields as needed
            // Get asset data if needed
            const Asset* asset = nullptr;
            if (assetId != 0) asset = assetLoader_->GetAssetById(assetId);
            // Create Sprite
            auto sprite = std::make_shared<Sprite>(spriteFilePath, initialize);
            // Optionally, set up more fields on sprite
            auto node = std::make_shared<SceneNode>(name);
            node->AddComponent(std::make_shared<RenderableNode>(sprite));
            return node;
        });
    }

    void AssetBuilder::RegisterCreator(const std::string& type, std::function<std::shared_ptr<SceneNode>(const nlohmann::json&)> creator) {
        creators_[type] = creator;
    }

    std::vector<std::shared_ptr<SceneNode>> AssetBuilder::BuildFromTemplate(const std::string& templatePath) const {
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
        if (!j.contains("template-object")) return nodes;
        for (const auto& obj : j["template-object"]) {
            std::string type = obj.value("type", "");
            auto it = creators_.find(type);
            if (it != creators_.end()) {
                auto node = it->second(obj);
                if (node) nodes.push_back(node);
            } else {
                spdlog::warn("No creator registered for type: {}", type);
            }
        }
        return nodes;
    }
}