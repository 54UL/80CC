#ifndef ASSET_BUILDER_HPP
#define ASSET_BUILDER_HPP

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "AssetLoader.hpp"
#include <functional>
#include <unordered_map>

namespace ettycc {
class SceneNode;

class AssetBuilder {
public:
    AssetBuilder(std::shared_ptr<AssetLoader> assetLoader);

    std::vector<std::shared_ptr<SceneNode>> BuildFromTemplate(const std::string& templatePath) const;

    // Returns all loaded template objects as a vector of nlohmann::json
    std::vector<nlohmann::json> GetAllLoadedTemplates() const;

    // Registration API
    void RegisterCreator(const std::string& type, std::function<std::shared_ptr<SceneNode>(const nlohmann::json&)> creator);
private:
    std::shared_ptr<AssetLoader> assetLoader_;
    std::unordered_map<std::string, std::function<std::shared_ptr<SceneNode>(const nlohmann::json&)>> creators_;
};
}

#endif //ASSET_BUILDER_HPP