#ifndef ASSET_BUILDER_HPP
#define ASSET_BUILDER_HPP

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "AssetLoader.hpp"

namespace ettycc {
class SceneNode;

class AssetBuilder {
public:
    AssetBuilder(std::shared_ptr<AssetLoader> assetLoader);

    std::vector<std::shared_ptr<SceneNode>> BuildFromTemplate(const std::string& templatePath);

    // Returns all loaded template objects as a vector of nlohmann::json
    std::vector<nlohmann::json> GetAllLoadedTemplates() const;
private:
    std::shared_ptr<AssetLoader> assetLoader_;
};
}

#endif //ASSET_BUILDER_HPP