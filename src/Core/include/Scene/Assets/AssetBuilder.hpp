#ifndef ASSET_BUILDER_HPP
#define ASSET_BUILDER_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "AssetLoader.hpp"

namespace ettycc {
class SceneNode;

class AssetBuilder {
public:
    AssetBuilder(std::shared_ptr<AssetLoader> assetLoader);

    std::vector<std::shared_ptr<SceneNode>> BuildFromTemplate(const std::string& templatePath) const;
    void SpawnIntoScene(const std::string& templatePath, std::shared_ptr<SceneNode> parentNode) const;
    std::vector<nlohmann::json> GetAllLoadedTemplates() const;
    void RegisterCreator(const std::string& type, std::function<std::shared_ptr<SceneNode>(const nlohmann::json&)> creator);

private:
    std::shared_ptr<AssetLoader> assetLoader_;
    std::unordered_map<std::string, std::function<std::shared_ptr<SceneNode>(const nlohmann::json&)>> creators_;
    mutable std::unordered_map<std::string, nlohmann::json> loadedTemplates_; // cache: path -> parsed json
};
}

#endif //ASSET_BUILDER_HPP
